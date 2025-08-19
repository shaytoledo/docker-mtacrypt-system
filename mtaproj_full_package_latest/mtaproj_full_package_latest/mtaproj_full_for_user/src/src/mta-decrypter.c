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

/* ===== constants kept identical ===== */
#define PIPE_DIR       "/mnt/mta/"
#define ENCRYPTER_PIPE "/mnt/mta/server_pipe"
#define LOG_FILE       "/var/log/mtacrypt.log"

#define MAX_MSG        1024
#define MAX_PIPE_NAME  256

static int  g_id = 1;
static FILE* g_log = NULL;

/* ===== helpers preserved for identical output ===== */
static long ts_now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}

static void print_str(FILE* out, const char* buf, unsigned int len) {
    for (unsigned int i = 0; i < len; ++i)
        fprintf(out, "%c", isprint((unsigned char)buf[i]) ? buf[i] : '.');
}

static int is_printable_str(const char* buf, unsigned int len) {
    for (unsigned int i = 0; i < len; ++i)
        if (!isprint((unsigned char)buf[i]))
            return 0;
    return 1;
}

static void log_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    fflush(g_log);
    va_end(ap);
}

/* identical policy: first unused id in 1..32 by checking pipe existence */
static int next_available_id(void) {
    for (int id = 1; id <= 32; ++id) {
        char p[MAX_PIPE_NAME];
        snprintf(p, sizeof(p), "%sdecrypter_pipe_%d", PIPE_DIR, id);
        if (access(p, F_OK) != 0) return id;
    }
    return 1;
}

int main(void) {
    g_log = fopen(LOG_FILE, "w");
    if (!g_log) { perror("Failed to open log file"); exit(EXIT_FAILURE); }

    if (MTA_crypt_init() != MTA_CRYPT_RET_OK) {
        log_printf("[CLIENT] Failed to initialize crypto library!\n");
        exit(EXIT_FAILURE);
    }

    g_id = next_available_id();

    char name[MAX_PIPE_NAME], path[MAX_PIPE_NAME * 2];
    snprintf(name, sizeof(name), "decrypter_pipe_%d", g_id);
    snprintf(path, sizeof(path), "%s%s", PIPE_DIR, name);

    if (mkfifo(path, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo pipe_path");
        exit(EXIT_FAILURE);
    }

    /* identical registration loop (non-blocking write to server pipe) */
    for (;;) {
        int reg_fd = open(ENCRYPTER_PIPE, O_WRONLY | O_NONBLOCK);
        if (reg_fd >= 0) {
            char msg[MAX_PIPE_NAME + 16];
            snprintf(msg, sizeof(msg), "SUBSCRIBE:%s\n", name);
            if (write(reg_fd, msg, strlen(msg)) > 0) {
                close(reg_fd);
                log_printf("%ld  [CLIENT #%d]  [INFO] Sent connect request to server\n",
                           ts_now_sec(), g_id);
                break;
            }
            close(reg_fd);
        }
        usleep(100000);
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        log_printf("%ld  [CLIENT #%d]  [ERROR] Failed to open %s for reading: %s\n",
                   ts_now_sec(), g_id, path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    char enc[MAX_MSG];
    unsigned int pwd_len = 0, key_len = 0;
    unsigned long iters = 0;
    int have_pwd = 0, first_pwd = 1;

    for (;;) {
        if (!have_pwd) {
            ssize_t n = read(fd, enc, sizeof(enc));
            if (n <= 0) { usleep(100000); continue; }
            pwd_len = (unsigned)n;
            key_len = pwd_len / 8;
            iters   = 0;
            have_pwd = 1;

            if (first_pwd) {
                log_printf("%ld  [CLIENT #%d]  [INFO] Received encrypted password %.*s\n",
                           ts_now_sec(), g_id, pwd_len, enc);
                first_pwd = 0;
            } else {
                log_printf("%ld  [CLIENT #%d]  [INFO] Received new encrypted password %.*s\n",
                           ts_now_sec(), g_id, pwd_len, enc);
            }
        }

        /* identical brute-force structure (random keys, check printable) */
        while (have_pwd) {
            ++iters;
            char* guess = (char*)malloc(key_len);
            char* plain = (char*)malloc(pwd_len);
            unsigned int plain_len = 0;

            MTA_get_rand_data(guess, key_len);

            if (MTA_decrypt(guess, key_len, enc, pwd_len, plain, &plain_len) == MTA_CRYPT_RET_OK) {
                if (plain_len == pwd_len && is_printable_str(plain, plain_len)) {
                    log_printf("%ld  [CLIENT #%d]  [INFO] Decrypted password: ",
                               ts_now_sec(), g_id);
                    print_str(g_log, plain, plain_len);
                    log_printf(", Key: ");
                    print_str(g_log, guess, key_len);
                    log_printf(" (in %lu iterations)\n", iters);

                    /* identical SOLUTION message back to server */
                    int sfd = open(ENCRYPTER_PIPE, O_WRONLY | O_NONBLOCK);
                    if (sfd >= 0) {
                        char header[64];
                        int hlen = snprintf(header, sizeof(header), "SOLUTION:%d:", g_id);

                        /* build SOLUTION line exactly as original */
                        char out[MAX_MSG + 64];
                        memcpy(out, header, hlen);
                        memcpy(out + hlen, plain, plain_len);
                        int len = hlen + (int)plain_len;
                        out[len++] = '\n';
                        write(sfd, out, len);
                        close(sfd);
                    }
                    have_pwd = 0;
                }
            }

            free(guess);
            free(plain);

            /* identical poll for new password every 1000 iters */
            if (iters % 1000 == 0) {
                char newer[MAX_MSG];
                ssize_t nn = read(fd, newer, sizeof(newer));
                if (nn > 0 && ((unsigned)nn != pwd_len || memcmp(newer, enc, pwd_len) != 0)) {
                    memcpy(enc, newer, nn);
                    pwd_len = (unsigned)nn;
                    key_len = pwd_len / 8;
                    iters   = 0;
                    log_printf("%ld  [CLIENT #%d]  [INFO] Received new encrypted password %.*s\n",
                               ts_now_sec(), g_id, pwd_len, enc);
                    break;
                }
            }
        }
    }

    close(fd);
    fclose(g_log);
    return 0;
}
