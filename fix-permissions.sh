#!/usr/bin/env bash
set -e

if [ $(pwd) != "/home/pi/home-monitoring" ]; then
    echo "Only run this from the \"home-monitoring\" folder"
    exit 1
fi

sudo chown -R pi:pi .
