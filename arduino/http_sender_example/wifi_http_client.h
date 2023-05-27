#pragma once

// Install dependencies with:
//   - Set additional board manager urls in settings to:
//       https://arduino.esp8266.com/stable/package_esp8266com_index.json
//       https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
//   - Install esp8266 boards through the board manager (contains a good WiFiClient.h)

///        ///
// Includes //
///        ///

#ifdef ESP8266
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h> // Works for arduino, esp32...
#endif
#include <WiFiClient.h>
#include "util.h"

///                 ///
// Class declaration //
///                 ///

/*
 * Very simple http client intended to send small amounts of data in intervals.
 * All response data is discarded.
 */
class WifiHttpClient
{
public: // Public methods
  /*
   * If use_serial is true, it is assumed that Serial.begin(...) is called in setup().
   * The constructor does nothing but store its arguments.
   */
  WifiHttpClient(
    const char *wifi_ssid, const char *wifi_pass,
    const char *http_server_address, const uint16_t http_server_port,
    const bool use_serial = false,
    const uint32_t wifi_retry_connect_delay_msecs = 8000, const uint32_t http_retry_connect_delay_msecs = 8000
  );

  void first_connect();
  void reconnect_if_needed();
  
  void send_post(String body);

private: // Private methods
  void connect_wifi();
  void connect_tcp();

private: // Attributes
  const char *wifi_ssid;
  const char *wifi_pass;
  const char *http_server_address;
  const uint16_t http_server_port;
  const bool use_serial;
  const uint32_t wifi_retry_connect_delay_msecs;
  const uint32_t http_retry_connect_delay_msecs;
  
  WiFiClient tcp_client;
};


///                                   ///
// Public class method implementations //
///                                   ///

WifiHttpClient::WifiHttpClient(
    const char *wifi_ssid, const char *wifi_pass,
    const char *http_server_address, const uint16_t http_server_port,
    const bool use_serial,
    const uint32_t wifi_retry_connect_delay_msecs, const uint32_t http_retry_connect_delay_msecs
)
: wifi_ssid(wifi_ssid), wifi_pass(wifi_pass),
  http_server_address(http_server_address), http_server_port(http_server_port),
  use_serial(use_serial),
  wifi_retry_connect_delay_msecs(wifi_retry_connect_delay_msecs), http_retry_connect_delay_msecs(http_retry_connect_delay_msecs)
{
}

void WifiHttpClient::first_connect()
{
  connect_wifi();
  connect_tcp();
}

void WifiHttpClient::reconnect_if_needed()
{   
  // Check if wifi is still connected
  if (!WiFi.status() != WL_CONNECTED)
  {
    // Wifi was disconnected, try to reconnect
    connect_wifi();
  }

  // Read and discard any available data
  ubyte buffer[16];
  while (tcp_client.read(buffer, sizeof(buffer)) != -1)
  {
  }

  // Check if the tcp client is still connected
  if (!tcp_client.connected())
  {
    // The tcp session was closed, try to reconnect
    tcp_client.stop();
    connect_tcp();
  }
}

void WifiHttpClient::send_post(String body)
{
    String http_message;
    
    http_message += String(F("POST /somepath HTTP/1.1\n"));

    // Headers
    http_message += String(F("Host: ")) + String(http_server_address) + String(F("\n"));
    if (body.length() > 0) http_message += String("Content-Length: ") + String(body.length()) + String(F("\n"));
    http_message += String(F("\n")); // Indicate end of headers with an empty line

    // Body
    http_message += body;

    tcp_client.print(http_message);
}


///                                    ///
// Private class method implementations //
///                                    ///

void WifiHttpClient::connect_wifi()
{
  while (true)
  {
    if (use_serial)
    {
      Serial.print(F("Connecting to wifi with SSID: "));
      Serial.println(wifi_ssid);
    }

    if (WiFi.begin(wifi_ssid, wifi_pass) != WL_CONNECTED)
    {
      if (use_serial)
      {
        Serial.print(F("Failed to connect, retrying in "));
        Serial.println(wifi_retry_connect_delay_msecs);
        Serial.println(F("ms"));
      }
      delay(wifi_retry_connect_delay_msecs);
      continue;
    }
    else
    {
      if (use_serial) Serial.println(F("Successfully connected to wifi"));
      break;
    }
  }
}

void WifiHttpClient::connect_tcp()
{
  while (true)
  {
    if (use_serial)
    {
      Serial.print(F("Connecting to http server at http://"));
      Serial.print(http_server_address);
      Serial.print(F(":"));
      Serial.println(http_server_port);
    }
    if (!tcp_client.connect(http_server_address, http_server_port))
    {
      if (use_serial)
      {
        Serial.print(F("Failed to connect, retrying in "));
        Serial.print(http_retry_connect_delay_msecs);
        Serial.println(F("ms"));
      }
      delay(http_retry_connect_delay_msecs);
      continue;
    }
    else
    {
      if (use_serial) Serial.println(F("Successfully connected to the http server"));
      break;
    }
  }
}
