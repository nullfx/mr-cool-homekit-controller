#include "AcPort.h"

#include "config.h"

#if AC_LINK_ON_USB_PORT
#include <HWCDC.h>
#include <soc/io_mux_reg.h>
#include <soc/usb_serial_jtag_struct.h>
#endif

namespace mrcool {

AcPort chooseAcPort() {
#if AC_LINK_ON_USB_PORT
  // A computer sends USB start-of-frame packets every millisecond; the AC
  // (or a phone charger) never does.
  const uint32_t start = millis();
  while (!HWCDC::isPlugged() && millis() - start < USB_HOST_DETECT_MS)
    delay(10);
  if (HWCDC::isPlugged())
    return {false, -1, -1, "USB port: a computer is attached, so it stays a serial console"};

  // Disconnect the USB PHY and its 1.5k D+ pull-up so the pins behave as
  // plain GPIO. (The GPIO driver also clears usb_pad_enable when the UART
  // claims these pins; doing it here makes the hand-over explicit.)
  USB_SERIAL_JTAG.conf0.pad_pull_override = 1;
  USB_SERIAL_JTAG.conf0.dp_pullup = 0;
  USB_SERIAL_JTAG.conf0.dp_pulldown = 0;
  USB_SERIAL_JTAG.conf0.dm_pullup = 0;
  USB_SERIAL_JTAG.conf0.dm_pulldown = 0;
  USB_SERIAL_JTAG.conf0.usb_pad_enable = 0;

  // Same convention as the wired mode: ESP TX -> D+, ESP RX <- D-.
  return {true, USB_INT_PHY0_DM_GPIO_NUM, USB_INT_PHY0_DP_GPIO_NUM, "the board's USB port"};
#else
  return {true, AC_UART_RX_PIN, AC_UART_TX_PIN, "wired pins"};
#endif
}

}  // namespace mrcool
