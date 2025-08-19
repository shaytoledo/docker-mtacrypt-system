#!/usr/bin/env bash
set -euo pipefail
PIPE_DIR="/mnt/mta"
PIPE_PATH="$PIPE_DIR/server_pipe"
sudo mkdir -p "$PIPE_DIR"
sudo chmod 777 "$PIPE_DIR"
if [[ -p "$PIPE_PATH" ]]; then
  echo "FIFO already exists at $PIPE_PATH"
else
  sudo mkfifo "$PIPE_PATH"
  sudo chmod 666 "$PIPE_PATH"
  echo "FIFO created at $PIPE_PATH"
fi
ls -l "$PIPE_PATH"
