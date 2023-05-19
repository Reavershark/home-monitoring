FROM python:3.10-alpine

WORKDIR /app

COPY ./requirements.txt /app/requirements.txt
RUN pip install --no-cache-dir -r requirements.txt

COPY ./influxdb_alert_handler.py /app/influxdb_alert_handler.py

CMD ["python", "-u", "influxdb_alert_handler.py"]
