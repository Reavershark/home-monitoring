FROM python:3.10-alpine

WORKDIR /app

COPY ./requirements.txt /app/requirements.txt
RUN pip install --no-cache-dir -r requirements.txt

COPY ./custom_scripts.py /app/custom_scripts.py

CMD ["python", "-u", "custom_scripts.py"]
