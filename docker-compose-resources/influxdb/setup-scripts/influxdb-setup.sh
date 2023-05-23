#!/bin/sh
set -e

cd /docker-entrypoint-initdb.d

echo "Creating buckets..."
influx bucket create -n disk -r 8w
echo "Buckets created"

echo "Creating dashboards..."
for file in $(ls dashboards); do
    echo "Creating dashboard \"${file}\""
    influx apply -f "dashboards/${file}" --force yes
done
echo "Dashboards created"
