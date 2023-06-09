# Home monitoring

## Installation

### Flashing Raspberry Pi OS

On an sd card, flash "Raspberry Pi OS Lite (64-bit)" using the "Raspberry Pi Imager" software.
Make sure to configure the imager first (settings button in the bottom-right corner):
  - Set hostname: `home-monitoring-pi`.
  - Enable ssh, with public-key auth.
    Copy the contents of the `.ssh/id_rsa.pub` file in your user/home folder into the `authorized_keys` field.
    (Install ssh and run `ssh-keygen` (keep hitting enter) if it doesn't exist)
  - Set the `pi` user password.
  - Optionally configure wlan if you can't use an ethernet connection.
  - Set the correct time zone.

Run wireshark on your own computer's network interface with the filter `dhcp.option.dhcp == "Request" && dhcp.option.hostname == "home-monitoring-pi"` to find out the ip adress your pi has received.
Alternatively find your pi's ip address in the info/settings page of your router.
Even better: assign a static ip address for your pi on the dhcp page of your router settings.

### Logging in

Boot up your pi and log into it with `ssh pi@pis_ip_address` (type yes to trust it).
If you get a "Remote host id changed" error, delete your `.ssh/known_hosts` file, or just remove the offending lines.

### Installing dependencies

Install this project's system dependencies and some utilities with:

```sh
sudo apt-get update
sudo apt-get install -y git vim
curl -sSL https://get.docker.com/ | sudo bash -
sudo gpasswd -a pi docker
```

Add some lines to your `.bashrc`:

```sh
echo >> .bashrc
echo 'alias d="docker"' >> .bashrc
echo 'alias dc="docker compose"' >> .bashrc
```

Limit the system journal size by setting `SystemMaxUse=50M` using `sudo nano /etc/systemd/journald.conf`.
Then restart the journal service with `sudo systemctl restart systemd-journald.service`.

Then restart your ssh session (`Ctrl-D` or `exit` and `ssh pi@...` again).

### Setup the project

Clone this repository and enter it:

```sh
git clone git@github.com:Reavershark/home-monitoring.git
cd home-monitoring
git config pull.ff only
```

Fill out the `.env` file (replace at least the password):

```sh
cp .env.example .env
nano .env # Use your preferred editor
```

Fill out the `dnsmasq.conf` file (Set the host:ip mappings, at least the ones for `home-monitoring-pi` and `home-monitoring-pi.local` (same ip)):

```sh
cd docker-compose-resources/dns-server
cp dnsmasq.conf.example dnsmasq.conf
nano dnsmasq.conf # Use your preferred editor
cd -
```

You create a `custom_scripts.py` file from the `custom_scripts.py.example` file.
You don't have to edit anything, this script does nothing by default: 

```sh
cd docker-compose-build/custom_scripts
cp custom_scripts.py.example custom_scripts.py
cd -
``` 

You can update your routers DHCP settings to include the ip of your pi as primary dns server.
**If you do do this, make sure to set `1.1.1.1` or similar as secondary dns server!**
This will allow you to use urls like http://home-monitoring-pi instead of http://192.168.x.x.
Reconnect your computer to the network afterwards.
(Also make sure you have not manually set a dns server in your network settings.)

Setup the docker compose project:

```sh
cd home-monitoring
dc up -d --build
```

This will take a while.
When it is finished, the landing page can be accessed at http://home-monitoring-pi (http://home-monitoring-pi.local for windows).

## Maintaining

Within the `home-monitoring` folder, you can use these commands to manage the docker compose project:
  - `git pull && dc pull`: Download the latest project version.
  - `dc up -d --build`: Create or update the project, starting all services.
  - `dc down --remove-orphans`: Destroy the project (doesn't remove persistent storage, so this is safe to do).
  - `dc ps -a`: Show status of all services.
  - `dc stop [service]`: Stop a service.
  - `dc start [service]`: Start a service.
  - `dc logs -f [service]`: View the logs of a service.
  - `dc exec [service] [bash/sh/...]`: Open a shell in a service.
  - `dc rm [service]`: Destroy a service (doesn't remove persistent storage, so this is safe to do).

Dangerous commands:
  - `dc down --remove-orphans && sudo rm volumes/* -rf`: **Delete all persistent storage. This cannot be undone!**

## External ingress

Messages can be sent either via http or mqtt (on the "data-points" topic).
The messages each contain a data point to be stored in influxdb.
These json messages look like this:

```json
{
    "measurement": "water_depth",
    "tags": {"location": "some_canal"}, // Optional
    "fields": {"depth_in_meters": 1},
    "time": "1500000001000000000", // Optional, generated automatically at the time of sending (nanosecs since unix epoch),
    "bucket": "rivers" // Optional, defaults to "default"
}
```

The "measurement" string and at least one field in "fields" must be set.

See the arduino examples for example implementations.

## Custom scripts

If you want to run custom python scripts that e.g. perform checks using influxdb queries, send alerts through apprise, send commands over mqtt...
You can write *runs* in `docker-compose-build/custom_scripts/custom_scripts.py`.
Copy one of the examples and start from there.
Make sure to register your run function along with a `timeinterval`!

If you want to use the `send_alert` helper function, you first need to specify where you want to receive notifications in apprise.
You can configure the apps to send notifications to on [home-monitoring-pi.local:8000](http://home-monitoring-pi.local:8000).
See [github.com/caronc/apprise/wiki#notification-services](https://github.com/caronc/apprise/wiki#notification-services) for supported services and creating "apprise urls" for them.

Example apprise configuration for a telegram bot, sending notifications to one user:

```yaml
urls:
  - "tgram://0000000000:xxxxxxxxxxxxxxxxxxxxxxxxxx-xxxxxxxx/0000000000": # format: tgram://bot-token/user1/user2/...
      - format: "HTML"
        tag: "telegram, tag2"
```

## Misc

Command to export all resources (dashboards, tasks, notebooks, alerts...):

```sh
docker compose exec influxdb sh -c 'influx export all -o default -t ${DOCKER_INFLUXDB_INIT_ADMIN_TOKEN}' > export.yaml
```
