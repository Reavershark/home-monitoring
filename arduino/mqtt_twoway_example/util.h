#pragma once

#include <ArduinoJson.h>

#define uchar unsigned char
#define byte char
#define ubyte uchar

template<typename T>
void run_in_interval_nonblocking(uint32_t *const state, const uint32_t interval_msecs, T func)
{
  uint32_t &last_send_timestamp_msecs = *state;

  const uint32_t curr_timestamp_msecs = millis();
  if (curr_timestamp_msecs < last_send_timestamp_msecs) // Detect millis() overflow
    last_send_timestamp_msecs = 0;
  const uint32_t msecs_since_last_send = curr_timestamp_msecs - last_send_timestamp_msecs;
  if (msecs_since_last_send >= interval_msecs)
  {
    last_send_timestamp_msecs = curr_timestamp_msecs;
    func();
  }
}

struct DynamicJsonParseResult
{
  DynamicJsonParseResult(size_t document_capacity) : json(document_capacity) {}
  DynamicJsonDocument json;
  bool success = false;
};

DynamicJsonParseResult dynamic_json_parse(String &json_string, size_t document_capacity, bool use_serial)
{
  DynamicJsonParseResult result(document_capacity);

  DeserializationError possible_error = deserializeJson(result.json, json_string);
  if (possible_error != DeserializationError::Ok)
  {
    if (use_serial)
    {
      Serial.print("Error ocurred while deserializing json message \"");
      Serial.print(json_string);
      Serial.print("\": ");
      Serial.println(possible_error.c_str());
    }
    result.success = false;
  }
  
  return result;
}
