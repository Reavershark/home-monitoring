#pragma once

#define byte char
#define ubyte unsigned char

void print_sketch_version(const String &version_stamp, const String &file_name)
{
  Serial.println(String("Sketch File    : ") + file_name);
  Serial.println(String("Sketch version : ") + version_stamp);
  Serial.println(
      String("Compiled: ") + String(__DATE__) + String(", ") + String(__TIME__) + String(", IDE version: ") + String(__VERSION__) + String("\n"));
}

// Runs for the first time when millis() == interval_msecs
template <typename T>
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
