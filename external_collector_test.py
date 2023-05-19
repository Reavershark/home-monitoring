import requests, random, time

while True:
    try:
        example = {
          "measurement": "water_depth",
          "tags": {"location": "some_canal"},
          "fields": {"depth_in_meters": random.uniform(1, 2)},
          "bucket": "default",
          #"time": str(int(time.time() * 1_000_000_000))
        }
        res = requests.post(
          url="http://home-monitoring-pi:8080/",
          json=example
        )
        print(res)
        time.sleep(1)
    except Exception as e:
        print(e)
        print("retrying in 10s")
        time.sleep(10)
