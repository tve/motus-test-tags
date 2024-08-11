#! /bin/bash -e
# Runs build.sh in all subdirectories

for b in */build.sh; do
    echo ""
    echo "===== $b ====="
    (cd $(dirname $b); ./build.sh)
done
