#pragma once
// Syntax-check stub. Not the real Pico SDK.
//
// Only the bring-up diagnostic uses this header: it waits for a host to open the USB CDC
// port before printing, so its banner is not lost to a port nobody is reading yet.

extern "C" {
bool stdio_usb_connected(void);
}
