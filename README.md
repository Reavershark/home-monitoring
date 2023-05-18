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

Then restart your ssh session (`Ctrl-D` or `exit` and `ssh pi@...` again).

### Setup the project

Clone this repository and enter it:

```sh
git clone git@github.com:Reavershark/home-monitoring.git
cd home-monitoring
```

Fill out the `.env` file (replace at least the password):

```sh
cp .env.example .env
nano .env # Use your preferred editor
```

Setup the docker compose project:

```sh
cd home-monitoring
dc up -d --build
```

This will take a while.
When finished, the influxdb interface can be accessed at http://pi-ip-address:8086.

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

## Misc

Command to export all resources (dashboards, tasks, notebooks, alerts...):

```sh
docker compose exec influxdb sh -c 'influx export all -o default -t ${DOCKER_INFLUXDB_INIT_ADMIN_TOKEN}' > export.yaml
```
