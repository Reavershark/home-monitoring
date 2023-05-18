//////////////
// Settings //
//////////////

#include "settings.h" // Create by copying settings.h.example to settings.h and filling in the dummy values

///////////////
// Libraries //
///////////////

// Install with:
//   - Set additional board manager urls in settings to:
//       https://arduino.esp8266.com/stable/package_esp8266com_index.json
//       https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
//   - Install esp8266 boards through the board manager (contains a good WiFi.h)
//   - Install ArduinoJson through the library manager

#ifdef ESP8266
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h> // Works for arduino, esp32...
#endif
#include <WiFiClient.h>
#include <ArduinoJson.h>

/////////////
// Globals //
/////////////

WiFiClient tcp_client;
unsigned long last_send_timestamp_msecs = 0;

///////////////////////////
// Function declarations //
///////////////////////////

void connect_wifi();
void setup();
void loop();

//////////////////////////////
// Function implementations //
//////////////////////////////

#ifdef USE_SERIAL
  #define SERIAL_CALL(arg) Serial.arg
#else
  #define SERIAL_CALL(arg)
#endif

void connect_wifi()
{
  while (1)
  {
    SERIAL_CALL(print("Connecting to wifi with SSID: "));
    SERIAL_CALL(println(wifi_ssid));

    if (WiFi.begin(wifi_ssid, wifi_pass) != WL_CONNECTED)
    {
      SERIAL_CALL(print("Failed to connect, retrying in "));
      SERIAL_CALL(println(wifi_retry_connect_delay_msecs));
      SERIAL_CALL(println("ms"));
      delay(wifi_retry_connect_delay_msecs);
      continue;
    }
    else
    {
      SERIAL_CALL(println("Successfully connected to wifi"));
      break;
    }
  }
}

void setup()
{
  SERIAL_CALL(begin(9600));
  connect_wifi();
  tcp_client.connect(http_server_address, http_server_port);
}

void loop()
{
  // Check if wifi is still connected
  if (!WiFi.status() != WL_CONNECTED)
  {
    // Wifi was disconnected, try to reconnect
    connect_wifi();
  }

  // Read and discard any available data
  while (tcp_client.read() != -1)
  {
  }

  // Check if the tcp client is still connected
  if (!tcp_client.connected())
  {
    tcp_client.stop();
    tcp_client.connect(http_server_address, http_server_port);
  }

  unsigned long curr_timestamp_msecs = millis();
  unsigned long time_since_last_send = curr_timestamp_msecs - last_send_timestamp_msecs;
  if (time_since_last_send >= send_interval_msecs)
  {
    last_send_timestamp_msecs = curr_timestamp_msecs;

    int value = 1 + (millis() % 789) / 789.0;

    // Create json object to send
    // Use https://arduinojson.org/v6/assistant to get the recommended size
    // Example json: See readme file
    String json_string;
    {
      StaticJsonDocument<192> json; // Gets destroyed when leaving this scope
      json["measurement"] = "water_depth";
      json["tags"]["location"] = "some_canal";
      json["fields"]["depth_in_meters"] = value;
      json["bucket"] = "default";
      serializeJson(json, json_string);
    }
    
    String http_message;
    {
      http_message += String("POST /somepath HTTP/1.1\n");

      // Headers
      http_message += String("Host: ") + String(http_server_address) + String("\n");
      http_message += String("Content-Length: ") + String(json_string.length()) + String("\n");
      http_message += String("\n"); // Indicate end of headers with an empty line
    }

    tcp_client.print(http_message);
  }
  else
  {
    delay(10);
  }
}
