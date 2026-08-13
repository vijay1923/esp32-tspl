#include "TSPL_Printer.h"
#include <stdarg.h>
#include <string.h>

// Status query command: ESC ! ?  - sent as a single write including the
// trailing null byte (4 bytes total). This exact form - rather than writing
// the 3 command bytes individually - is what was confirmed reliable on the
// actual printer hardware during bench testing; kept as-is intentionally.
static const char STATUS_QUERY_CMD[] = "\x1B!?";

TSPLPrinter::TSPLPrinter(HardwareSerial &serialPort)
    : _serial(serialPort)
{
}

void TSPLPrinter::begin(unsigned long baud, int8_t rxPin, int8_t txPin)
{
    _serial.begin(baud, SERIAL_8N1, rxPin, txPin);
}

void TSPLPrinter::_flushRx()
{
    while (_serial.available() > 0)
    {
        (void)_serial.read();
    }
}

static void formatMm(float mm, char *out, size_t outSize)
{
    // TSPL wants "W.D mm" (one decimal place)
    int whole = (int)mm;
    int tenths = (int)((mm - whole) * 10.0f + 0.5f);
    if (tenths >= 10) { tenths = 0; whole += 1; }
    snprintf(out, outSize, "%d.%d", whole, tenths);
}

// Appends a printf-style formatted command onto buf at the given offset,
// bounds-checked against bufSize. Advances offset only on success, leaves
// buf/offset untouched (aside from the null terminator snprintf already
// wrote) on failure so callers can safely bail out.
static bool appendFmt(char *buf, size_t bufSize, size_t &offset, const char *fmt, ...)
{
    if (buf == nullptr || offset >= bufSize)
    {
        return false;
    }

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + offset, bufSize - offset, fmt, args);
    va_end(args);

    if (n < 0 || (size_t)n >= (bufSize - offset))
    {
        return false;
    }

    offset += (size_t)n;
    return true;
}

// Escapes a content string for embedding inside a quoted TSPL parameter.
// Always escapes '"' as \["] per the manual. If escapeTilde is true, also
// escapes '~' as '~~' - required for DMATRIX content only, since '~' opens
// DMATRIX's own control-sequence mini-language (not needed for TEXT/QR).
static bool escapeContent(const char *in, char *out, size_t outSize, bool escapeTilde)
{
    if (in == nullptr || out == nullptr || outSize == 0)
    {
        return false;
    }

    size_t outPos = 0;

    for (size_t i = 0; in[i] != '\0'; i++)
    {
        const char *replacement = nullptr;
        size_t replacementLen = 0;

        if (in[i] == '"')
        {
            replacement = "\\[\"]";
            replacementLen = 4;
        }
        else if (escapeTilde && in[i] == '~')
        {
            replacement = "~~";
            replacementLen = 2;
        }

        if (replacement != nullptr)
        {
            if (outPos + replacementLen >= outSize)
            {
                return false;
            }
            memcpy(out + outPos, replacement, replacementLen);
            outPos += replacementLen;
        }
        else
        {
            if (outPos + 1 >= outSize)
            {
                return false;
            }
            out[outPos++] = in[i];
        }
    }

    out[outPos] = '\0';
    return true;
}

bool TSPLPrinter::printQR(const char *data)
{
    if (data == nullptr || data[0] == '\0')
    {
        return false;
    }

    char sizeW[12], sizeH[12], gapW[12], gapH[12];
    formatMm(config.size_width_mm, sizeW, sizeof(sizeW));
    formatMm(config.size_height_mm, sizeH, sizeof(sizeH));
    formatMm(config.gap_width_mm, gapW, sizeof(gapW));
    formatMm(config.gap_height_mm, gapH, sizeof(gapH));

    char jobBuf[512];
    int n = snprintf(
        jobBuf, sizeof(jobBuf),
        "SIZE %s mm,%s mm\r\n"
        "GAP %s mm,%s mm\r\n"
        "DIRECTION %u\r\n"
        "CLS\r\n"
        "QRCODE %u,%u,%c,%u,%c,%u,\"%s\"\r\n"
        "PRINT %u,%u\r\n",
        sizeW, sizeH, gapW, gapH,
        (unsigned)config.direction,
        (unsigned)config.qr_x_dots, (unsigned)config.qr_y_dots,
        config.qr_ecc, (unsigned)config.qr_cell_width,
        config.qr_mode, (unsigned)config.qr_rotation,
        data,
        (unsigned)config.print_sets, (unsigned)config.print_copies);

    if (n <= 0 || (size_t)n >= sizeof(jobBuf))
    {
        return false; // data too long / buffer too small for this job
    }

    _serial.write((const uint8_t *)jobBuf, (size_t)n);
    _serial.flush();
    return true;
}

// ---------------------------------------------------------------------------
// Multi-element label building
// ---------------------------------------------------------------------------

