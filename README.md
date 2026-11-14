# HIoT: monitoramento ambiental com ESP32, MQTT e Node-RED

Projeto da disciplina IMD0904, Plataforma de Hardware para IoT (UFRN/IMD).

Um ESP32 lê um sensor de presença PIR (HC-SR501) e um DHT11 (temperatura e umidade). Cada sensor tem a sua própria task do FreeRTOS, presa a um núcleo diferente, e publica as leituras num broker MQTT (Mosquitto) na nuvem. O Node-RED lê essas leituras, compara com os limites e manda de volta o comando para acender ou apagar os LEDs do ESP32.

```
PIR   ─► pirTask (core 0) ─┐                                ┌─► led/temperature ─► LED de temperatura
                           ├─► Mosquitto ◄─► Node-RED ──────┼─► led/humidity    ─► LED de umidade
DHT11 ─► dhtTask (core 1) ─┘                    │           └─► led/presence    ─► LED de presença
                                                └─► Dashboard
```

## Requisitos da atividade

1. Cada sensor é lido e enviado por uma task em um núcleo diferente (`xTaskCreatePinnedToCore`).
2. As leituras vão para o broker MQTT.
3. O Node-RED lê os dados do broker.
4. Quando um limite é atingido, o Node-RED manda um comando e o ESP32 acende um LED.

## Limites padrão

| Alerta | LED | Liga quando | Desliga quando |
|---|---|---|---|
| Temperatura alta | temperatura | > 25 °C | ≤ 24,5 °C |
| Temperatura baixa | temperatura | < 20 °C | ≥ 20,5 °C |
| Umidade alta | umidade | > 50 % | ≤ 48 % |
| Presença | presença | PIR = 1 | 10 s sem movimento |

A diferença entre "liga" e "desliga" é a **histerese**: sem ela, um valor parado bem no limite (25,0 → 25,1 → 25,0) faria o LED ligar e desligar sem parar. Todos os limites podem ser mudados na página **Config** do dashboard.

## Ligações

Placa: ESP32-S3-WROOM (N16R8), com duas portas USB-C. Só uma fileira vai na protoboard. Da antena para o USB-C:

```
3V3, EN, 4, 5, 6, 7, 15, 16, 17, 18, 8, 3, 46, 9, 10, 11, 12, 13, 14, 5V
```

Essa fileira não tem GND, então um jumper macho-fêmea leva o GND da outra fileira até o trilho de GND da protoboard.

| Componente | Pino | ESP32 |
|---|---|---|
| HC-SR501 | VCC | 5V (o módulo precisa de 4,5 V a 20 V) |
| HC-SR501 | OUT | GPIO 17 (a saída é de 3,3 V) |
| DHT11 | VCC | 3V3 |
| DHT11 | DATA | GPIO 3 |
| LED de temperatura | perna longa | GPIO 8, com resistor de 220 Ω |
| LED de presença | perna longa | GPIO 18, com resistor de 220 Ω |
| LED de umidade | perna longa | GPIO 15, com resistor de 220 Ω |
| Todos | GND / perna curta | GND |

Não ligue o 5V no trilho de 3,3 V. Evite os pinos 0, 45 e 46 (mexem no boot), 35 a 37 (usados pela PSRAM) e 19 e 20 (USB). Não desconecte o DHT11 com a placa ligada: sem ele, a biblioteca trava e o ESP32 reinicia.

## Como funciona

### Firmware (`firmware/src/main.cpp`)

Três tasks:

| Task | Núcleo | O que faz |
|---|---|---|
| `pirTask` | 0 | lê o PIR a cada 100 ms e publica quando muda (e a cada 10 s) |
| `dhtTask` | 1 | lê o DHT11 a cada 2 s e publica |
| `mqttTask` | 0 | mantém o Wi-Fi e o MQTT conectados e recebe os comandos dos LEDs |

Cada mensagem leva o núcleo em que foi publicada (`xPortGetCoreID()`), e o dashboard mostra isso.

O `PubSubClient` não pode ser usado por duas tasks ao mesmo tempo, então todo acesso a ele passa por um mutex (`mqttMutex`).

Os LEDs só mudam quando chega uma mensagem do Node-RED (`on` ou `off`). O ESP32 nunca acende um LED sozinho.

### Node-RED (`infra/node-red/flows/flows.json`)

