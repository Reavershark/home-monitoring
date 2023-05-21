from typing import Any

import os, time, json, logging, requests

from queue import Queue
from threading import Thread

from flask import Flask, request, Response
from http import HTTPStatus

################################
# alert_queue_processor thread #
################################

def alert_queue_processor_thread_entrypoint(alert_queue: Queue) -> None:
    log = logging.getLogger("alert_queue_processor_thread")
    log.setLevel(logging.INFO)

    log.info(" Thread started")

    while True:
        try:
            try:
                APPRISE_URL: str = os.environ["APPRISE_URL"]
            except KeyError as e:
                raise Exception(f"Missing environment variable: {str(e)}")

            while True:
                influxdb_alert_str: str = alert_queue.get() # waits when empty
                log.info(f" Received alert from influxdb: {influxdb_alert_str}")

                # Parse influxdb_alert_str as json
                try:
                    influxdb_alert: dict[str, Any] = json.loads(influxdb_alert_str)
                except Exception:
                    log.error(f"Invalid json alert: \"{alert_str}\"")
                    continue

                # Check _version field
                if influxdb_alert["_version"] != 1:
                    log.error(f"Unsupported influxdb alert json version: {influxdb_alert['_version']}")
                    continue

                # Remove some fields from the alert dict
                modified_influxdb_alert = influxdb_alert.copy()
                fields_to_remove = [
                    "_check_id", "_measurement", "_notification_endpoint_id",
                    "_notification_endpoint_name","_notification_rule_id",
                    "_version"
                ]
                for field in fields_to_remove:
                    modified_influxdb_alert.pop(field)

                # Construct the apprise notification
                al = modified_influxdb_alert
                apprise_msg = {
                    "title": f"<b>Alert for check: \"{al['_check_name']}\"</b>",
                    "body": "",
                    "tag": "all"
                }
                apprise_msg["body"] += f"Status: <u><b>{al['_level']}</b></u>\n"
                apprise_msg["body"] += f"Check type: <b>{al['_type']}</b>\n"
                if al["_type"] == "deadman":
                    apprise_msg["body"] += f"Dead: <b>{al['dead']}</b>\n"
                    al.pop("dead")
                    apprise_msg["body"] += f"Last data point returned by check query:\n"
                else:
                    apprise_msg["body"] += f"Data point (or result of a function of many) returned by the check query:\n"
                if "_time" in al:
                    apprise_msg["body"] += f"  - Time: <b>{al['_time']}</b>\n"
                for key, val in al.items():
                    if len(key) > 0 and key[0] != "_":
                        apprise_msg["body"] += f"  - {key}: <b>{val}</b>\n"
                if "_message" in al:
                    apprise_msg["body"] += "Custom message:\n"
                    apprise_msg["body"] += f"<i>{al['_message']}</i>\n"

                # Send the notification to apprise
                log.info(f" Sending notification to apprise: {apprise_msg}")
                try:
                    requests.post(url=APPRISE_URL, data=apprise_msg)
                except Exception as e:
                    log.error(f"Failed to send a notification: {e}")
                    continue

                alert_queue.task_done()
        except Exception as e:
            log.error(f"Exception in alert_queue_processor_thread: {e}")
            wait_secs = 1
            log.error(f"Restarting alert_queue_processor_thread in {wait_secs} {'sec' if wait_secs == 1 else 'secs'}")
            time.sleep(wait_secs)

########################
# http_listener thread #
########################

def http_listener_thread_entrypoint(alert_queue: Queue) -> None:
    log = logging.getLogger("http_listener_thread")
    log.setLevel(logging.INFO)

    log.info(" Thread started")

    app = Flask(__name__)

    # Disable logging each request
    logging.getLogger("werkzeug").setLevel(logging.ERROR)

    @app.route("/", methods=['POST'])
    def new_alert() -> Response:
        alert_queue.put(request.data)
        return Response(status=HTTPStatus.NO_CONTENT)

    log.info(" Listening...")
    app.run(host='0.0.0.0', port='80')

#########
# setup #
#########

logging.basicConfig()

alert_queue = Queue()

alert_queue_processor_thread = Thread(
    name="alert_queue_processor_thread",
    target=alert_queue_processor_thread_entrypoint,
    args=(alert_queue,),
    daemon=True
)
alert_queue_processor_thread.start()

# Reuse main thread for http listener
http_listener_thread_entrypoint(alert_queue)
