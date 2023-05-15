//////////////
// Settings //
//////////////

#include "settings.h" // Create by copying settings.h.example to settings.h and filling in the dummy values

///////////////
// Libraries //
///////////////

#include <WiFi.h>
#include <WiFiClient.h>

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
    if (time_since_last_send >= send_interval_msecs) {
        last_send_timestamp_msecs = curr_timestamp_msecs;

        // Create json object to send
        String json;
        {
            json += "{\n";
            json += "  \"hello\": \"world\"\n";
            json += "}\n";
        }

        String http_message;
        {
            http_message += String("POST /somepath HTTP/1.1\n");

            // Headers
            http_message += String("Host: ") + String(http_server_address) + String("\n");
            http_message += "\n"; // Indicate end of headers with an empty line

            // Body
            http_message += json;
        }

        tcp_client.print(http_message);
    }
    else
    {
        delay(10);
    }
}
