#ifndef TSPL_PRINTER_H
#define TSPL_PRINTER_H

#include <Arduino.h>
#include <HardwareSerial.h>

// Status codes as reported by the printer's status query response (single
// status byte). Matches the TSPL/TSC command set used by common thermal
// label printers of this type.
enum class TSPLPrinterStatus : uint8_t
{
    Normal                       = 0x00,
    HeadOpen                     = 0x01,
    PaperJam                     = 0x02,
    PaperJamHeadOpen             = 0x03,
    OutOfPaper                   = 0x04,
    OutOfPaperHeadOpen           = 0x05,
    OutOfRibbon                  = 0x08,
    OutOfRibbonHeadOpen          = 0x09,
    OutOfRibbonPaperJam          = 0x0A,
    OutOfRibbonPaperJamHeadOpen  = 0x0B,
    OutOfRibbonOutOfPaper        = 0x0C,
    OutOfRibbonOutOfPaperHeadOpen = 0x0D,
    Paused                       = 0x10,
    Printing                     = 0x20,
    OtherError                   = 0x80
};

// Label + QR job configuration. Public fields, set directly before calling
// printQR() - e.g. printer.config.size_width_mm = 25.0f;
// Defaults match a commonly-used starting point (60mm x 40mm label).
struct TSPLLabelConfig
{
    float size_width_mm   = 60.0f;   // label width, mm
    float size_height_mm  = 40.0f;   // label height, mm
    float gap_width_mm    = 0.0f;    // gap between labels, mm (0 = continuous stock)
    float gap_height_mm   = 0.0f;    // gap offset, mm
    uint8_t direction     = 1;       // TSPL DIRECTION parameter (0 or 1)

    uint16_t qr_x_dots    = 100;     // QR position, in dots (not mm)
    uint16_t qr_y_dots    = 100;
    char qr_ecc           = 'H';     // error correction: L (7%), M (15%), Q (25%), H (30%)
    uint8_t qr_cell_width = 4;       // module size, 1-10
    char qr_mode          = 'A';     // 'A' = auto-encode, 'M' = manual
    uint16_t qr_rotation  = 0;       // 0, 90, 180, or 270

    uint8_t print_sets    = 1;       // TSPL PRINT m parameter
    uint8_t print_copies  = 1;       // TSPL PRINT n parameter
};

class TSPLPrinter
{
public:
    // Pass the HardwareSerial instance this printer is wired to (e.g.
    // Serial1, Serial2). The caller owns the UART - this library never
    // assumes which peripheral number it is, avoiding conflicts with other
    // devices on the same board.
    explicit TSPLPrinter(HardwareSerial &serialPort);

    void begin(unsigned long baud, int8_t rxPin, int8_t txPin);

    // Builds a QR-code label job from the current `config` and sends it.
    // Returns false if the job could not be built (e.g. data too long for
    // the internal buffer) - does not confirm the printer actually printed
    // it. Call pollStatus() separately to check printer health before
    // and/or after calling this.
    bool printQR(const char *data);

    // Queries the printer's current status. Returns false if the printer
    // did not respond within timeoutMs (status is left unchanged in that
    // case); returns true and fills `status` if it did respond.
    bool pollStatus(TSPLPrinterStatus &status, unsigned long timeoutMs = 500);

    // Human-readable text for a status code, for logging/display.
    static const char *statusToText(TSPLPrinterStatus status);

    TSPLLabelConfig config;

private:
    HardwareSerial &_serial;
    void _flushRx();
};

#endif
