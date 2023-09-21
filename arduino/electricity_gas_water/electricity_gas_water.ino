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
static const String sketch_name = "electricity_gas_water";
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
float battery_current = 0; // -100 to 100

///                     ///
// Function declarations //
///                     ///

void setup();
void loop();
void on_dsmr_message_callback(FluviusDSMRData &message);
void read_battery_current();
void send_heartbeat();

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
  wifi_http_client.reconnect_if_needed(); // blocks but fails fast
  dsmr_wrapper.process_incoming_data(); // calls on_dsmr_message_callback for each new telegram

  // Call dsmr_wrapper.trigger_read() every dsmr_p1_read_interval_msecs
  {
    static uint32_t rii_dsmr_wrapper_trigger_read_state = 0;
    run_in_interval_nonblocking(&rii_dsmr_wrapper_trigger_read_state, settings.dsmr_p1_read_interval_msecs, []()
                                { dsmr_wrapper.trigger_read(); });
  }

  // Call read_battery_current() every battery_current_read_interval_msecs
  {
    static uint32_t rii_read_battery_current_state = 0;
    run_in_interval_nonblocking(&rii_read_battery_current_state, settings.battery_current_read_interval_msecs, []()
                                { read_battery_current(); });
  }

  // Call display_wrapper.draw_metrics() every second
  {
    static uint32_t rii_display_wrapper_draw_metrics_state = 0;
    run_in_interval_nonblocking(&rii_display_wrapper_draw_metrics_state, 1000, []()
                                { display_wrapper.draw_metrics(power_consumption, battery_current, 0, String(settings.wifi_ssid)); });
  }

  // Call send_heartbeat() every 30 seconds
  {
    static uint32_t rii_heartbeat_read_state = 0;
    run_in_interval_nonblocking(&rii_heartbeat_read_state, 30000, []()
                                { send_heartbeat(); });
  }
}

void on_dsmr_message_callback(FluviusDSMRData &message)
{
  if (settings.use_debug_serial)
    dsmr_wrapper.print_dsmr_values(message);

  // Store power consumption in W (original is Wh) for Display
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

      // Influxdb-specific
      json["bucket"] = "fluvius_smart_meter";
      json["measurement"] = "fluvius_smart_meter_electricity";

      // Metadata (general)
      json["tags"]["identification"] = message.identification; // String
      json["tags"]["equipment_id"] = message.equipment_id;     // String
      json["fields"]["message_long"] = message.message_long;   // String

      // Metadata (electricity-specific)
      json["tags"]["meter_id_electr"] = message.meter_id_electr;                                          // String (MM 23-5-2023: added)
      json["fields"]["electricity_switch_position"] = message.electricity_switch_position;                // uint8_t
      json["fields"]["electricity_threshold"] = fixed_value_to_json_float(message.electricity_threshold); // FixedValue
      json["fields"]["current_max"] = message.current_max;                                                // uint16_t (MM 23-5-2023: added)
      json["fields"]["electricity_tariff"] = message.electricity_tariff;                                  // String

      // Electricity aggregates
      json["fields"]["energy_delivered_tariff1"] = fixed_value_to_json_float(message.energy_delivered_tariff1); // FixedValue
      json["fields"]["energy_delivered_tariff2"] = fixed_value_to_json_float(message.energy_delivered_tariff2); // FixedValue
      json["fields"]["energy_returned_tariff1"] = fixed_value_to_json_float(message.energy_returned_tariff1);   // FixedValue
      json["fields"]["energy_returned_tariff2"] = fixed_value_to_json_float(message.energy_returned_tariff2);   // FixedValue

      // Electricity live values
      json["fields"]["power_delivered"] = fixed_value_to_json_float(message.power_delivered);       // FixedValue
      json["fields"]["power_delivered_l1"] = fixed_value_to_json_float(message.power_delivered_l1); // FixedValue
      json["fields"]["power_delivered_l2"] = fixed_value_to_json_float(message.power_delivered_l2); // FixedValue
      json["fields"]["power_delivered_l3"] = fixed_value_to_json_float(message.power_delivered_l3); // FixedValue
      json["fields"]["power_returned"] = fixed_value_to_json_float(message.power_returned);         // FixedValue
      json["fields"]["power_returned_l1"] = fixed_value_to_json_float(message.power_returned_l1);   // FixedValue
      json["fields"]["power_returned_l2"] = fixed_value_to_json_float(message.power_returned_l2);   // FixedValue
      json["fields"]["power_returned_l3"] = fixed_value_to_json_float(message.power_returned_l3);   // FixedValue
      json["fields"]["voltage_l1"] = fixed_value_to_json_float(message.voltage_l1);                 // FixedValue
      json["fields"]["voltage_l2"] = fixed_value_to_json_float(message.voltage_l2);                 // FixedValue
      json["fields"]["voltage_l3"] = fixed_value_to_json_float(message.voltage_l3);                 // FixedValue
      json["fields"]["current_l1"] = fixed_value_to_json_float(message.current_l1_redef);           // FixedValue
      json["fields"]["current_l2"] = fixed_value_to_json_float(message.current_l2_redef);           // FixedValue
      json["fields"]["current_l3"] = fixed_value_to_json_float(message.current_l3_redef);           // FixedValue

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

      // Influxdb-specific
      json["bucket"] = "fluvius_smart_meter";
      json["measurement"] = "fluvius_smart_meter_gas";

      // Metadata (general)
      json["tags"]["identification"] = message.identification; // String
      json["tags"]["equipment_id"] = message.equipment_id;     // String
      json["fields"]["message_long"] = message.message_long;   // String
      
      // Metadata (gas-specific)
      json["tags"]["meter_id_gas"] = message.meter_id_gas;                 // String (MM 23-5-2023: added)
      json["fields"]["gas_device_type"] = String(message.gas_device_type); // uint16_t
      json["fields"]["gas_valve_position"] = message.gas_valve_position;   // uint8_t

      // Gas live values
      json["fields"]["gas_m3"] = fixed_value_to_json_float(message.gas_m3); // TimestampedFixedValue (MM 23-5-2023: added)

      serializeJson(json, json_string);
    }
    wifi_http_client.send_post("/", json_string);
  }
}

void read_battery_current()
{
  uint16_t voltage_10bit = analogRead(settings.battery_current_read_pin);
  float voltage_normalized = voltage_10bit / 1023.0;
  float voltage_centered_on_zero = (voltage_normalized * 2) - 1;
  battery_current = voltage_centered_on_zero * 100;
}

void send_heartbeat()
{
  if (settings.use_debug_serial)
    Serial.println("Sending heartbeat");

  // Construct a json string to send over HTTP
  String json_string;
  {
    // Create json object to send
    // Use https://arduinojson.org/v6/assistant to get the recommended static document size
    StaticJsonDocument<192> json; // Gets destroyed when leaving this scope

    json["bucket"] = "heartbeat";
    json["measurement"] = "heartbeat";
    json["tags"]["device"] = settings.device_identifier;
    json["fields"]["software"] = sketch_name + String(" Arduino sketch");
    json["fields"]["software_version"] = version_stamp;
    json["fields"]["healthy"] = 1;

    serializeJson(json, json_string);
  }
  wifi_http_client.send_post("/", json_string);
}
