#!/bin/sh
# cria os usuários esp32 e nodered a partir do .env
set -eu
umask 077
pw=/mosquitto/data/passwd
touch "$pw"
mosquitto_passwd -b "$pw" esp32 "$MQTT_ESP32_PASS"
mosquitto_passwd -b "$pw" nodered "$MQTT_NODERED_PASS"
chown mosquitto:mosquitto "$pw"
chmod 0700 "$pw"
exec /usr/sbin/mosquitto -c /mosquitto/config/mosquitto.conf
