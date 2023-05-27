// Install dependencies with:
//   - Set additional board manager urls in settings to:
//       https://arduino.esp8266.com/stable/package_esp8266com_index.json
//       https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
//   - Install esp8266 boards through the board manager (contains a good WiFiClient.h)
//   - Install ArduinoJson through the library manager

#include "settings.h" // Create by copying settings.h.example to settings.h and filling in the dummy values

#include "wifi_http_client.h"
#include "util.h"
#include <ArduinoJson.h>

WifiHttpClient client(
    settings.wifi_ssid, settings.wifi_pass,
    settings.http_server_address, settings.http_server_port,
    settings.use_serial
);

void setup()
{
  if (settings.use_serial) Serial.begin(9600);
  client.first_connect();
}

void loop()
{
  client.reconnect_if_needed();
  
  auto read_and_send_data = [&]() {
    int value = random(1, 10);
    
    String json_string;
    {
      // Create json object to send
      // Use https://arduinojson.org/v6/assistant to get the recommended static document size
      // Example json: See readme file
      StaticJsonDocument<192> json; // Gets destroyed when leaving this scope
      
      json["measurement"] = "water_depth";
      json["tags"]["location"] = "some_canal";
      json["fields"]["depth_in_meters"] = value;
      json["bucket"] = "default";
      
      serializeJson(json, json_string);
    }
    
    client.send_post(json_string);
  };

  // Call read_and_send_data() every send_interval_msecs
  {
    uint32_t state = 0;
    run_in_interval_nonblocking(&state, settings.send_interval_msecs, read_and_send_data);
  }
}
