import os, time, json, logging

from queue import Queue
from threading import Thread

from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS

import paho.mqtt.client as mqtt

from flask import Flask, request, Response
from http import HTTPStatus

##################################
# message_queue_processor thread #
##################################

def message_queue_processor_thread_entrypoint(message_queue: Queue):
    while True:
        try:
            try:
                INFLUX_URL = os.environ["INFLUX_URL"]
                INFLUX_TOKEN = os.environ["INFLUX_TOKEN"]
                INFLUX_ORG = os.environ["INFLUX_ORG"]
            except KeyError as e:
                raise Exception(f"Missing environment variable: {str(e)}")

            client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG)

            write_api = client.write_api(write_options=SYNCHRONOUS)

            while True:
                try:
                    msg_str, unix_time = message_queue.get() # waits when empty
                    logging.info(f"message_queue_processor_thread message: {msg_str}")

                    # Parse message as json
                    try:
                        msg = json.loads(msg_str)
                    except Exception:
                        logging.warning(f"Invalid json message \"{msg_str}\":")
                        continue

                    # Validate the message json structure
                    # Example:
                    # {
                    #     "measurement": "water_depth",
                    #     "tags": {"location": "some_canal"}, // Optional
                    #     "fields": {"depth_in_meters": 1},
                    #     "time": 100000.123, // Optional, generated automatically (secs since unix epoch),
                    #     "bucket": "rivers" // Optional, defaults to "default"
                    # }
                    try:
                        assert type(msg) == dict, "The message is not a json object"

                        assert "measurement" in msg, "The field \"measurement\" is missing"
                        assert type(msg["measurement"]) == str, "The field \"measurement\" is not a string"

                        if "tags" in msg:
                            assert type(msg["tags"]) == dict, "The provided optional field \"tags\" is not an object"
                            for key, value in msg["tags"].items():
                                assert type(key) == str and type(value) == str, f"The provided object \"tags\" has an entry with invalid type(s): \"{key}:{value}\", should both be strings"

                        assert "fields" in msg, "The field \"fields\" is missing"
                        assert type(msg["fields"]) == dict, "The field \"fields\" is not an object"
                        for key, value in msg["fields"].items():
                            assert type(key) == str, f"The object \"fields\" has a non-string key: \"{key}\""

                        if "time" in msg:
                            assert type(msg["time"]) == str, "The provided optional field \"time\" is not a nanosecond unix timestamp string"

                        if "bucket" in msg:
                            assert type(msg["bucket"]) == str, "The provided optional field \"buckket\" is not a string"
                    except AssertError as e:
                        logging.warning(f"Validation failed for message \"{msg_str}\": {str(e)}")
                        continue

                    # Set the data point time
                    if "time" in msg:
                        # Reassign unix_time
                        unix_time = int(msg["time"])
                    msg["time"] = unix_time

                    # Create and the data point
                    data_point = Point.from_dict(msg, WritePrecision.NS)

                    # Write the data point to a bucket
                    write_api.write(
                        bucket=msg["bucket"] if "bucket" in msg else "default",
                        org=INFLUX_ORG,
                        record=data_point
                    )
                finally:
                    message_queue.task_done()
        except Exception as e:
            logging.error(type(e))
            logging.error(f"Exception in message_queue_processor_thread: {str(e)}")
            wait_secs = 1
            logging.error(f"Restarting message_queue_processor_thread in {wait_secs} {'sec' if wait_secs == 1 else 'secs'}")
            time.sleep(wait_secs)

##########################
# mqtt_subscriber thread #
##########################

def mqtt_subscriber_thread_entrypoint(message_queue: Queue):
    while True:
        try:
            try:
                MQTT_BROKER_ADDRESS = os.environ["MQTT_BROKER_ADDRESS"]
                MQTT_BROKER_PORT = os.environ["MQTT_BROKER_PORT"]
            except KeyError as e:
                raise Exception(f"Missing environment variable: {str(e)}")
            try:
                MQTT_BROKER_PORT = int(MQTT_BROKER_PORT)
            except ValueError as e:
                raise Exception(f"The MQTT_BROKER_PORT environment variable is invalid: {MQTT_BROKER_PORT}")

            def on_connect(client, userdata, flags, rc):
                logging.info(f"Connected to \"mqtt://{MQTT_BROKER_ADDRESS}:{MQTT_BROKER_PORT}\" with result code \"{str(rc)}\"")
                client.subscribe("data-points")

            def on_message(client, userdata, msg):
                if msg.topic == "data-points":
                    unix_time = int(time.time() * 1_000_000_000) # utc nanosecond unix timestamp
                    message_queue.put((msg.payload, unix_time))

            client = mqtt.Client()
            client.on_connect = on_connect
            client.on_message = on_message

            client.connect(host=MQTT_BROKER_ADDRESS, port=MQTT_BROKER_PORT)

            client.loop_forever()
        except Exception as e:
            logging.error(f"Exception in mqtt_subscriber_thread: {str(e)}")
            wait_secs = 1
            logging.error(f"Restarting mqtt_subscriber_thread in {wait_secs} {'sec' if wait_secs == 1 else 'secs'}")
            time.sleep(wait_secs)

########################
# http_listener thread #
########################

def http_listener_thread_entrypoint(message_queue: Queue):
    app = Flask(__name__)

    # Disable logging each request
    logging.getLogger('werkzeug').setLevel(logging.ERROR)

    @app.route("/", methods=['POST'])
    def new_message():
        unix_time = int(time.time() * 1_000_000_000) # utc nanosecond unix timestamp
        message_queue.put((request.data, unix_time))
        return Response(status=HTTPStatus.NO_CONTENT)

    app.run(host='0.0.0.0', port='8080')

#########
# setup #
#########

# Set to WARNING to disable printing each message
logging.root.setLevel(logging.INFO)

message_queue = Queue()

message_queue_processor_thread = Thread(
    name="message_queue_processor_thread",
    target=message_queue_processor_thread_entrypoint,
    args=(message_queue,),
    daemon=True
)
message_queue_processor_thread.start()

mqtt_subscriber_thread = Thread(
    name="mqtt_subscriber_thread",
    target=mqtt_subscriber_thread_entrypoint,
    args=(message_queue,),
    daemon=True
)
mqtt_subscriber_thread.start()

# Reuse main thread for http listener
http_listener_thread_entrypoint(message_queue)
