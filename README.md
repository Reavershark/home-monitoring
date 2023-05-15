Command to export all resources (dashboards, tasks, notebooks, alerts...):

```sh
docker compose exec influxdb sh -c 'influx export all -o default -t ${DOCKER_INFLUXDB_INIT_ADMIN_TOKEN}' > export.yaml
```
