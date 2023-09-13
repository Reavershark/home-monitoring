// Install dependencies with:
//   - Set additional board manager urls in settings to:
//       https://arduino.esp8266.com/stable/package_esp8266com_index.json
//       https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
//   - Install esp8266 boards through the board manager (contains a good WiFiClient.h for arduinos)
//   - Install ArduinoJson through the library manager

///        ///
// Includes //
///        ///

#include <ArduinoJson.h>

#include "wifi_http_client.h"
#include "util.h"

#include "settings.h" // Create by copying settings.h.example to settings.h and filling in the dummy values

///       ///
// Globals //
///       ///

WifiHttpClient client(
    settings.wifi_ssid, settings.wifi_pass,
    settings.http_server_address, settings.http_server_port,
    settings.use_serial);

///                     ///
// Function declarations //
///                     ///

void setup();
void loop();
void read_and_send_data();

///                    ///
// Function definitions //
///                    ///

void setup()
{
  if (settings.use_serial)
    Serial.begin(115200);
  client.first_connect();
}

void loop()
{
  client.reconnect_if_needed();

  // Call read_and_send_data() every send_interval_msecs
  {
    static uint32_t state = 0;
    run_in_interval_nonblocking(&state, settings.send_interval_msecs, read_and_send_data);
  }
}

void read_and_send_data()
{
  float value = (float)random(1, 10);
  if (settings.use_serial)
  {
    Serial.print("Value: ");
    Serial.println(value);
  }

  String json_string;
  {
    // Create json object to send
    // Use https://arduinojson.org/v6/assistant to get the recommended static document size
    // Example json: See project readme file
    StaticJsonDocument<192> json; // Gets destroyed when leaving this scope

    json["bucket"] = "default";
    json["measurement"] = "water_depth";
    json["tags"]["location"] = "some_canal";
    json["fields"]["depth_in_meters"] = value;

    serializeJson(json, json_string);
  }

  client.send_post("/", json_string);
};
