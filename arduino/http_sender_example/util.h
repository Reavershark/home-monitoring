#pragma once

#define byte char
#define ubyte unsigned char

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
