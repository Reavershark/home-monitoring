#pragma once

///        ///
// Includes //
///        ///

// Graphics and font library for many TFT driver chips
// For examples, see: https://github.com/Bodmer/TFT_eSPI/tree/master/examples
// Note: This will also include the tft_setup.h file from this folder.
//       You need to create this file by copying the tft_setup.h.example file and modifying it with
//       the settings applicable to your display.
#include <TFT_eSPI.h>
#include <SPI.h>

///                 ///
// Class declaration //
///                 ///

class TftDisplayWrapper
{
public:
  void init();
  void draw_sketch_version(const String &version_stamp);
  void draw_metrics(const int32_t &power_consumption, const int32_t &teller_stand_water, const String &wifi_ssid);

private:
  bool is_initialized = false;
  TFT_eSPI tft;
};

///                                   ///
// Public class method implementations //
///                                   ///

void TftDisplayWrapper::init()
{
  assert(!is_initialized); // Ensure this is only called once

  tft.init();
  tft.setRotation(3); // 3 = upside-down

  is_initialized = true;
}

// Some tft method signatures:
//   - int16_t TFT_eSPI::drawString(const String& string, int32_t poX, int32_t poY, uint8_t font)

void TftDisplayWrapper::draw_sketch_version(const String &version_stamp)
{
  assert(is_initialized); // Ensure init() was called

  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(1);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(String("Sketch version : ") + version_stamp, 0, 0, 4);

  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString(String("Compiled: ") + String(__DATE__) + String(", ") + String(__TIME__), 0, 48, 4);

  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString(String(" IDE version: ") + String(__VERSION__), 0, 96, 4);
}

void TftDisplayWrapper::draw_metrics(const int32_t &power_consumption, const int32_t &teller_stand_water, const String &wifi_ssid)
{
  assert(is_initialized); // Ensure init() was called

  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(1);
  tft.setTextColor(power_consumption > 0 ? TFT_RED : TFT_GREEN, TFT_BLACK);
  tft.drawString(String("PCons=") + String(power_consumption), 10, 10, 4);

  tft.setTextSize(1);
  tft.setTextColor(TFT_BLUE, TFT_BLACK);
  tft.drawString(String("Water=") + String(teller_stand_water), 10, 40, 4);

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String("SSID=") + String(wifi_ssid), 10, 100, 4);

  // tft.setTextSize(1);
  // tft.setTextColor(TFT_BLUE, TFT_BLACK);
  // tft.drawString( "    3", 30, 80, 4); // cubic symbol
  // tft.drawString( "m   x 100   GAS", 30, 99, 4);

  // tft.drawString(String(TellerStandGas_delta) + String("   Delta/min"), 40, 80, 4);
}
