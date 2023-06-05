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
static const String version_stamp = "1.2.0";


///        ///
// Includes //
///        ///

#include "dsmr_wrapper.h"
#include "tft_display_wrapper.h"
#include "util.h"
#include "settings.h"


///       ///
// Globals //
///       ///

FluviusDSMRWrapper dsmr_wrapper;
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
  if (settings.use_debug_serial) Serial.begin(115200); // Setup debug console

  display_wrapper.init();
  dsmr_wrapper.init();
  dsmr_wrapper.set_on_message_callback(on_dsmr_message_callback);

  if (settings.do_initial_wait)
  {
    if (settings.use_debug_serial) Serial.println("Waiting for 3 seconds...");
    // Allows for some time to open the console window
    delay(3000);
  }

  if (settings.use_debug_serial) print_sketch_version(version_stamp, String(__FILE__));

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
  dsmr_wrapper.process_incoming_data();

  // Call dsmr_wrapper.trigger_read() every dsmr_p1_read_interval_msecs
  {
    static uint32_t run_trigger_read_state = 0;
    run_in_interval_nonblocking(&run_trigger_read_state, settings.dsmr_p1_read_interval_msecs, []() {
      dsmr_wrapper.trigger_read();
    });
  }

  // Call display_draw_metrics() every second
  {
    static uint32_t run_display_draw_metrics_state = 0;
    run_in_interval_nonblocking(&run_display_draw_metrics_state, 1000, []() {
      display_wrapper.draw_metrics(power_consumption, 0);
    });
  }
}

void on_dsmr_message_callback(FluviusDSMRData &message)
{
  if (settings.use_debug_serial) dsmr_wrapper.print_dsmr_values(message);

  // Store power consumption in Wh (original is kWh) for Display
  power_consumption = (message.power_delivered - message.power_returned) * 1000;
}
