import os, time, json, logging, requests

from queue import Queue
from threading import Thread

from flask import Flask, request, Response
from http import HTTPStatus

################################
# alert_queue_processor thread #
################################

def alert_queue_processor_thread_entrypoint(alert_queue: Queue):
    # Disable logging each message
    # logging.setLevel(logging.WARNING)

    while True:
        try:
            try:
                APPRISE_URL = os.environ["APPRISE_URL"]
            except KeyError as e:
                raise Exception(f"Missing environment variable: {str(e)}")

            while True:
                alert_str = alert_queue.get() # waits when empty
                logging.info("alert_queue_processor_thread alert:", alert_str)

                # Parse alert as json
                try:
                    alert = json.loads(alert_str)
                except Exception:
                    logging.error(f"Invalid json alert \"{alert_str}\":")
                    continue

                try:
                    requests.post(
                        url=APPRISE_URL,
                        data={
                            "title": "Alert",
                            "body": json.dumps(alert, indent=2),
                            "tag": "all"
                        }
                    )
                except Exception as e:
                    logging.error(f"Failed to send a notification:", e)
                    continue

                logging.info("Alert dict:", alert)

                alert_queue.task_done()
        except Exception as e:
            logging.error("Exception in alert_queue_processor_thread:", e)
            wait_secs = 1
            logging.error(f"Restarting alert_queue_processor_thread in {wait_secs} {'sec' if wait_secs == 1 else 'secs'}")
            time.sleep(wait_secs)

########################
# http_listener thread #
########################

def http_listener_thread_entrypoint(alert_queue: Queue):
    app = Flask(__name__)

    # Disable logging each request
    logging.getLogger('werkzeug').setLevel(logging.ERROR)

    @app.route("/", methods=['POST'])
    def new_alert():
        alert_queue.put(request.data)
        return Response(status=HTTPStatus.NO_CONTENT)

    app.run(host='0.0.0.0', port='80')

#########
# setup #
#########

alert_queue = Queue()

alert_queue_processor_thread = Thread(
    name="alert_queue_processor_thread",
    target=alert_queue_processor_thread_entrypoint,
    args=(alert_queue,),
    daemon=True
)
alert_queue_processor_thread.start()

http_listener_thread_entrypoint(alert_queue)