| Função | O que faz |
|---|---|
| **Verifica temperatura e umidade** | compara com os limites (com histerese), atualiza o dashboard e manda `on`/`off` para os LEDs de temperatura e de umidade quando o estado de cada um muda |
| **Verifica presença** | manda `on` para o LED de presença assim que há movimento e `off` depois de X segundos sem movimento |
| **Carregar limites** | coloca os valores padrão ao iniciar (ou no botão "Restaurar padrões") e atualiza os sliders |
| **Salvar limite** | guarda o valor de cada slider |

Os limites ficam salvos no contexto `file` do Node-RED, então sobrevivem a um reinício.

### Tópicos MQTT

Base: `hiot/esp32-01/`

| Tópico | Quem publica | Exemplo |
|---|---|---|
| `status` | ESP32 (retido) | `online`, ou `offline` publicado pelo broker (Last Will) se o ESP32 cair |
| `sensor/presence` | ESP32 | `{"v":1,"core":0}` |
| `sensor/dht` | ESP32 | `{"t":24.1,"h":55,"core":1}` |
| `led/temperature` | Node-RED (retido) | `on` ou `off` |
| `led/humidity` | Node-RED (retido) | `on` ou `off` |
| `led/presence` | Node-RED (retido) | `on` ou `off` |

## 1. Subir o broker e o Node-RED na VM

Pré-requisito: Docker com `docker compose`.

```sh
git clone https://github.com/Jcassio-dev/hiot-esp32.git hiot && cd hiot/infra
cp .env.example .env
```

No `.env`:

1. Troque as senhas `MQTT_ESP32_PASS` e `MQTT_NODERED_PASS`.
2. Gere o hash da senha do editor com `scripts/hash-node-red-password.sh` e cole em `NODE_RED_ADMIN_HASH`, **entre aspas simples**.
3. Coloque um texto aleatório em `NODE_RED_CREDENTIAL_SECRET` (`openssl rand -hex 32`).

```sh
docker compose up -d --build
docker compose ps        # os dois serviços devem ficar "healthy"
```

- Dashboard: `http://<ip-da-vm>:1880/dashboard`
- Editor: `http://<ip-da-vm>:1880`

O broker recusa conexões sem usuário. O usuário `esp32` só pode publicar o status e os sensores e só pode ler os LEDs; o `nodered` pode tudo em `hiot/#` (veja `infra/mosquitto/config/acl`).

MQTT na 1883 e Node-RED em HTTP mandam senhas sem criptografia. Se puder, libere essas portas no firewall só para os IPs que você usa.

### Com domínio e proxy reverso

Se a VM já tem um proxy reverso (Caddy, Nginx) numa rede Docker, crie um `infra/docker-compose.override.yml` (fica fora do git) para tirar a porta 1880 e colocar o Node-RED nessa rede:

```yaml
services:
  node-red:
    ports: !reset []
    networks:
      default:
      proxy:
        aliases: [hiot-node-red]

networks:
  proxy:
    external: true
```

No Caddy: `nodered.seudominio.com { reverse_proxy hiot-node-red:1880 }`. Na Cloudflare, o domínio do Node-RED pode ficar com proxy (nuvem laranja); o do MQTT tem que ficar **só DNS** (nuvem cinza), porque o proxy gratuito não repassa a porta 1883.

## 2. Gravar o firmware

Pré-requisito: [PlatformIO](https://platformio.org/install/cli).

```sh
cd firmware
cp include/secrets.example.h include/secrets.h   # preencha Wi-Fi, broker e senha
pio run -t upload
pio device monitor -b 115200
```

Grave pela porta USB-C marcada **UART** (ou "COM"). O ESP32 só conecta em Wi-Fi de 2,4 GHz. No monitor serial aparecem as leituras com o núcleo de cada task e os comandos dos LEDs.

## Roteiro da demo

1. Monitor serial: `[pir] core 0` e `[dht] core 1`.
2. Dashboard, bloco **ESP32**: status online, PIR no core 0 e DHT11 no core 1.
3. Passe a mão na frente do PIR: o LED de presença acende. Saia da frente: ele apaga depois do tempo configurado.
4. Segure o DHT11 entre os dedos: a temperatura sobe, aparece "Temperatura alta" e o LED de temperatura acende. Bafeje no sensor: a umidade sobe e o LED de umidade acende.
5. Em **Config**, mude o limite de temperatura alta para logo acima da temperatura atual: o LED de temperatura apaga na próxima leitura, sem regravar a placa.
6. Tire o ESP32 da USB: em até ~25 s o status no dashboard muda para `offline` (Last Will).
