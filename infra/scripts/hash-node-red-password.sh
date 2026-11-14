#!/bin/sh
# Uso: scripts/hash-node-red-password.sh
# Pede a senha do editor do Node-RED e imprime o hash bcrypt para o NODE_RED_ADMIN_HASH do .env.
set -eu
cd "$(dirname "$0")/.."
printf 'Senha do editor do Node-RED: '
stty -echo; read -r pass; stty echo; echo
docker run --rm -e PW="$pass" --entrypoint node nodered/node-red:4.1.17-22 \
  -e "console.log(require('/usr/src/node-red/node_modules/bcryptjs').hashSync(process.env.PW, 8))"
