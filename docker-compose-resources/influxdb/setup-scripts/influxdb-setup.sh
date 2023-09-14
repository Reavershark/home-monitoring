#!/bin/sh
set -e

cd /docker-entrypoint-initdb.d

echo "Creating buckets..."
influx bucket create -r 8w  -n disk
influx bucket create -r 7d  -n heartbeat
influx bucket create -r 12h -n fluvius_smart_meter
influx bucket create -r 90d -n fluvius_smart_meter_downsampled
echo "Buckets created"

echo "Creating dashboards..."
for file in $(ls dashboards); do
    echo "Creating dashboard \"${file}\""
    influx apply -f "dashboards/${file}" --force yes
done
echo "Dashboards created"
