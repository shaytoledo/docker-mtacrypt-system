#!/usr/bin/env bash
set -euo pipefail
CLIENTS="${1:-3}"    
COUNT="${2:-10}"    


sudo mkdir -p /mnt/mta && sudo chmod 777 /mnt/mta
sudo rm -f /mnt/mta/server_pipe /mnt/mta/decrypter_pipe_* 2>/dev/null || true
sudo mkfifo /mnt/mta/server_pipe && sudo chmod 666 /mnt/mta/server_pipe
for i in $(seq 1 "$CLIENTS"); do
  sudo mkfifo /mnt/mta/decrypter_pipe_$i
  sudo chmod 666 /mnt/mta/decrypter_pipe_$i
done


for i in $(seq 1 "$CLIENTS"); do
  docker run -d --rm -v /mnt/mta:/mnt/mta -v /var/log:/var/log \
    --name "dec_$i" omerovadia1/mtaproj-decrypter:latest
done
sleep 2


docker run --rm -v /mnt/mta:/mnt/mta -v /var/log:/var/log \
  omerovadia1/mtaproj-rng:latest \
  bash -lc "/usr/local/bin/mtaproj-rng | head -n ${COUNT} > /mnt/mta/server_pipe"


docker ps --filter "name=dec_" --format "table {{.Names}}\t{{.Status}}"
sudo tail -n 30 /var/log/mtacrypt.log || true


docker rm -f $(docker ps --filter "name=dec_" -q) 2>/dev/null || true
echo "Done."
