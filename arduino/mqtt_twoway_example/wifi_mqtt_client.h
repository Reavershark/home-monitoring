#pragma once

// Install dependencies with:
//   - Set additional board manager urls in settings to:
//       https://arduino.esp8266.com/stable/package_esp8266com_index.json
//       https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
//   - Install esp8266 boards through the board manager (contains a good WiFiClient.h for arduinos)
//   - Install ArduinoMqttClient through the library manager

///        ///
// Includes //
///        ///

#ifdef ESP8266
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h> // Works for arduino, esp32...
#endif
#include <WiFiClient.h>
#include <ArduinoMqttClient.h>
#include "util.h"


///                 ///
// Class declaration //
///                 ///

class WifiMqttClient
{
public: // Public methods
  /*
   * If use_serial is true, it is assumed that Serial.begin(...) is called in setup().
   * The constructor does nothing but store its arguments.
   */
  WifiMqttClient(
    const char *wifi_ssid, const char *wifi_pass,
    const char *mqtt_broker_address, const uint16_t mqtt_broker_port = 1833,
    const bool use_serial = false,
    const uint32_t incoming_message_size_limit = 256,
    const uint32_t wifi_connected_check_delay_msecs = 1000, const uint32_t mqtt_retry_connect_delay_msecs = 8000
  );

  void first_connect();
  void reconnect_if_needed();

  void publish(String topic, String message);

  bool is_on_message_set() const;
  void set_on_message(void (*func)(String &topic, String &message));
  void subscribe(const String topic);
  void process_incoming_messages();

private: // Private methods
  void connect_wifi();
  void connect_mqtt();

private: // Attributes
  const char *wifi_ssid;
  const char *wifi_pass;
  const char *mqtt_broker_address;
  const uint16_t mqtt_broker_port;
  const bool use_serial;
  const uint32_t incoming_message_size_limit;
  const uint32_t wifi_connected_check_delay_msecs;
  const uint32_t mqtt_retry_connect_delay_msecs;

  WiFiClient wifi_client;
  MqttClient mqtt_client = MqttClient(wifi_client);

  void (*on_message)(String &topic, String &message) = nullptr;

  static WifiMqttClient *instance;

  // Friends
  friend void on_mqtt_message_internal_callback(int32_t message_size);
};


///                                   ///
// Public class method implementations //
///                                   ///

WifiMqttClient::WifiMqttClient(
    const char *wifi_ssid, const char *wifi_pass,
    const char *mqtt_broker_address, const uint16_t mqtt_broker_port,
    const bool use_serial,
    const uint32_t incoming_message_size_limit,
    const uint32_t wifi_connected_check_delay_msecs, const uint32_t mqtt_retry_connect_delay_msecs
)
: wifi_ssid(wifi_ssid), wifi_pass(wifi_pass),
  mqtt_broker_address(mqtt_broker_address), mqtt_broker_port(mqtt_broker_port),
  use_serial(use_serial),
  incoming_message_size_limit(incoming_message_size_limit),
  wifi_connected_check_delay_msecs(wifi_connected_check_delay_msecs), mqtt_retry_connect_delay_msecs(mqtt_retry_connect_delay_msecs)
{
  assert(instance == nullptr); // Ensure another WifiMqttClient instance is not already active
  instance = this;
}

void WifiMqttClient::first_connect()
{
  connect_wifi();
  connect_mqtt();
}

void WifiMqttClient::reconnect_if_needed()
{   
  // Check if wifi is still connected
  if (WiFi.status() != WL_CONNECTED)
  {
    // Wifi was disconnected, try to reconnect
    connect_wifi();
  }

  // Check if the mqtt client is still connected
  if (!mqtt_client.connected())
  {
    mqtt_client.stop();
    connect_mqtt();
  }
}

void WifiMqttClient::publish(String topic, String message)
{
    mqtt_client.beginMessage(topic);
    mqtt_client.print(message);
    mqtt_client.endMessage();

    if (use_serial) Serial.println(F("Successfully published message"));
}

bool WifiMqttClient::is_on_message_set() const
{
  return on_message != nullptr;
}

void WifiMqttClient::set_on_message(void (*func)(String &topic, String &message))
{
  on_message = func;
}

void WifiMqttClient::subscribe(const String topic)
{
  mqtt_client.subscribe(topic);
}

void WifiMqttClient::process_incoming_messages()
{
  mqtt_client.poll();
}


///                                    ///
// Private class method implementations //
///                                    ///

void WifiMqttClient::connect_wifi()
{
  while (true)
  {
    if (use_serial)
    {
      Serial.print(F("Connecting to wifi with SSID: "));
      Serial.println(wifi_ssid);
    }
    delay(1000);
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid, wifi_pass);
    while(WiFi.status() != WL_CONNECTED)
    {
      Serial.print(".");
      delay(wifi_connected_check_delay_msecs);
    }
    Serial.println();
    if (use_serial) Serial.println(F("Successfully connected to wifi"));
    break;
  }
}

void WifiMqttClient::connect_mqtt()
{
  while (true)
  {
    if (use_serial) 
    {
      Serial.print(F("Connecting to mqtt broker at mqtt://"));
      Serial.print(mqtt_broker_address);
      Serial.print(F(":"));
      Serial.println(mqtt_broker_port);
    }

    if (!mqtt_client.connect(mqtt_broker_address, mqtt_broker_port))
    {
      if (use_serial)
      {
        Serial.print(F("Failed to connect with error code "));
        Serial.print(mqtt_client.connectError());
        Serial.print(F(", retrying in "));
        Serial.print(mqtt_retry_connect_delay_msecs);
        Serial.println(F("ms"));
      }
      delay(mqtt_retry_connect_delay_msecs);
      continue;
    }
    else
    {
      if (use_serial) Serial.println(F("Successfully connected to the mqtt broker"));
      break;
    }
  }
}


///                                  ///
// Static class attribute definitions //
///                                  ///

WifiMqttClient *WifiMqttClient::instance = nullptr;


///         ///
// Callbacks //
///         ///

void on_mqtt_message_internal_callback(int32_t message_size)
{
  WifiMqttClient *instance = WifiMqttClient::instance;

  if ((uint32_t)message_size >= instance->incoming_message_size_limit)
  {
    if (instance->use_serial) Serial.println(F("Received mqtt message is too large, skipping"));
    return;
  }

  String topic = instance->mqtt_client.messageTopic();

  String message = "";
  ubyte buffer[17];
  while (instance->mqtt_client.available())
  {
    memset(buffer, 0, 17);
    instance->mqtt_client.read(buffer, 16);
    message += (char *)buffer;
  }

  instance->on_message(topic, message);
}
