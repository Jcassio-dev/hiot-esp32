#pragma once

// pinos (ESP32-S3)
#define PIN_PIR 17
#define PIN_DHT 3
#define PIN_TEMP_LED 8
#define PIN_PRESENCE_LED 18
#define PIN_HUMIDITY_LED 15

// núcleos
#define PIR_CORE 0
#define DHT_CORE 1
#define MQTT_CORE 0

#define DHT_INTERVAL_MS 2000
#define PIR_INTERVAL_MS 100
#define PIR_HEARTBEAT_MS 10000  // reenvia mesmo sem mudança
