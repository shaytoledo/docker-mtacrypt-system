# Exercise 3 – Linux Course

## Authors:
- Omer Ovadia – 211603865
- Gal Jacoby – 322659350
- Shay Toledo – 314654484

---

## Project Overview
This project implements a password encryption and decryption pipeline using Docker containers and FIFO (named pipes) for inter-process communication.
It consists of two main components:
- **RNG (Encrypter)** – Generates encrypted passwords and writes them to a shared FIFO.
- **Decrypter** – Reads from the FIFO and attempts to decrypt the passwords.

---

## Requirements
- **Linux environment**
- **Docker** installed and running
- Shared libraries:
  - `libmta_crypt.so`
  - `libmta_rand.so`

---

## How To Run 
# 1) Create the FIFO
sudo bash ./scripts/setup_fifo.sh

# 2) Run the pipeline (example: 10 values)
sudo bash ./scripts/start_pipeline.sh --count 10

# 3) View logs
sudo tail -f /var/log/mtacrypt.log
