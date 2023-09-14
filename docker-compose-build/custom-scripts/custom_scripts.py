"""
Reload this script by running the following command in the home-monitoring directory:
$ dc up -d --build custom-scripts
"""

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


    #############################
    # Setup the influxdb client #
    #############################

    client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
    client.ping()
    applog.info(f"Connected to InfluxDB at \"{INFLUXDB_URL}\"")


    ###########################################
    # The Job class and register_job function #
    ###########################################

    @dataclass
    class Job:
        func: Callable[[dict], dict]
        interval: timedelta

    jobs = []
    def register_job(job: Job) -> None:
        jobs.append(job)


    #####################################
    # Helper functions for writing jobs #
    #####################################

    def flux_query(query: str) -> TableList:
        query_api: QueryApi = client.query_api()
        return query_api.query(org=INFLUXDB_ORG, query=query)

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


    ###############
    # Define jobs #
    ###############

    applog.info("Registering jobs...")

    # Downsample fluvius smart meter data
    def downsample_fluvius_smart_meter_data_job(last_state: dict) -> dict:
        joblog = applog.getChild("downsample_fluvius_smart_meter_data_job")
        joblog.info("downsample_fluvius_smart_meter_data_job: entry")

        flux_query("""
            relStartTime = -20m
            windowSize = 2m
            outputBucket = "fluvius_smart_meter_downsampled"

            data =
                from(bucket: "fluvius_smart_meter")
                    |> range(start: relStartTime)

            // Electricity
            data
                |> filter(fn: (r) => r["_measurement"] == "fluvius_smart_meter_electricity")
                |> aggregateWindow(every: windowSize, fn: last, createEmpty: false)
                |> to(bucket: outputBucket)

            // Gas
            data
                |> filter(fn: (r) => r["_measurement"] == "fluvius_smart_meter_gas")
                |> aggregateWindow(every: windowSize, fn: last, createEmpty: false)
                |> to(bucket: outputBucket)
        """)

        joblog.info("downsample_fluvius_smart_meter_data_job: exit")
        # Don't save a state
        return {}
    register_job(Job(
        func=downsample_fluvius_smart_meter_data_job,
        interval=timedelta(minutes=10)
    ))

    applog.info(f"Registered jobs: {jobs}")


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

    # Init state for new jobs that don't have one yet
    def init_missing_job_states(states: dict[str, dict]) -> None:
        for job in jobs:
            if job.func.__name__ not in states:
                states[job.func.__name__] = {}


    #############
    # Main loop #
    #############

    applog.info("Preparing main loop...")
    states = load_states()
    init_missing_job_states(states)
    save_states(states)
    last_executed = [datetime.now() - timedelta(days=365) for job in jobs]

    applog.info("Starting main loop...")
    while True:
        any_ran = False
        for i in range(len(jobs)):
            if last_executed[i] + jobs[i].interval < datetime.now():
                job_name = jobs[i].func.__name__
                last_executed[i] = datetime.now()
                any_ran = True

                applog.info(f"Starting job \"{job_name}\"")
                try:
                    states[job_name] = jobs[i].func(states[job_name].copy())
                    applog.info(f"Job \"{job_name}\" has finished")
                    save_states(states)
                except Exception as e:
                    applog.exception(f"Error occurred in job \"{job_name}\": {e}")

        if not any_ran:
            time.sleep(0.1)


while True:
    try:
        app()
    except Exception as e:
        rootlog.exception(f"Error in app(): {e}")
        rootlog.error("Restarting app...")
        time.sleep(1)
