#pragma once

// Install dependencies with:
//   - Set additional board manager urls in settings to:
//       https://arduino.esp8266.com/stable/package_esp8266com_index.json
//       https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
//   - Install esp8266 boards through the board manager (contains a good WiFiClient.h for arduinos)

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
   * If use_debug_serial is true, it is assumed that Serial.begin(...) is called in setup().
   * The constructor does nothing but store its arguments.
   */
  WifiHttpClient(
      const char *wifi_ssid, const char *wifi_pass,
      const char *http_server_address, const uint16_t http_server_port,
      const bool use_debug_serial = false,
      const uint32_t wifi_connected_check_delay_msecs = 1000, const uint32_t wifi_connected_check_times = 5);

  void first_connect();
  void reconnect_if_needed();

  void send_post(String path, String body);

private: // Private methods
  bool connect_wifi();
  bool connect_tcp();

private: // Attributes
  const char *wifi_ssid;
  const char *wifi_pass;
  const char *http_server_address;
  const uint16_t http_server_port;
  const bool use_debug_serial;
  const uint32_t wifi_connected_check_delay_msecs;
  const uint32_t wifi_connected_check_times;

  WiFiClient tcp_client;
};

///                                   ///
// Public class method implementations //
///                                   ///

WifiHttpClient::WifiHttpClient(
    const char *wifi_ssid, const char *wifi_pass,
    const char *http_server_address, const uint16_t http_server_port,
    const bool use_debug_serial,
    const uint32_t wifi_connected_check_delay_msecs, const uint32_t wifi_connected_check_times)
    : wifi_ssid(wifi_ssid), wifi_pass(wifi_pass),
      http_server_address(http_server_address), http_server_port(http_server_port),
      use_debug_serial(use_debug_serial),
      wifi_connected_check_delay_msecs(wifi_connected_check_delay_msecs), wifi_connected_check_times(wifi_connected_check_times)
{
}

void WifiHttpClient::first_connect()
{
  connect_wifi();
}

void WifiHttpClient::reconnect_if_needed()
{
  // Check if wifi is still connected
  if (WiFi.status() != WL_CONNECTED)
  {
    // Wifi was disconnected, try to reconnect
    if (use_debug_serial) Serial.println(F("Wifi was disconnected, reconnecting..."));
    connect_wifi();
  }

  // Discard any available data
  tcp_client.flush();

  // Check if the tcp client is still connected
  if (!tcp_client.connected())
  {
    // The tcp session was closed
    if (use_debug_serial) Serial.println(F("The tcp session closed"));
    tcp_client.stop();
  }
}

void WifiHttpClient::send_post(String path, String body)
{
  if (tcp_client.connected())
  {
    tcp_client.flush();
    tcp_client.stop();
  }
  connect_tcp();
  
  String http_message;

  http_message += String(F("POST ")) + path + String(F(" HTTP/1.1\n"));

  // Headers
  http_message += String(F("Host: ")) + String(http_server_address) + String(F("\n"));
  http_message += String(F("Connection: close\n"));
  if (body.length() > 0)
    http_message += String("Content-Length: ") + String(body.length()) + String(F("\n"));
  http_message += String(F("\n")); // Indicate end of headers with an empty line

  // Body
  http_message += body;

  tcp_client.print(http_message);

  if (use_debug_serial)
    Serial.println(F("Successfully sent HTTP POST"));
}

///                                    ///
// Private class method implementations //
///                                    ///

bool WifiHttpClient::connect_wifi()
{
  while (true)
  {
    if (use_debug_serial)
    {
      Serial.print(F("Connecting to wifi with SSID: "));
      Serial.println(wifi_ssid);
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid, wifi_pass);
    int32_t i = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      if (i >= wifi_connected_check_times)
      {
        if (use_debug_serial)
          Serial.println(F("Failed to connect to wifi"));
        return false;
      }
      i++;
      Serial.print(".");
      delay(wifi_connected_check_delay_msecs);
    }
    Serial.println();
    if (use_debug_serial)
      Serial.println(F("Successfully connected to wifi"));
    break;
  }
}

bool WifiHttpClient::connect_tcp()
{
  if (use_debug_serial)
  {
    Serial.print(F("Connecting to http server at http://"));
    Serial.print(http_server_address);
    Serial.print(F(":"));
    Serial.println(http_server_port);
  }
  
  if (!tcp_client.connect(http_server_address, http_server_port))
  {
    if (use_debug_serial)
      Serial.println(F("Failed to connect to http server"));
    return false;
  }
  else
  {
    if (use_debug_serial)
      Serial.println(F("Successfully connected to the http server"));
    return true;
  }
}
