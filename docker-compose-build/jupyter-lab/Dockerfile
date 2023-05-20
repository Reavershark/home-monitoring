FROM python:3.10-alpine

RUN apk add --no-cache bash gcc musl-dev linux-headers cargo
RUN pip install jupyterlab jupyter_collaboration

# Disable "Would you like to receive official Jupyter news?" message
RUN jupyter labextension disable "@jupyterlab/apputils-extension:announcements"

CMD jupyter-lab \
        --collaborative \
        --ip="0.0.0.0" \
        --port=9000 \
        --notebook-dir="/hostos/home/pi/" \
        --LabApp.token='' \
        --LabApp.password='' \
        --ServerApp.allow_root=true \
        --ServerApp.allow_remote_access=true \
