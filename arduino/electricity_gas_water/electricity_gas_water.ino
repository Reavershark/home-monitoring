// Sketch intended for use with a "ESP32 TTGO board with OLED display"
// Functionality:
//   - Electricity & Gas meter: Reads & parses Fluvius DSMR P1 telegrams from a serial input
//   - Water meter: Reads a Reed-relay, debounces and counts pulses
//   - Report measurements: Both on an OLED-display and printed on USB-Serial
//   - TODO: log to local server
// Dependencies:
//   - arduino-dsmr:
//     - Download the zip (save it as dsmr.zip): https://github.com/matthijskooijman/arduino-dsmr/archive/5a0b558e251fb6ce935f97c4b349740c594f70ee.zip
//     - Add the zip to the Arduino IDE using "Sketch -> Include Library -> Add .ZIP Library..."
//   - TFT_eSPI:
//     - Install the "TFT_eSPI" library version "2.5.0" through the library manager (Sketch -> Include Library -> Manage Libraries...)
// Version history :
//   - 1.1.0: Extended with Water meter: reed contact on pin XX (13 or 27), with internal pullup  (gives pulse every 0.5 liter)
//   - 1.2.0: Large refactor
//   - 1.3.0: Send measurements to home-monitoring collector over http
static const String version_stamp = "1.3.0";

///        ///
// Includes //
///        ///

#include <ArduinoJson.h>

#include "dsmr_wrapper.h"
#include "wifi_http_client.h"
#include "tft_display_wrapper.h"
#include "util.h"

#include "settings.h" // Create by copying settings.h.example to settings.h and filling in the dummy values

///       ///
// Globals //
///       ///

FluviusDSMRWrapper dsmr_wrapper;
WifiHttpClient wifi_http_client(
    settings.wifi_ssid, settings.wifi_pass,
    settings.http_server_address, settings.http_server_port,
    settings.use_debug_serial);
TftDisplayWrapper display_wrapper;

uint32_t power_consumption = 0; // in Wh

///                     ///
// Function declarations //
///                     ///

void setup();
void loop();
void on_dsmr_message_callback(FluviusDSMRData &message);

///                    ///
// Function definitions //
///                    ///

void setup()
{
  if (settings.use_debug_serial)
    Serial.begin(115200); // Setup debug console

  if (settings.do_initial_wait)
  {
    if (settings.use_debug_serial)
      Serial.println("Waiting for 3 seconds...");
    // Allows for some time to open the console window
    delay(3000);
  }

  if (settings.use_debug_serial)
    print_sketch_version(version_stamp, String(__FILE__));

  dsmr_wrapper.init();
  dsmr_wrapper.set_on_message_callback(on_dsmr_message_callback);

  wifi_http_client.first_connect();

  display_wrapper.init();

  if (settings.show_sketch_version_on_display)
  {
    display_wrapper.draw_sketch_version(version_stamp);
    // Keep the sketch version on the display for 2 seconds
    // Only then, start the actual program
    delay(2000);
  }
}

void loop()
{
  wifi_http_client.reconnect_if_needed();
  dsmr_wrapper.process_incoming_data();

  // Call dsmr_wrapper.trigger_read() every dsmr_p1_read_interval_msecs
  {
    static uint32_t run_trigger_read_state = 0;
    run_in_interval_nonblocking(&run_trigger_read_state, settings.dsmr_p1_read_interval_msecs, []()
                                { dsmr_wrapper.trigger_read(); });
  }

  // Call display_wrapper.draw_metrics() every second
  {
    static uint32_t run_display_draw_metrics_state = 0;
    run_in_interval_nonblocking(&run_display_draw_metrics_state, 1000, []()
                                { display_wrapper.draw_metrics(power_consumption, 0, String(settings.wifi_ssid)); });
  }
}