void TSPLPrinter::beginLabel()
{
    _elementCount = 0;
}

bool TSPLPrinter::addText(const char *data, TSPLTextConfig cfg)
{
    if (data == nullptr || data[0] == '\0')
    {
        return false;
    }
    if (_elementCount >= MAX_ELEMENTS)
    {
        return false;
    }
    if (strlen(data) >= MAX_ELEMENT_DATA_LEN)
    {
        return false;
    }

    Element &el = _elements[_elementCount];
    el.type = ElementType::Text;
    strncpy(el.data, data, sizeof(el.data) - 1);
    el.data[sizeof(el.data) - 1] = '\0';
    el.text = cfg;

    _elementCount++;
    return true;
}

bool TSPLPrinter::addQR(const char *data, TSPLQRConfig cfg)
{
    if (data == nullptr || data[0] == '\0')
    {
        return false;
    }
    if (_elementCount >= MAX_ELEMENTS)
    {
        return false;
    }
    if (strlen(data) >= MAX_ELEMENT_DATA_LEN)
    {
        return false;
    }

    Element &el = _elements[_elementCount];
    el.type = ElementType::QR;
    strncpy(el.data, data, sizeof(el.data) - 1);
    el.data[sizeof(el.data) - 1] = '\0';
    el.qr = cfg;

    _elementCount++;
    return true;
}

bool TSPLPrinter::addDataMatrix(const char *data, TSPLDMatrixConfig cfg)
{
    if (data == nullptr || data[0] == '\0')
    {
        return false;
    }
    if (_elementCount >= MAX_ELEMENTS)
    {
        return false;
    }
    if (strlen(data) >= MAX_ELEMENT_DATA_LEN)
    {
        return false;
    }
    // row/col must be set together, and only within the printer's supported range.
    if ((cfg.rows > 0) != (cfg.cols > 0))
    {
        return false;
    }
    if (cfg.rows > 0 && (cfg.rows < 10 || cfg.rows > 144 || cfg.cols < 10 || cfg.cols > 144))
    {
        return false;
    }

    Element &el = _elements[_elementCount];
    el.type = ElementType::DMatrix;
    strncpy(el.data, data, sizeof(el.data) - 1);
    el.data[sizeof(el.data) - 1] = '\0';
    el.dmatrix = cfg;

    _elementCount++;
    return true;
}

bool TSPLPrinter::_appendPreamble(char *buf, size_t bufSize, size_t &offset) const
{
    char sizeW[12], sizeH[12], gapW[12], gapH[12];
    formatMm(config.size_width_mm, sizeW, sizeof(sizeW));
    formatMm(config.size_height_mm, sizeH, sizeof(sizeH));
    formatMm(config.gap_width_mm, gapW, sizeof(gapW));
    formatMm(config.gap_height_mm, gapH, sizeof(gapH));

    if (!appendFmt(buf, bufSize, offset, "SIZE %s mm,%s mm\r\n", sizeW, sizeH)) return false;
    if (!appendFmt(buf, bufSize, offset, "GAP %s mm,%s mm\r\n", gapW, gapH)) return false;
    if (!appendFmt(buf, bufSize, offset, "DIRECTION %u\r\n", (unsigned)config.direction)) return false;
    // CLS must come after SIZE per the manual - preamble order here
    // guarantees that regardless of what GAP/DIRECTION do.
    if (!appendFmt(buf, bufSize, offset, "CLS\r\n")) return false;

    return true;
}

