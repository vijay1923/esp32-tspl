// BasicQRPrint - minimal example for the TSPLPrinter library.
//
// Wire the printer's RS232/TTL RX/TX to the pins set below, adjust them to
// match your board, then upload. Sends one status poll and one QR label on
// boot.

#include <TSPL_Printer.h>

// Adjust to your wiring. Use whichever HardwareSerial instance is free on
// your board (avoid one already used by another peripheral).
#define PRINTER_RX_PIN 17
#define PRINTER_TX_PIN 35

TSPLPrinter printer(Serial2);

void setup()
{
    Serial.begin(115200);
    printer.begin(9600, PRINTER_RX_PIN, PRINTER_TX_PIN);

    // Optional: adjust label/QR settings before printing. Defaults are a
    // 60mm x 40mm label with the QR positioned at (100,100) dots.
    printer.config.size_width_mm = 60.0f;
    printer.config.size_height_mm = 40.0f;
    printer.config.qr_x_dots = 100;
    printer.config.qr_y_dots = 100;
    printer.config.qr_cell_width = 4;
    printer.config.qr_ecc = 'H';

    // Check the printer is ready before printing.
    TSPLPrinterStatus status;
    if (printer.pollStatus(status))
    {
        Serial.print("Printer status: ");
        Serial.println(TSPLPrinter::statusToText(status));

        if (status == TSPLPrinterStatus::Normal)
        {
            if (printer.printQR("HELLO-WORLD-001"))
            {
                Serial.println("QR job sent.");
            }
            else
            {
                Serial.println("Failed to build QR job.");
            }
        }
        else
        {
            Serial.println("Printer not ready - skipping print.");
        }
    }
    else
    {
        Serial.println("No response from printer.");
    }
}

void loop()
{
}
