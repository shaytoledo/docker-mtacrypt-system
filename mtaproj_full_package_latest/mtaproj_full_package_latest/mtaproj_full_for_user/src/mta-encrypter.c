#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include "mta_crypt.h"
#include "mta_rand.h"

#define ENCRYPTER_PIPE "/mnt/mta/server_pipe"
#define PIPE_DIR       "/mnt/mta/"
#define CONF_FILE      "/mnt/mta/mtacrypt.conf"
#define LOG_FILE       "/var/log/mtacrypt.log"

#define MAX_DECRYPTERS 32
#define MAX_MSG        1024
#define MAX_PIPE_NAME  512

typedef struct {
    char pipe_name[MAX_PIPE_NAME];
    int  id;
    int  active;
} decrypter_t;

static decrypter_t g_dec[MAX_DECRYPTERS];
static int         g_dec_count = 0;
static unsigned int g_pwd_len  = 24;
static FILE*       g_log = NULL;

static long ts_now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}

static void print_str(FILE* out, const char* buf, unsigned int len) {
    for (unsigned int i = 0; i < len; ++i) {
        fprintf(out, "%c", isprint((unsigned char)buf[i]) ? buf[i] : '.');
    }
}

static void log_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    fflush(g_log);
    va_end(ap);
}

static void gen_printable(char* buf, unsigned int len) {
    for (unsigned int i = 0; i < len; ++i) {
        char c;
        do { c = MTA_get_rand_char(); } while (!isprint((unsigned char)c));
        buf[i] = c;
    }
}

static void read_config(void) {
    log_printf("Reading /mnt/mta/mtacrypt.conf...\n");
    FILE* f = fopen(CONF_FILE, "r");
    if (!f) {
        log_printf("[SERVER][ERROR] Could not open config file %s: %s\n",
                   CONF_FILE, strerror(errno));
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "PASSWORD_LENGTH=", 16) == 0) {
            g_pwd_len = (unsigned)atoi(line + 16);
            log_printf("Password length set to %u\n", g_pwd_len);
        }
    }
    fclose(f);
}

static int register_decrypter(const char* pipe_name) {
    for (int i = 0; i < g_dec_count; ++i) {
        if (strcmp(g_dec[i].pipe_name, pipe_name) == 0) {
            return g_dec[i].id;
        }
    }
    if (g_dec_count >= MAX_DECRYPTERS) return -1;

    int id = g_dec_count + 1;
    snprintf(g_dec[g_dec_count].pipe_name, sizeof(g_dec[g_dec_count].pipe_name), "%s", pipe_name);
    g_dec[g_dec_count].id     = id;
    g_dec[g_dec_count].active = 1;

    log_printf("%ld  [SERVER]  [INFO] Received connection request from decrypter id %d, fifo name %s%s\n",
               ts_now_sec(), id, PIPE_DIR, pipe_name);

    g_dec_count++;
    return id;
}

static void send_to_decrypter_idx(int idx, const char* enc, unsigned int enc_len) {
    char full[1024];
    snprintf(full, sizeof(full), "%s%s", PIPE_DIR, g_dec[idx].pipe_name);

    int fd = open(full, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        log_printf("%ld  [SERVER]  [ERROR] Failed to open %s for writing: %s\n",
                   ts_now_sec(), full, strerror(errno));
        return;
    }
    ssize_t w = write(fd, enc, enc_len);
    if (w < 0) {
        log_printf("%ld  [SERVER]  [ERROR] Failed to write to %s: %s\n",
                   ts_now_sec(), full, strerror(errno));
    }
    close(fd);
}

static void broadcast_password(const char* enc, unsigned int enc_len) {
    for (int i = 0; i < g_dec_count; ++i) {
        if (g_dec[i].active) send_to_decrypter_idx(i, enc, enc_len);
    }
}

int main(void) {
    g_log = fopen(LOG_FILE, "w");
    if (!g_log) { perror("Failed to open log file"); exit(EXIT_FAILURE); }

    read_config();

    if (MTA_crypt_init() != MTA_CRYPT_RET_OK) {
        log_printf("[SERVER] Failed to initialize crypto library!\n");
        exit(EXIT_FAILURE);
    }

    umask(0);
    unlink(ENCRYPTER_PIPE);
    if (mkfifo(ENCRYPTER_PIPE, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
    chmod(ENCRYPTER_PIPE, 0666);

    int reg_fd = open(ENCRYPTER_PIPE, O_RDONLY | O_NONBLOCK);
    if (reg_fd < 0) { perror("open server_pipe"); exit(EXIT_FAILURE); }

    char *pwd = NULL, *enc = NULL, *key = NULL;
    unsigned int enc_len = 0, key_len = 0;
    int first_pwd = 1;

    for (;;) {
        if (!pwd) {
            key_len = g_pwd_len / 8;
            pwd = (char*)malloc(g_pwd_len);
            key = (char*)malloc(key_len);
            enc = (char*)malloc(g_pwd_len);

            gen_printable(pwd, g_pwd_len);
            MTA_get_rand_data(key, key_len);

            if (MTA_encrypt(key, key_len, pwd, g_pwd_len, enc, &enc_len) != MTA_CRYPT_RET_OK) {
                log_printf("%ld  [SERVER]  [ERROR] Encryption failed\n", ts_now_sec());
                free(pwd); free(key); free(enc);
                pwd = enc = key = NULL;
                goto poll_input;
            }

            if (first_pwd) {
                log_printf("%ld  [SERVER]  [INFO] New password generated: ", ts_now_sec());
                print_str(g_log, pwd, g_pwd_len);
                log_printf(", key: ");
                print_str(g_log, key, key_len);
                fprintf(g_log, ", After encryption: %.*s", enc_len, enc);
                log_printf("\nListening on /mnt/mta/server_pipe\n");
                first_pwd = 0;
            } else {
                log_printf("%ld  [SERVER]  [INFO] New password: ", ts_now_sec());
                print_str(g_log, pwd, g_pwd_len);
                log_printf(", key: ");
                print_str(g_log, key, key_len);
                fprintf(g_log, ", Encrypted: %.*s", enc_len, enc);
                log_printf("\n");
            }
            broadcast_password(enc, enc_len);
        }

poll_input: ;
        char buf[2048];
        ssize_t n = read(reg_fd, buf, sizeof(buf) - 1);
        if (n == 0) {
            close(reg_fd);
            reg_fd = open(ENCRYPTER_PIPE, O_RDONLY | O_NONBLOCK);
        } else if (n > 0) {
            buf[n] = '\0';
            char *nl = strchr(buf, '\n');
            if (nl) *nl = '\0';

            if (strncmp(buf, "SUBSCRIBE:", 10) == 0) {
                const char* p = buf + 10;
                int id = register_decrypter(p);
                if (id > 0 && enc) {
                    send_to_decrypter_idx(id - 1, enc, enc_len);
                }
            } else if (strncmp(buf, "SOLUTION:", 9) == 0) {
                char* p = buf + 9;
                char* colon = strchr(p, ':');
                if (colon) {
                    int dec_id = atoi(p);
                    char* solution = colon + 1;
                    if (pwd && strlen(solution) == g_pwd_len &&
                        memcmp(solution, pwd, g_pwd_len) == 0) {
                        log_printf("%ld  [SERVER]  [OK] Password decrypted successfully by decrypter #%d\n",
                                   ts_now_sec(), dec_id);
                        free(pwd); free(enc); free(key);
                        pwd = enc = key = NULL;
                    }
                }
            }
        }

        usleep(100000);
    }

    fclose(g_log);
    return 0;
}
