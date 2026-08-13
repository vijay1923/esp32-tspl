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

// TEXT command font names, from the TSPL manual. Pass any of these (or a
// raw string like "3.EFT" / "A.FNT") as TSPLTextConfig::font.
#define TSPL_FONT_0        "0"   // stretchable Monotype CG Triumvirate Bold Condensed
#define TSPL_FONT_1        "1"   // 8 x 12 fixed pitch dot font
#define TSPL_FONT_2        "2"   // 12 x 20 fixed pitch dot font
#define TSPL_FONT_3        "3"   // 16 x 24 fixed pitch dot font
#define TSPL_FONT_4        "4"   // 24 x 32 fixed pitch dot font
#define TSPL_FONT_5        "5"   // 32 x 48 fixed pitch dot font
#define TSPL_FONT_6        "6"   // 14 x 19 dot fixed pitch font OCR-B
#define TSPL_FONT_7        "7"   // 21 x 27 dot fixed pitch font OCR-B
#define TSPL_FONT_8        "8"   // 14 x 25 dot fixed pitch font OCR-A
#define TSPL_FONT_ROMAN    "ROMAN.TTF"

// Label-level configuration - applies to the whole label job (SIZE, GAP,
// DIRECTION, PRINT). Also still holds the legacy QR fields used by the
// existing printQR(data) call, unchanged, for backward compatibility.
// Public fields, set directly - e.g. printer.config.size_width_mm = 25.0f;
// Defaults match a commonly-used starting point (60mm x 40mm label).
struct TSPLLabelConfig
{
    float size_width_mm   = 60.0f;   // label width, mm
    float size_height_mm  = 40.0f;   // label height, mm
    float gap_width_mm    = 0.0f;    // gap between labels, mm (0 = continuous stock)
    float gap_height_mm   = 0.0f;    // gap offset, mm
    uint8_t direction     = 1;       // TSPL DIRECTION parameter (0 or 1)

    // Legacy QR fields - used only by printQR(data), the original
    // single-shot call. Kept exactly as before; new code should prefer
    // TSPLQRConfig + addQR() instead, see below.
    uint16_t qr_x_dots    = 100;
    uint16_t qr_y_dots    = 100;
    char qr_ecc           = 'H';
    uint8_t qr_cell_width = 4;
    char qr_mode          = 'A';
    uint16_t qr_rotation  = 0;

    uint8_t print_sets    = 1;       // TSPL PRINT m parameter
    uint8_t print_copies  = 1;       // TSPL PRINT n parameter
};

// Per-element config for a QRCODE command, used with addQR(). Same fields
// as the legacy config.qr_* group, just packaged so a label can carry more
// than one QR code, or mix QR with TEXT/DMATRIX.
struct TSPLQRConfig
{
    uint16_t x            = 100;   // dots
    uint16_t y            = 100;   // dots
    char ecc              = 'H';   // L (7%), M (15%), Q (25%), H (30%)
    uint8_t cell_width    = 4;     // module size, 1-10
    char mode             = 'A';   // 'A' = auto-encode, 'M' = manual
    uint16_t rotation     = 0;     // 0, 90, 180, or 270
};

// Per-element config for a TEXT command, used with addText().
struct TSPLTextConfig
{
    uint16_t x             = 10;            // dots
    uint16_t y             = 10;            // dots
    const char *font       = TSPL_FONT_2;   // see TSPL_FONT_* above
    uint16_t rotation      = 0;             // 0, 90, 180, or 270
    uint8_t x_mult         = 1;             // 1-10
    uint8_t y_mult         = 1;             // 1-10

    // Alignment was added in printer firmware V6.73 EZ. Leave this false
    // (default) to emit the 7-field TEXT form from the manual's own basic
    // example, which works on all firmware. Set true only once you've
    // confirmed your printer's firmware supports the 8-field form.
    bool use_alignment     = false;
    uint8_t alignment      = 0;             // 0 default/left, 1 left, 2 center, 3 right
};

