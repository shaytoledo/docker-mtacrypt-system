#!/usr/bin/env bash
set -euo pipefail

PIPE_PATH="/mnt/mta/server_pipe"
COUNT=""
if [[ "${1:-}" == "--count" ]]; then
  COUNT="${2:-}"
  [[ -n "$COUNT" ]] || { echo "Missing value for --count"; exit 1; }
fi


if [[ ! -p "$PIPE_PATH" ]]; then
  echo "FIFO not found at $PIPE_PATH; creating..."
  "$(dirname "$0")/setup_fifo.sh"
fi
if [[ ! -f "/mnt/mta/mtacrypt.conf" ]]; then
  echo "Config not found; creating default /mnt/mta/mtacrypt.conf ..."
  sudo bash -c 'cat > /mnt/mta/mtacrypt.conf <<CONF
PIPE_PATH=/mnt/mta/server_pipe
LOG_PATH=/var/log/mtacrypt.log
CONF'
  sudo chmod 666 /mnt/mta/mtacrypt.conf
fi

cleanup() {
  echo; echo "Stopping pipeline..."
  [[ -n "${RNG_CID:-}" ]] && docker rm -f "$RNG_CID" >/dev/null 2>&1 || true
  [[ -n "${DEC_RC:-}" ]] && true
}
trap cleanup INT TERM


if [[ -n "$COUNT" ]]; then
  RNG_CID=$(docker run -d --rm \
    -v /mnt/mta:/mnt/mta -v /var/log:/var/log \
    omerovadia1/mtaproj-rng:latest \
    bash -lc "/usr/local/bin/mtaproj-rng | head -n $COUNT > /mnt/mta/server_pipe")
else
  RNG_CID=$(docker run -d --rm \
    -v /mnt/mta:/mnt/mta -v /var/log:/var/log \
    omerovadia1/mtaproj-rng:latest \
    bash -lc "/usr/local/bin/mtaproj-rng > /mnt/mta/server_pipe")
fi
echo "RNG started (cid=$RNG_CID), writing into $PIPE_PATH"


echo "Starting Decrypter..."
set +e
timeout 8 docker run --rm \
  -v /mnt/mta:/mnt/mta -v /var/log:/var/log \
  omerovadia1/mtaproj-decrypter:latest
DEC_RC=$?
set -e


docker ps --filter "id=$RNG_CID"
echo "Pipeline finished. Decrypter exit code: $DEC_RC"
