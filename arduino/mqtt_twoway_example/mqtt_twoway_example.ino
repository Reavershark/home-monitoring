// Install dependencies with:
//   - Set additional board manager urls in settings to:
//       https://arduino.esp8266.com/stable/package_esp8266com_index.json
//       https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
//   - Install esp8266 boards through the board manager (contains a good WiFiClient.h)
//   - Install ArduinoMqttClient through the library manager
//   - Install ArduinoJson through the library manager

///        ///
// Includes //
///        ///

#include <ArduinoJson.h>

#include "wifi_mqtt_client.h"
#include "util.h"

#include "settings.h" // Create by copying settings.h.example to settings.h and filling in the dummy values


///       ///
// Globals //
///       ///

WifiMqttClient client(
    settings.wifi_ssid, settings.wifi_pass,
    settings.mqtt_broker_address, settings.mqtt_broker_port,
    settings.use_serial,
    settings.incoming_message_size_limit
);

const String topic("data-points");


///                     ///
// Function declarations //
///                     ///

void setup();
void loop();
void read_and_publish_data();
void on_mqtt_message(String &topic, String &message);


///                    ///
// Function definitions //
///                    ///

void setup()
{
  if (settings.use_serial) Serial.begin(9600);
  client.first_connect();
  client.subscribe(topic);
  client.set_on_message(on_mqtt_message);
}

void loop()
{
  client.reconnect_if_needed();

  // Calls on_mqtt_message for every new message, if any
  client.process_incoming_messages();

  // Call read_and_publish_data() every publish_interval_msecs
  {
    static uint32_t state = 0;
    run_in_interval_nonblocking(&state, settings.publish_interval_msecs, read_and_publish_data);
  }
}

void read_and_publish_data()
{
  int32_t value = random(1, 10);
  
  String json_string;
  {
    // Create json object to publish
    // Use https://arduinojson.org/v6/assistant to get the recommended static document size
    // Example json: See readme file
    StaticJsonDocument<192> json; // Gets destroyed when leaving this scope
    
    json["measurement"] = "water_depth";
    json["tags"]["location"] = "some_canal";
    json["fields"]["depth_in_meters"] = value;
    json["bucket"] = "default";
    
    serializeJson(json, json_string);
  }
  
  client.publish(topic, json_string);
}

void on_mqtt_message(String &topic, String &message)
{
  DynamicJsonDocument json(settings.incoming_message_size_limit); // Gets destroyed when leaving this scope

  DeserializationError result = deserializeJson(json, message);
  if (result != DeserializationError::Ok)
  {
    if (settings.use_serial) 
    {
      Serial.print("Error ocurred while deserializing json message \"");
      Serial.print(message);
      Serial.print("\": ");
      Serial.println(result.c_str());
    }
  }
  else
  {
    const char* target = json["target"];
    if (c_strings_equal(target, "me"))
    {
      const char* command = json["command"];
    }
  }
}
