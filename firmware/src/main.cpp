#include <Arduino.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "config.h"
#include "secrets.h"

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
DHT dht(PIN_DHT, DHT11);

// protege o mqtt (usado por mais de uma task)
SemaphoreHandle_t mqttMutex;

String topic(const char *name) {
  return String("hiot/") + DEVICE_ID + "/" + name;
}

bool publish(const char *name, const String &payload) {
  if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }
  bool ok = mqtt.connected() && mqtt.publish(topic(name).c_str(), payload.c_str());
  xSemaphoreGive(mqttMutex);
  return ok;
}

// comandos do Node-RED: "on" / "off"
void onMessage(char *topicName, byte *payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  bool on = message == "on";

  if (topic("led/temperature") == topicName) {
    digitalWrite(PIN_TEMP_LED, on);
  } else if (topic("led/humidity") == topicName) {
    digitalWrite(PIN_HUMIDITY_LED, on);
  } else if (topic("led/presence") == topicName) {
    digitalWrite(PIN_PRESENCE_LED, on);
  }
  Serial.printf("[mqtt] %s = %s\n", topicName, message.c_str());
}

void connectMqtt() {
  Serial.println("[mqtt] conectando...");
  // last will: broker publica "offline" se o ESP cair
  bool ok = mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASS, topic("status").c_str(), 1, true, "offline");
  if (!ok) {
    Serial.printf("[mqtt] falhou, estado %d\n", mqtt.state());
    return;
  }
  mqtt.publish(topic("status").c_str(), "online", true);
  mqtt.subscribe(topic("led/temperature").c_str(), 1);
  mqtt.subscribe(topic("led/humidity").c_str(), 1);
  mqtt.subscribe(topic("led/presence").c_str(), 1);
  Serial.println("[mqtt] conectado");
}

void mqttTask(void *param) {
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (true) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[wifi] conectando...");
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    bool connected = false;
    if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (!mqtt.connected()) {
        connectMqtt();
      }
      connected = mqtt.connected();
      mqtt.loop();
      xSemaphoreGive(mqttMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(connected ? 10 : 2000));
  }
}

// PIR - core 0
void pirTask(void *param) {
  int lastValue = -1;
  unsigned long lastSent = 0;

  while (true) {
    int value = digitalRead(PIN_PIR);

    if (value != lastValue || millis() - lastSent > PIR_HEARTBEAT_MS) {
      String payload = "{\"v\":" + String(value) + ",\"core\":" + String(xPortGetCoreID()) + "}";
      if (publish("sensor/presence", payload)) {
        Serial.printf("[pir] core %d: %s\n", xPortGetCoreID(), payload.c_str());
        lastValue = value;
        lastSent = millis();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(PIR_INTERVAL_MS));
  }
}

// DHT11 - core 1
void dhtTask(void *param) {
  dht.begin();

  while (true) {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("[dht] erro na leitura");
    } else {
      String payload = "{\"t\":" + String(temperature, 1) + ",\"h\":" + String(humidity, 0) +
                       ",\"core\":" + String(xPortGetCoreID()) + "}";
      publish("sensor/dht", payload);
      Serial.printf("[dht] core %d: %s\n", xPortGetCoreID(), payload.c_str());
    }

    vTaskDelay(pdMS_TO_TICKS(DHT_INTERVAL_MS));
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_TEMP_LED, OUTPUT);
  pinMode(PIN_HUMIDITY_LED, OUTPUT);
  pinMode(PIN_PRESENCE_LED, OUTPUT);

  mqttMutex = xSemaphoreCreateMutex();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setKeepAlive(15);
  // timeout curto pro watchdog não resetar no connect
  mqtt.setSocketTimeout(2);

  xTaskCreatePinnedToCore(mqttTask, "mqttTask", 8192, NULL, 1, NULL, MQTT_CORE);
  xTaskCreatePinnedToCore(pirTask, "pirTask", 4096, NULL, 2, NULL, PIR_CORE);
  xTaskCreatePinnedToCore(dhtTask, "dhtTask", 4096, NULL, 2, NULL, DHT_CORE);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
