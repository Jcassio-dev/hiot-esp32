#!/bin/sh
# Uso: scripts/add-mqtt-user.sh <usuario> [senha]
# Cria ou atualiza um usuário no broker e recarrega o Mosquitto.
# Usuários novos só acessam o que estiver liberado para eles em mosquitto/config/acl.
set -eu
cd "$(dirname "$0")/.."
user=${1:?"uso: $0 <usuario> [senha]"}
if [ $# -ge 2 ]; then
  pass=$2
else
  printf 'Senha para %s: ' "$user"
  stty -echo; read -r pass; stty echo; echo
fi
docker compose exec -T mosquitto sh -c '
  mosquitto_passwd -b /mosquitto/data/passwd "$1" "$2" &&
  chown mosquitto:mosquitto /mosquitto/data/passwd &&
  chmod 0700 /mosquitto/data/passwd' _ "$user" "$pass"
docker compose kill -s HUP mosquitto >/dev/null
echo "Usuário $user salvo e broker recarregado."
