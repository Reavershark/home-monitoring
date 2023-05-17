FROM python:3.10-alpine

WORKDIR /app

COPY ./requirements.txt /app/requirements.txt
RUN pip install --no-cache-dir -r requirements.txt

COPY ./http_collector.py /app/http_collector.py

CMD ["python", "http_collector.py"]
