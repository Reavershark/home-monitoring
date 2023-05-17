import os, time

from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS

INFLUX_URL = os.environ["INFLUX_URL"]
INFLUX_TOKEN = os.environ["INFLUX_TOKEN"]
INFLUX_ORG = os.environ["INFLUX_ORG"]

time.sleep(3)

client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG)

write_api = client.write_api(write_options=SYNCHRONOUS)

bucket = "default"


for value in range(5):
  message = {"val": value}

  dict_structure = {
      "measurement": "h2o_feet",
      "tags": {"location": "coyote_creek"},
      "fields": message
  }

  point = Point.from_dict(dict_structure, WritePrecision.NS)
  write_api.write(bucket=bucket, org=INFLUX_ORG, record=point)
  time.sleep(1) # separate points by 1 second

while True:
    time.sleep(100)
