# TSPLPrinter

Arduino library for TSPL/TSC-command thermal label printers, communicating over a
`HardwareSerial` UART.

**Initial release scope:** QR-code label printing and status polling only. Nothing else
(plain text labels, barcodes, etc.) is implemented yet - this is a deliberately narrow
first version, extended only as real needs come up.

## Install

Standalone for now - unzip and drop the `TSPLPrinter` folder into your
`Documents/Arduino/libraries/` folder, then restart the Arduino IDE.

## Usage

```cpp
#include <TSPL_Printer.h>

TSPLPrinter printer(Serial2);   // pass whichever HardwareSerial you're wiring it to

void setup() {
    printer.begin(9600, /*rxPin=*, /*txPin=*/);

    // optional - defaults to a 60x40mm label
    printer.config.size_width_mm = 60.0f;
    printer.config.size_height_mm = 40.0f;
    printer.config.qr_x_dots = 100;
    printer.config.qr_y_dots = 100;

    TSPLPrinterStatus status;
    if (printer.pollStatus(status) && status == TSPLPrinterStatus::Normal) {
        printer.printQR("YOUR-DATA-HERE");
    }
}
```

See `examples/BasicQRPrint` for a complete sketch.

## API

- `TSPLPrinter(HardwareSerial &serialPort)` — constructor; you own the UART, this library
  never assumes a specific peripheral number.
- `begin(baud, rxPin, txPin)` — initializes the UART.
- `config` — public `TSPLLabelConfig` struct; set fields directly before printing.
- `printQR(const char *data)` — builds and sends a QR label job from `config`. Returns
  `false` only if the job couldn't be built (e.g. data too long); does not confirm the
  printer actually printed successfully.
- `pollStatus(TSPLPrinterStatus &status, timeoutMs = 500)` — queries printer status.
  Returns `false` if the printer didn't respond in time.
- `TSPLPrinter::statusToText(status)` — static helper, human-readable status text.

## Notes

- `pollStatus()`'s status query uses the exact byte sequence confirmed reliable on real
  hardware during testing (a single write including a trailing null byte) — not simplified
  to the "cleaner" 3-byte version, since that wasn't confirmed to work as consistently.
- The caller is responsible for avoiding UART peripheral conflicts with other devices on
  the same board (e.g. don't wire this to the same `HardwareSerial` instance used by
  another peripheral).
