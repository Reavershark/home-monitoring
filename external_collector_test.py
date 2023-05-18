import requests, random

example = {
  "measurement": "water_depth",
  "tags": {"location": "some_canal"},
  "fields": {"depth_in_meters": random.uniform(1, 2)},
  "bucket": "default"
}

res = requests.post(
  url="http://localhost:8080/",
  json=example
)

print(res)
