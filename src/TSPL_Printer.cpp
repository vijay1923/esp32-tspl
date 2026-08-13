#include "TSPL_Printer.h"

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