// Per-element config for a DMATRIX command, used with addDataMatrix().
// Optional TSPL fields (x#, r#, a#, row/col) are each included in the
// generated command only when set to a non-default value below - matching
// how the manual's own examples selectively include them
// (e.g. "x6" alone, or "x8,18,18" skipping rotation/shape entirely).
struct TSPLDMatrixConfig
{
    uint16_t x            = 100;   // dots
    uint16_t y            = 100;   // dots
    uint16_t width         = 400;   // dots, expected barcode area width
    uint16_t height        = 400;   // dots, expected barcode area height

    uint8_t module_size    = 0;     // dots; 0 = omit "x#" (printer auto-sizes)
    uint16_t rotation      = 0;     // 0/90/180/270; 0 = omit "r#" (0 is also the default)
    uint8_t shape          = 0;     // 0 = square (default, omitted); 1 = rectangle ("a1")
    uint8_t rows           = 0;     // symbol row count, 10-144; must be set with cols
    uint8_t cols           = 0;     // symbol col count, 10-144; must be set with rows
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

    // --- Original single-shot QR call - unchanged, still works exactly as
    // before, using config.qr_* fields. ---
    // Builds a QR-code label job from the current `config` and sends it.
    // Returns false if the job could not be built (e.g. data too long for
    // the internal buffer) - does not confirm the printer actually printed
    // it. Call pollStatus() separately to check printer health before
    // and/or after calling this.
    bool printQR(const char *data);

    // --- Multi-element label building ---
    // A label can hold any mix of TEXT / QRCODE / DMATRIX elements, added
    // in the order you want them printed, then sent together in one job
    // with a single PRINT command. Typical use: a DataMatrix code plus a
    // TEXT line of the same payload, so the data can be read without
    // scanning. Elements can also be used independently, one at a time.
    //
    //   printer.beginLabel();
    //   printer.addDataMatrix(payload, dmCfg);
    //   printer.addText(payload, txtCfg);
    //   printer.printLabel();

    // Clears any elements queued from a previous label.
    void beginLabel();

    // Queues a TEXT element. Returns false if data is null/empty, too long
    // for the fixed per-element buffer, or the element list is full.
    bool addText(const char *data, TSPLTextConfig cfg);

    // Queues a QRCODE element. Same failure conditions as addText().
    bool addQR(const char *data, TSPLQRConfig cfg);

    // Queues a DMATRIX element. Same failure conditions as addText().
    bool addDataMatrix(const char *data, TSPLDMatrixConfig cfg);

    // Builds one job from config (SIZE/GAP/DIRECTION/CLS) + all queued
    // elements in the order added + PRINT, and sends it. Returns false if
    // no elements were queued, or if the combined job doesn't fit the
    // internal buffer. Does not clear the element list - call beginLabel()
    // before queuing the next label.
    bool printLabel();

    // Queries the printer's current status. Returns false if the printer
    // did not respond within timeoutMs (status is left unchanged in that
    // case); returns true and fills `status` if it did respond.
    bool pollStatus(TSPLPrinterStatus &status, unsigned long timeoutMs = 500);

    // Human-readable text for a status code, for logging/display.
    static const char *statusToText(TSPLPrinterStatus status);

    TSPLLabelConfig config;

private:
    static const uint8_t MAX_ELEMENTS = 8;
    static const size_t MAX_ELEMENT_DATA_LEN = 128;
    static const size_t JOB_BUF_SIZE = 1024;

    enum class ElementType : uint8_t { Text, QR, DMatrix };

    struct Element
    {
        ElementType type;
        char data[MAX_ELEMENT_DATA_LEN];
        TSPLTextConfig text;
        TSPLQRConfig qr;
        TSPLDMatrixConfig dmatrix;
    };

    HardwareSerial &_serial;
    Element _elements[MAX_ELEMENTS];
    uint8_t _elementCount = 0;

    void _flushRx();
    bool _appendPreamble(char *buf, size_t bufSize, size_t &offset) const;
    bool _appendElement(const Element &el, char *buf, size_t bufSize, size_t &offset) const;
};

#endif