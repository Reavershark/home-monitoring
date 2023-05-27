#pragma once

#define uchar unsigned char
#define ushort unsigned short
#define uint unsigned int
#define ulong unsigned long

#define byte char
#define ubyte unsigned char

template<typename T>
void run_in_interval_nonblocking(ulong *const state, const ulong interval_msecs, T func)
{
  ulong &last_send_timestamp_msecs = *state;
  
  const unsigned long curr_timestamp_msecs = millis();
  if (curr_timestamp_msecs < last_send_timestamp_msecs) // Detect millis() overflow
    last_send_timestamp_msecs = 0;
  const unsigned long msecs_since_last_send = curr_timestamp_msecs - last_send_timestamp_msecs;
  if (msecs_since_last_send >= interval_msecs)
  {
    last_send_timestamp_msecs = curr_timestamp_msecs;
    func();
  }
}
