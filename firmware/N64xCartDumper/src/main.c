/**
 * SPX-License-Identifier: BSD-2-Clause 
 * Copyright (c) 2023 - NopJne
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>



#include "bsp/board.h"
#include "tusb.h"
#include "n64cartinterface.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

enum  {
  USB_LED_NOT_MOUNTED,
  USB_LED_MOUNTED,
  USB_LED_SUSPENDED,
};

static uint8_t usb_led_mode = USB_LED_NOT_MOUNTED;

void led_blinking_task(void);
void cdc_task(void);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  cartio_init();
  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();

    cdc_task();
    EepromIdleFlushTask();
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  usb_led_mode = USB_LED_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  usb_led_mode = USB_LED_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  usb_led_mode = USB_LED_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  usb_led_mode = USB_LED_MOUNTED;
}


//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
void cdc_task(void)
{
  // connected() check for DTR bit
  // Most but not all terminal client set this when making connection
  // if ( tud_cdc_connected() )
  {
    // connected and there are data available
    if ( tud_cdc_available() )
    {
      // read data
      char buf[64];
      uint32_t count = tud_cdc_read(buf, sizeof(buf));
      (void) count;

      // Echo back
      // Note: Skip echo by commenting out write() and write_flush()
      // for throughput test e.g
      //    $ dd if=/dev/zero of=/dev/ttyACM0 count=10000
      tud_cdc_write(buf, count);
      tud_cdc_write_flush();
    }
  }
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void) itf;
  (void) rts;

  // TODO set some indicator
  if ( dtr )
  {
    // Terminal connected
  }else
  {
    // Terminal disconnected
  }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
  (void) itf;
}

//--------------------------------------------------------------------+
// STATUS LED TASK
//--------------------------------------------------------------------+
// Solid = ready. Blink pattern indicates activity, priority order:
// error (fast) > write (double-blink) > read (single blink) > idle.
#define LED_ACTIVITY_HOLD_MS 150
#define LED_ERROR_BLINK_MS   80
#define LED_READ_BLINK_MS    150

void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;
  uint32_t now = board_millis();

  if (gEepromCommError || gEepromWriteSizeMismatch) {
    if (now - start_ms < LED_ERROR_BLINK_MS) return;
    start_ms = now;
    board_led_write(led_state);
    led_state = 1 - led_state;
    return;
  }

  if (now - gLastCartWriteMs < LED_ACTIVITY_HOLD_MS) {
    // Two quick pulses, then a pause.
    uint32_t pos = now % 500;
    board_led_write(pos < 80 || (pos >= 160 && pos < 240));
    return;
  }

  if (now - gLastCartReadMs < LED_ACTIVITY_HOLD_MS) {
    if (now - start_ms < LED_READ_BLINK_MS) return;
    start_ms = now;
    board_led_write(led_state);
    led_state = 1 - led_state;
    return;
  }

  switch (usb_led_mode) {
    case USB_LED_MOUNTED:
      board_led_write(true);
      break;
    case USB_LED_SUSPENDED:
    case USB_LED_NOT_MOUNTED:
    default:
      board_led_write(false);
      break;
  }
  led_state = false;
  start_ms = now;
}
