#pragma once

///        ///
// Includes //
///        ///

// DSMR P1 parser: https://github.com/matthijskooijman/arduino-dsmr
#include "dsmr.h"

#include "settings.h"

///                          ///
// Setup the DSMR data struct //
///                          ///

namespace dsmr
{
  namespace fields
  {
    // Obis id '0-0:96.1.4': Meter serial nr - ID

    DEFINE_FIELD(meter_ID_electr, String, ObisId(0, 0, 96, 1, 4), StringField, 0, 100);
    constexpr ObisId meter_ID_electr::id;
    constexpr char meter_ID_electr::name_progmem[];

    // Obis id '1-0:31.4.0': Fuse supervision threshold
    DEFINE_FIELD(current_Max, uint16_t, ObisId(1, 0, 31, 4, 0), IntField, units::A);
    constexpr ObisId current_Max::id;
    constexpr char current_Max::name_progmem[];

    // Obis id '0-1:96.1.1': Gas meter identifier
    DEFINE_FIELD(meter_ID_gas, String, ObisId(0, 1, 96, 1, 1), StringField, 0, 96);
    constexpr ObisId meter_ID_gas::id;
    constexpr char meter_ID_gas::name_progmem[];

    // Obis id '0-1:24.2.3': Gas timestamp + Gas m^3 delivered
    DEFINE_FIELD(gas_m3, TimestampedFixedValue, ObisId(0, 1, 24, 2, 3), TimestampedFixedField, units::m3, units::dm3);
    constexpr ObisId gas_m3::id;
    constexpr char gas_m3::name_progmem[];
  }
}

using FluviusDSMRData = ParsedData<
    identification,              // String
    timestamp,                   // String
    electricity_threshold,       // FixedValue
    meter_ID_electr,             // String (MM 23-5-2023: added)
    equipment_id,                // String
    electricity_switch_position, // uint8_t
    electricity_tariff,          // String
    current_Max,                 // uint16_t (MM 23-5-2023: added)
    energy_delivered_tariff1,    // FixedValue
    energy_delivered_tariff2,    // FixedValue
    energy_returned_tariff1,     // FixedValue
    energy_returned_tariff2,     // FixedValue
    power_delivered,             // FixedValue
    power_returned,              // FixedValue
    message_long,                // String
    voltage_l1,                  // FixedValue
    voltage_l2,                  // FixedValue
    voltage_l3,                  // FixedValue
    current_l1,                  // uint16_t
    current_l2,                  // uint16_t
    current_l3,                  // uint16_t
    power_delivered_l1,          // FixedValue
    power_delivered_l2,          // FixedValue
    power_delivered_l3,          // FixedValue
    power_returned_l1,           // FixedValue
    power_returned_l2,           // FixedValue
    power_returned_l3,           // FixedValue
    gas_device_type,             // uint16_t
    gas_valve_position,          // uint8_t
    meter_ID_gas,                // String (MM 23-5-2023: added)
    gas_m3                       // TimestampedFixedValue (MM 23-5-2023: added)
    >;

///                 ///
// Class declaration //
///                 ///

class FluviusDSMRWrapper
{
public:
  void init();
  void set_on_message_callback(void (*on_message_callback)(FluviusDSMRData &message));
  // This reads new data in the uart stream and calls on_message_callback for every new message
  void process_incoming_data();
  // Triggers a one-off reading
  void trigger_read();
  // Prints a FluviusDSMRData struct to the debug console
  void print_dsmr_values(FluviusDSMRData &data);

private:
  bool is_initialized = false;
  HardwareSerial dsmr_p1_hardware_serial = HardwareSerial(settings.dsmr_p1_uart_controller_index);
  P1Reader dsmr_p1_reader = P1Reader(&dsmr_p1_hardware_serial, settings.dsmr_p1_unconnected_request_output_pin);
  void (*on_message_callback)(FluviusDSMRData &message) = nullptr;
};

///                                   ///
// Public class method implementations //
///                                   ///

