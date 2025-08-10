#!/usr/bin/env bash
set -euo pipefail
PIPE_PATH="/tmp/mtaproj/server_pipe"
if [[ -p "$PIPE_PATH" ]]; then
  echo "Removing FIFO $PIPE_PATH"
  sudo rm -f "$PIPE_PATH"
else
  echo "FIFO not found at $PIPE_PATH"
fi