void on_dsmr_message_callback(FluviusDSMRData &message)
{
  if (settings.use_debug_serial)
    dsmr_wrapper.print_dsmr_values(message);

  // Store power consumption in Wh (original is kWh) for Display
  power_consumption = (message.power_delivered - message.power_returned) * 1000;

  // Send electricity measurement
  {
    // Construct a json string to send over HTTP
    String json_string;
    {
      // Create json object to send
      // Use https://arduinojson.org/v6/assistant to get the recommended static document size
      // Example json: See project readme file
      StaticJsonDocument<768> json; // Gets destroyed when leaving this scope

      json["bucket"] = "fluvius_smart_meter";
      json["measurement"] = "fluvius_smart_meter_electricity";

      json["tags"]["identification"] = message.identification;  // String
      json["fields"]["original_timestamp"] = message.timestamp; // String

      json["fields"]["electricity_threshold"] = message.electricity_threshold.val();       // FixedValue
      json["fields"]["meter_ID_electr"] = message.meter_ID_electr;                         // String (MM 23-5-2023: added)
      json["fields"]["equipment_id"] = message.equipment_id;                               // String
      json["fields"]["electricity_switch_position"] = message.electricity_switch_position; // uint8_t
      json["fields"]["electricity_tariff"] = message.electricity_tariff;                   // String
      json["fields"]["current_Max"] = message.current_Max;                                 // uint16_t (MM 23-5-2023: added)
      json["fields"]["energy_delivered_tariff1"] = message.energy_delivered_tariff1.val(); // FixedValue
      json["fields"]["energy_delivered_tariff2"] = message.energy_delivered_tariff2.val(); // FixedValue
      json["fields"]["energy_returned_tariff1"] = message.energy_returned_tariff1.val();   // FixedValue
      json["fields"]["energy_returned_tariff2"] = message.energy_returned_tariff2.val();   // FixedValue
      json["fields"]["power_delivered"] = message.power_delivered.val();                   // FixedValue
      json["fields"]["power_returned"] = message.power_returned.val();                     // FixedValue
      json["fields"]["message_long"] = message.message_long;                               // String
      json["fields"]["voltage_l1"] = message.voltage_l1.val();                             // FixedValue
      json["fields"]["voltage_l2"] = message.voltage_l2.val();                             // FixedValue
      json["fields"]["voltage_l3"] = message.voltage_l3.val();                             // FixedValue
      json["fields"]["current_l1"] = message.current_l1;                                   // uint16_t
      json["fields"]["current_l2"] = message.current_l2;                                   // uint16_t
      json["fields"]["current_l3"] = message.current_l3;                                   // uint16_t
      json["fields"]["power_delivered_l1"] = message.power_delivered_l1.val();             // FixedValue
      json["fields"]["power_delivered_l2"] = message.power_delivered_l2.val();             // FixedValue
      json["fields"]["power_delivered_l3"] = message.power_delivered_l3.val();             // FixedValue
      json["fields"]["power_returned_l1"] = message.power_returned_l1.val();               // FixedValue
      json["fields"]["power_returned_l2"] = message.power_returned_l2.val();               // FixedValue
      json["fields"]["power_returned_l3"] = message.power_returned_l3.val();               // FixedValue

      serializeJson(json, json_string);
    }
    wifi_http_client.send_post("/", json_string);
  }

  // Send gas measurement
  {
    // Construct a json string to send over HTTP
    String json_string;
    {
      // Create json object to send
      // Use https://arduinojson.org/v6/assistant to get the recommended static document size
      // Example json: See project readme file
      StaticJsonDocument<384> json; // Gets destroyed when leaving this scope

      json["bucket"] = "fluvius_smart_meter";
      json["measurement"] = "fluvius_smart_meter_gas";

      json["tags"]["identification"] = message.identification;         // String
      json["tags"]["equipment_id"] = message.equipment_id;             // String
      json["tags"]["gas_device_type"] = message.gas_device_type;       // uint16_t
      json["tags"]["gas_valve_position"] = message.gas_valve_position; // uint8_t
      json["tags"]["meter_ID_gas"] = message.meter_ID_gas;             // String (MM 23-5-2023: added)

      json["fields"]["original_timestamp"] = message.timestamp; // String
      json["fields"]["message_long"] = message.message_long;    // String

      json["fields"]["gas_m3"] = message.gas_m3.val();                        // TimestampedFixedValue (MM 23-5-2023: added)
      json["fields"]["gas_m3_original_timestamp"] = message.gas_m3.timestamp; // String

      serializeJson(json, json_string);
    }
    wifi_http_client.send_post("/", json_string);
  }
}