void FluviusDSMRWrapper::init()
{
  assert(!is_initialized); // Ensure this is only called once

  dsmr_p1_hardware_serial.begin(115200, SERIAL_8N1, settings.dsmr_p1_uart_rx_pin, settings.dsmr_p1_uart_unconnected_tx_pin);
  // Essential AFTER serial.begin: use ESP32 input pull-up (~45K) (omits the external 10K pullup and simplifies interconnection)
  pinMode(settings.dsmr_p1_uart_rx_pin, INPUT_PULLUP);

  is_initialized = true;
}

void FluviusDSMRWrapper::set_on_message_callback(void (*on_message_callback)(FluviusDSMRData &message))
{
  this->on_message_callback = on_message_callback;
}

void FluviusDSMRWrapper::process_incoming_data()
{
  assert(is_initialized);                 // Ensure init() was called
  assert(on_message_callback != nullptr); // Ensure on_message_callback was set

  // MM: timing per Fluvius telegram: uart reading 62 ms + parsing 1 ms

  dsmr_p1_reader.loop(); // Processes any new data in the uart stream, then returns

  auto handle_parser_error = [](String &err)
  {
    if (settings.use_debug_serial)
      Serial.println(err);

    // NOTE: harlmess, but for some unclear reason: 1st time the parser is called, it always detects this fault on the 2nd OBIS field:
    // > 0-0:96.1.4(50217)
    // >            ^
    // > Duplicate field

    // No additional fault handling provided yet; e.g. add Failures-counter...
  };

  // available() returns true when a complete and 'correct' message was received (can still be semantically incorrect -> parsing error)
  while (dsmr_p1_reader.available())
  {
    FluviusDSMRData message;
    String error;
    if (dsmr_p1_reader.parse(&message, &error))
    {
      on_message_callback(message);
    }
    else
    {
      handle_parser_error(error);
    }
  }
}

void FluviusDSMRWrapper::trigger_read()
{
  dsmr_p1_reader.enable(true);
}

#define print_val(str, val) \
  Serial.print(str ": ");   \
  Serial.println(val)
#define print_val_float(str, val, precision) \
  Serial.print(str ": ");                    \
  Serial.println(val, precision)

void FluviusDSMRWrapper::print_dsmr_values(FluviusDSMRData &data)
{
  assert(is_initialized); // Ensure init() was called

  // This prints the raw input (full telegram), for debugging purposes
  // Serial.println(raw);

  Serial.println("DSMR Parsing result (relevant fields):");

  print_val("timestamp", data.timestamp);

  print_val_float("energy_delivered_tariff1 (kWh)", data.energy_delivered_tariff1, 3);
  print_val_float("energy_delivered_tariff2 (kWh)", data.energy_delivered_tariff2, 3);
  print_val_float("energy_returned_tariff1  (kWh)", data.energy_returned_tariff1, 3);
  print_val_float("energy_returned_tariff2  (kWh)", data.energy_returned_tariff2, 3);
  print_val_float("power_delivered (kWh)", data.power_delivered, 3);
  print_val_float("power_returned  (kWh)", data.power_returned, 3);
  print_val_float("voltage_l1 (V)", data.voltage_l1, 1);
  print_val_float("voltage_l2 (V)", data.voltage_l2, 1);
  print_val_float("voltage_l3 (V)", data.voltage_l3, 1);
  print_val_float("current_l1 (A)", data.current_l1, 2);
  print_val_float("current_l2 (A)", data.current_l2, 2);
  print_val_float("current_l3 (A)", data.current_l3, 2);
  print_val_float("power_delivered_l1 (kWh)", data.power_delivered_l1, 3);
  print_val_float("power_delivered_l2 (kWh)", data.power_delivered_l2, 3);
  print_val_float("power_delivered_l3 (kWh)", data.power_delivered_l3, 3);
  print_val_float("power_returned_l1  (kWh)", data.power_returned_l1, 3);
  print_val_float("power_returned_l2  (kWh)", data.power_returned_l2, 3);
  print_val_float("power_returned_l3  (kWh)", data.power_returned_l3, 3);

  print_val("Gas timestamp", data.gas_m3.timestamp); // TimestampedFixedValue::timestamp
  print_val_float("gas_m3", data.gas_m3, 3);         // TimestampedFixedValue, default casts to float

  Serial.println();
}

#undef print_val
#undef print_val_float
