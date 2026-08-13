# TSPLPrinter

Arduino library for TSPL/TSC-command thermal label printers, communicating over a
`HardwareSerial` UART.

**Scope:** QR, TEXT, and DataMatrix label printing (single or combined on one label),
plus status polling. Nothing else (other barcode types, etc.) is implemented yet -
extended only as real needs come up.

## Install

Standalone for now - unzip and drop the `TSPLPrinter` folder into your
`Documents/Arduino/libraries/` folder, then restart the Arduino IDE.

## Usage

### Single QR code (original API, unchanged)

```cpp
#include <TSPL_Printer.h>

TSPLPrinter printer(Serial2);   // pass whichever HardwareSerial you're wiring it to

void setup() {
    printer.begin(9600, /*rxPin=*/17, /*txPin=*/35);

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

### Combining TEXT + DataMatrix (or QR) on one label

Useful for data verification - print a DataMatrix/QR code alongside a plain-text line
of the same payload, so it can be checked without scanning.

```cpp
const char *payload = "LOT-4471-BATCH-9";

printer.beginLabel();                 // clears any previously queued elements

TSPLDMatrixConfig dm;
dm.x = 100; dm.y = 100;
dm.width = 300; dm.height = 300;
dm.module_size = 8;                   // dots per module
printer.addDataMatrix(payload, dm);

TSPLTextConfig txt;
txt.x = 100; txt.y = 420;             // below the DataMatrix - you compute this
txt.font = TSPL_FONT_2;
printer.addText(payload, txt);

printer.printLabel();                 // builds + sends one job for both elements
```

Each element can also be used on its own - just call one `add*()` and then
`printLabel()`. `addQR()` works the same way with a `TSPLQRConfig`.

See `examples/BasicQRPrint` and `examples/CombinedLabel` for complete sketches.

## API

- `TSPLPrinter(HardwareSerial &serialPort)` — constructor; you own the UART, this library
  never assumes a specific peripheral number.
- `begin(baud, rxPin, txPin)` — initializes the UART.
- `config` — public `TSPLLabelConfig` struct; label-level fields (size, gap, direction,
  print sets/copies) plus the legacy QR fields used by `printQR()`.
- `printQR(const char *data)` — builds and sends a single QR label job from `config`.
  Unchanged from the original release. Returns `false` only if the job couldn't be
  built (e.g. data too long); does not confirm the printer actually printed successfully.
- `beginLabel()` — clears the queued element list, before building a new label.
- `addText(data, TSPLTextConfig)` / `addQR(data, TSPLQRConfig)` /
  `addDataMatrix(data, TSPLDMatrixConfig)` — queue one element for the next
  `printLabel()` call. Can be mixed and called multiple times (up to 8 elements per
  label). Return `false` if `data` is null/empty, too long (128 bytes max per element),
  the element list is full, or (for DataMatrix) `rows`/`cols` are invalid.
- `printLabel()` — builds one job from `config` (SIZE/GAP/DIRECTION/CLS) plus all
  queued elements in the order added, plus PRINT, and sends it. Returns `false` if no
  elements were queued or the job doesn't fit the internal buffer.
- `pollStatus(TSPLPrinterStatus &status, timeoutMs = 500)` — queries printer status.
  Returns `false` if the printer didn't respond in time.
- `TSPLPrinter::statusToText(status)` — static helper, human-readable status text.

## Config structs

- `TSPLLabelConfig` — label-level: `size_width_mm`, `size_height_mm`, `gap_width_mm`,
  `gap_height_mm`, `direction` (1 = top-to-bottom, 0 = bottom-to-top), `print_sets`,
  `print_copies`; plus legacy `qr_*` fields for `printQR()`.
- `TSPLQRConfig` — per-element, for `addQR()`: `x`, `y`, `ecc` ('L'/'M'/'Q'/'H'),
  `cell_width` (1-10), `mode` ('A'/'M'), `rotation` (0/90/180/270).
- `TSPLTextConfig` — per-element, for `addText()`: `x`, `y`, `font` (see `TSPL_FONT_*`
  constants), `rotation`, `x_mult`/`y_mult` (1-10), `use_alignment` + `alignment`.
  `use_alignment` defaults to `false`, emitting the 7-field TEXT form from the manual's
  own basic example (works on all firmware). Only set it `true` once you've confirmed
  your printer's firmware supports the alignment parameter (added in V6.73 EZ).
- `TSPLDMatrixConfig` — per-element, for `addDataMatrix()`: `x`, `y`, `width`, `height`,
  `module_size` (0 = omit, printer auto-sizes), `rotation` (0 = omit, is also default),
  `shape` (0 = square/omit, 1 = rectangle), `rows`/`cols` (10-144, must be set together).
  Each optional TSPL field is included in the generated command only when set to a
  non-default value, matching how the manual's own examples selectively include them.

## Notes

- `pollStatus()`'s status query uses the exact byte sequence confirmed reliable on real
  hardware during testing (a single write including a trailing null byte) — not simplified
  to the "cleaner" 3-byte version, since that wasn't confirmed to work as consistently.
- The caller is responsible for avoiding UART peripheral conflicts with other devices on
  the same board (e.g. don't wire this to the same `HardwareSerial` instance used by
  another peripheral).
- Element `data` (TEXT/QR/DataMatrix content) is capped at 128 bytes per element and up
  to 8 elements per label - both fixed sizes, no dynamic allocation. Raise
  `MAX_ELEMENT_DATA_LEN` / `MAX_ELEMENTS` in `TSPL_Printer.h` if you need more.
- FNC1/GS1 control sequences inside DataMatrix content (the `c#` escape-char parameter)
  are not yet implemented - only plain text/URL-style payloads are supported for now.