import requests

# Post form data
requests.post(
    url="http://home-monitoring-pi:8000/notify/apprise",
    data={
        "title": "Optional title",
        "body": "Hello world!",
        "tag": "all"
    }
)
