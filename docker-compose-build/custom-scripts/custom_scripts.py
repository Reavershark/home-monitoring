from typing import Callable

import os, shutil, json, time, logging
from dataclasses import dataclass
from datetime import datetime, timedelta

import influxdb_client
from influxdb_client import InfluxDBClient
from influxdb_client.client.query_api import QueryApi
from influxdb_client.client.flux_table import TableList, FluxTable, FluxRecord

import requests

logging.basicConfig(format='%(asctime)s: %(levelname)s: %(name)s: %(message)s')
rootlog = logging.root
rootlog.setLevel(logging.INFO)

def app():
    applog = rootlog.getChild("app")

    #############################
    # Get environment variables #
    #############################

    try:
        INFLUXDB_URL = os.environ["INFLUXDB_URL"]
        INFLUXDB_TOKEN = os.environ["INFLUXDB_TOKEN"]
        INFLUXDB_ORG = os.environ["INFLUXDB_ORG"]
        MQTT_BROKER_ADDRESS = os.environ["MQTT_BROKER_ADDRESS"]
        MQTT_BROKER_PORT = os.environ["MQTT_BROKER_PORT"]
        APPRISE_URL = os.environ["APPRISE_URL"]
    except KeyError as e:
        raise Exception(f"Missing environment variable: {e}")


    #########################
    # Setup influxdb client #
    #########################

    client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
    client.ping()
    applog.info(f"Connected to InfluxDB at \"{INFLUXDB_URL}\"")


    #######################################
    # Run class and register_run function #
    #######################################
    
    @dataclass
    class Run:
        func: Callable[[dict], dict]
        interval: timedelta
        
    runs = []
    def register_run(run: Run) -> None:
        runs.append(run)
    

    ########################
    # Run helper functions #
    ########################

    def send_alert(title: str, body: str, tags: list[str] = ["all"]) -> None:
        apprise_msg = {
            "title": f"<b>{title}</b>",
            "body": "",
            "tag": ", ".join(tags)
        }
        try:
            requests.post(url=APPRISE_URL, data=apprise_msg)
        except Exception as e:
            raise Exception(f"Failed to send a notification: {e}")

    
    #################
    # Run functions #
    #################

    applog.info("Registering runs functions...")

    
    # Example run description
    def example_run(last_state: dict) -> dict:
        runlog = applog.getChild("example_run")
        runlog.info("Example run")

        times_called = last_state.get("times_called", 0) + 1
        runlog.info(f"Times called: {times_called}")

        return {
            "times_called": times_called
        }
    
    #register_run(Run(
    #    func=example_run,
    #    interval=timedelta(seconds=15)
    #))
    
    
    # Example run 2 description
    def example_run2(last_state: dict) -> dict:
        runlog = applog.getChild("example_run2")
        runlog.info("Example run 2")

        query_api: QueryApi = client.query_api()

        query = """
            from(bucket: "disk")
                |> range(start: -1h)
                |> filter(fn: (r) => r._measurement == "disk" and r._field == "used_percent")
        """

        tables: TableList = query_api.query(query)

        # Print the last record
        assert len(tables) == 1, f"Expected one table, but got {len(tables)}"
        assert len(tables[0].records) >= 1, f"The table does not contain any records"
        record = tables[0].records[-1]
        runlog.info(f"repr(record): {repr(record)}")
        runlog.info(f"str(record): {str(record)}")

        # Don't save a state
        return {}
    
    #register_run(Run(
    #    func=example_run2,
    #    interval=timedelta(minutes=1)
    #))

    
    applog.info(f"Registered runs: {runs}")


    ##############################
    # Main loop helper functions #
    ##############################
    
    # load states from json file
    def load_states() -> dict[str, dict]:
        if not os.path.isdir("data"):
            os.mkdir("data")
        if os.path.isfile("data/states.json"):
            with open("data/states.json", "r") as f:
                return json.load(f)
        else:
            return {}
            
    # Save states to json file
    def save_states(states: dict[str, dict]) -> None:
        
        if os.path.isfile("data/states.json"):
            shutil.copyfile("data/states.json", "data/states.json.backup-of-previous")
        with open("data/states.json", "w") as f:
            json.dump(states, f)

            
    # Init state for functions that don't have one
    def init_missing_run_states(states: dict[str, dict]) -> None:
        for run in runs:
            if run.func.__name__ not in states:
                states[run.func.__name__] = {}


    #############
    # Main loop #
    #############

    applog.info("Preparing main loop...")
    states = load_states()
    init_missing_run_states(states)
    save_states(states)
    last_executed = [datetime.now() - timedelta(days=365) for run in runs]

    applog.info("Starting main loop...")
    while True:
        any_ran = False
        for i in range(len(runs)):
            if last_executed[i] + runs[i].interval < datetime.now():
                any_ran = True

                run_name = runs[i].func.__name__
                last_executed[i] = datetime.now()

                applog.info(f"Starting run \"{run_name}\"")
                try:
                    states[run_name] = runs[i].func(states[run_name].copy())
                    applog.info(f"Run \"{run_name}\" has finished")
                    save_states(states)
                except Exception as e:
                    applog.exception(f"Error in run \"{run_name}\": {e}")

        if not any_ran:
            time.sleep(0.1)


while True:
    try:
        app()
    except Exception as e:
        rootlog.exception(f"Error in app(): {e}")
        rootlog.error("Restarting app...")
        time.sleep(1)