bool TSPLPrinter::_appendElement(const Element &el, char *buf, size_t bufSize, size_t &offset) const
{
    char escaped[MAX_ELEMENT_DATA_LEN + 8]; // headroom for \["] / ~~ expansion

    switch (el.type)
    {
        case ElementType::Text:
        {
            if (!escapeContent(el.data, escaped, sizeof(escaped), false)) return false;

            if (el.text.use_alignment)
            {
                return appendFmt(
                    buf, bufSize, offset,
                    "TEXT %u,%u,\"%s\",%u,%u,%u,%u,\"%s\"\r\n",
                    (unsigned)el.text.x, (unsigned)el.text.y, el.text.font,
                    (unsigned)el.text.rotation, (unsigned)el.text.x_mult, (unsigned)el.text.y_mult,
                    (unsigned)el.text.alignment, escaped);
            }
            return appendFmt(
                buf, bufSize, offset,
                "TEXT %u,%u,\"%s\",%u,%u,%u,\"%s\"\r\n",
                (unsigned)el.text.x, (unsigned)el.text.y, el.text.font,
                (unsigned)el.text.rotation, (unsigned)el.text.x_mult, (unsigned)el.text.y_mult,
                escaped);
        }

        case ElementType::QR:
        {
            if (!escapeContent(el.data, escaped, sizeof(escaped), false)) return false;

            return appendFmt(
                buf, bufSize, offset,
                "QRCODE %u,%u,%c,%u,%c,%u,\"%s\"\r\n",
                (unsigned)el.qr.x, (unsigned)el.qr.y, el.qr.ecc, (unsigned)el.qr.cell_width,
                el.qr.mode, (unsigned)el.qr.rotation, escaped);
        }

        case ElementType::DMatrix:
        {
            if (!escapeContent(el.data, escaped, sizeof(escaped), true)) return false;

            if (!appendFmt(buf, bufSize, offset, "DMATRIX %u,%u,%u,%u,",
                    (unsigned)el.dmatrix.x, (unsigned)el.dmatrix.y,
                    (unsigned)el.dmatrix.width, (unsigned)el.dmatrix.height))
            {
                return false;
            }
            if (el.dmatrix.module_size > 0)
            {
                if (!appendFmt(buf, bufSize, offset, "x%u,", (unsigned)el.dmatrix.module_size)) return false;
            }
            if (el.dmatrix.rotation != 0)
            {
                if (!appendFmt(buf, bufSize, offset, "r%u,", (unsigned)el.dmatrix.rotation)) return false;
            }
            if (el.dmatrix.shape != 0)
            {
                if (!appendFmt(buf, bufSize, offset, "a%u,", (unsigned)el.dmatrix.shape)) return false;
            }
            if (el.dmatrix.rows > 0 && el.dmatrix.cols > 0)
            {
                if (!appendFmt(buf, bufSize, offset, "%u,%u,",
                        (unsigned)el.dmatrix.rows, (unsigned)el.dmatrix.cols))
                {
                    return false;
                }
            }
            return appendFmt(buf, bufSize, offset, "\"%s\"\r\n", escaped);
        }
    }

    return false; // unreachable - all enum cases handled above
}

bool TSPLPrinter::printLabel()
{
    if (_elementCount == 0)
    {
        return false;
    }

    char jobBuf[JOB_BUF_SIZE];
    size_t offset = 0;

    if (!_appendPreamble(jobBuf, sizeof(jobBuf), offset))
    {
        return false;
    }

    for (uint8_t i = 0; i < _elementCount; i++)
    {
        if (!_appendElement(_elements[i], jobBuf, sizeof(jobBuf), offset))
        {
            return false;
        }
    }

    if (!appendFmt(jobBuf, sizeof(jobBuf), offset, "PRINT %u,%u\r\n",
            (unsigned)config.print_sets, (unsigned)config.print_copies))
    {
        return false;
    }

    _serial.write((const uint8_t *)jobBuf, offset);
    _serial.flush();
    return true;
}

// ---------------------------------------------------------------------------
// Status polling
// ---------------------------------------------------------------------------

bool TSPLPrinter::pollStatus(TSPLPrinterStatus &status, unsigned long timeoutMs)
{
    _flushRx();

    _serial.write((const uint8_t *)STATUS_QUERY_CMD, sizeof(STATUS_QUERY_CMD));

    unsigned long start = millis();
    while ((millis() - start) < timeoutMs)
    {
        if (_serial.available() > 0)
        {
            status = (TSPLPrinterStatus)_serial.read();
            return true;
        }
    }

    return false; // no response within timeout
}

const char *TSPLPrinter::statusToText(TSPLPrinterStatus status)
{
    switch (status)
    {
        case TSPLPrinterStatus::Normal: return "Normal";
        case TSPLPrinterStatus::HeadOpen: return "Head Open";
        case TSPLPrinterStatus::PaperJam: return "Paper Jam";
        case TSPLPrinterStatus::PaperJamHeadOpen: return "Paper Jam, Head Open";
        case TSPLPrinterStatus::OutOfPaper: return "Out of Paper";
        case TSPLPrinterStatus::OutOfPaperHeadOpen: return "Out of Paper, Head Open";
        case TSPLPrinterStatus::OutOfRibbon: return "Out of Ribbon";
        case TSPLPrinterStatus::OutOfRibbonHeadOpen: return "Out of Ribbon, Head Open";
        case TSPLPrinterStatus::OutOfRibbonPaperJam: return "Out of Ribbon, Paper Jam";
        case TSPLPrinterStatus::OutOfRibbonPaperJamHeadOpen: return "Out of Ribbon, Paper Jam, Head Open";
        case TSPLPrinterStatus::OutOfRibbonOutOfPaper: return "Out of Ribbon, Out of Paper";
        case TSPLPrinterStatus::OutOfRibbonOutOfPaperHeadOpen: return "Out of Ribbon, Out of Paper, Head Open";
        case TSPLPrinterStatus::Paused: return "Paused";
        case TSPLPrinterStatus::Printing: return "Printing";
        case TSPLPrinterStatus::OtherError: return "Other Error";
        default: return "Unknown Status";
    }
}