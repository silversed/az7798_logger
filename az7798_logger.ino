#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "az7798_logger_settings.h"



void wifi_reconnect() {
  DEBUG_STREAM.print("reconnect to WiFi: ");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG_STREAM.print(".");
  }
  DEBUG_STREAM.println("done");
  DEBUG_STREAM.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
}


String get_sensor_data() {
  for (int i = 1; i <= SENSOR_REQ_LIMIT; i++) {
    DEBUG_STREAM.printf("sensor request (cmd=\":\") (try=%d)\n", i);
    SENSOR_STREAM.print(":\n");

    SENSOR_STREAM.setTimeout(SENSOR_REQ_TIMEOUT_MS);
    String str = SENSOR_STREAM.readStringUntil('\n');
    DEBUG_STREAM.printf("sensor response: %s\n", str.c_str());
    if (str.length())
      return str;
  }
  DEBUG_STREAM.printf("no data from sensor after %d attempts\n", SENSOR_REQ_LIMIT);
  return "";
}


String process_sensor_data(const String str) {
  float t, h;
  int co2;
  String out;
  if (sscanf(str.c_str(), ": T%fC:C%dppm:H%f", &t, &co2, &h) == 3) {
    out = String(t, 1) + " " + String(co2) + " " + String(h, 1);
  }
  DEBUG_STREAM.printf("after process raw data: %s\n", out.c_str());
  return out;
}


void send_sensor_data(const String str) {
  WiFiClient client;
  HTTPClient http;

  http.setTimeout(HTTP_TIMEOUT_MS);
  int rc = http.begin(client, SERVER_URL);
  DEBUG_STREAM.printf("[HTTP] url=%s connect: %d\n", SERVER_URL, rc);
  if (rc) {
    rc = http.POST(str);
    DEBUG_STREAM.printf("[HTTP] POST... code: %d (%s)\n", rc, http.errorToString(rc).c_str());

    if (rc > 0) {
      if (rc == HTTP_CODE_OK || rc == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = http.getString();
        DEBUG_STREAM.printf("[HTTP] POST... response:\n%s\n", payload.c_str());
      }
    }
    http.end();
  }
}




void setup() {
  SENSOR_STREAM.begin(SENSOR_STREAM_BAUD);
  SENSOR_STREAM.swap();
  DEBUG_STREAM.begin(DEBUG_STREAM_BAUD);
  delay(1000);

  wifi_reconnect();
}


void loop() {
  unsigned long time_start_ms = millis();

  String raw = get_sensor_data();
  if (raw.length()) {
    String str = process_sensor_data(raw);
    if (str.length()) {
      send_sensor_data(str);
    }
  }
  DEBUG_STREAM.printf("took time: %lu ms\n", millis() - time_start_ms);

  DEBUG_STREAM.println("sleep...\n");
  while (millis() - time_start_ms < LOOP_PERIOD_MS) {
    delay(100);
  }
}
