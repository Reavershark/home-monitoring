#pragma once

// Install dependencies with:
//   - Set additional board manager urls in settings to:
//       https://arduino.esp8266.com/stable/package_esp8266com_index.json
//       https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
//   - Install esp8266 boards through the board manager (contains a good WiFi.h)
//   - Install ArduinoJson through the library manager

///        ///
// Includes //
///        ///

#ifdef ESP8266
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h> // Works for arduino, esp32...
#endif
#include <WiFiClient.h>


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
    const char *http_server_address, const long http_server_port,
    const bool use_serial = false,
    const long wifi_retry_connect_delay_msecs = 8000
  );

  void first_connect();
  void reconnect_if_needed();
  
  void send_post(String body);

private: // Private methods
  void connect_wifi();

private: // Attributes
  const char *wifi_ssid;
  const char *wifi_pass;
  const char *http_server_address;
  const long http_server_port;
  const long wifi_retry_connect_delay_msecs;
  const bool use_serial;

  WiFiClient tcp_client;
};


///                                   ///
// Public class method implementations //
///                                   ///

WifiHttpClient::WifiHttpClient(
    const char *wifi_ssid, const char *wifi_pass,
    const char *http_server_address, const long http_server_port,
    const bool use_serial = false,
    const long wifi_retry_connect_delay_msecs
)
: wifi_ssid(wifi_ssid), wifi_pass(wifi_pass),
  http_server_address(http_server_address), http_server_port(http_server_port),
  wifi_retry_connect_delay_msecs(wifi_retry_connect_delay_msecs),
  use_serial(use_serial)
{
}

void WifiHttpClient::first_connect()
{
  connect_wifi();
  tcp_client.connect(http_server_address, http_server_port);
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
  while (tcp_client.read() != -1)
  {
  }

  // Check if the tcp client is still connected
  if (!tcp_client.connected())
  {
    tcp_client.stop();
    tcp_client.connect(http_server_address, http_server_port);
  }
}

void WifiHttpClient::send_post(String body)
{
    String http_message;
    
    http_message += String("POST /somepath HTTP/1.1\n");

    // Headers
    http_message += String("Host: ") + String(http_server_address) + String("\n");
    if (body.length() > 0) http_message += String("Content-Length: ") + String(body.length()) + String("\n");
    http_message += String("\n"); // Indicate end of headers with an empty line

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
    if (use_serial) Serial.print("Connecting to wifi with SSID: ");
    if (use_serial) Serial.println(wifi_ssid);

    if (WiFi.begin(wifi_ssid, wifi_pass) != WL_CONNECTED)
    {
      if (use_serial) Serial.print("Failed to connect, retrying in ");
      if (use_serial) Serial.println(wifi_retry_connect_delay_msecs);
      if (use_serial) Serial.println("ms");
      delay(wifi_retry_connect_delay_msecs);
      continue;
    }
    else
    {
      if (use_serial) Serial.println("Successfully connected to wifi");
      break;
    }
  }
}
