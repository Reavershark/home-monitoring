FROM python:3.10-alpine

WORKDIR /app

COPY ./requirements.txt /app/requirements.txt
RUN pip install --no-cache-dir -r requirements.txt

COPY ./external_collector.py /app/external_collector.py

CMD ["python", "-u", "external_collector.py"]
