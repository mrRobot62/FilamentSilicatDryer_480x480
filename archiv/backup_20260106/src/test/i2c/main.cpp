#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP9600.h>
#include <PCF8575.h> // robtillaart/PCF8575 @ ^0.2.4

// ==== I2C pin configuration for ESP32-S3 ====
static const int I2C_SDA_PIN = 40; // IO02
static const int I2C_SCL_PIN = 2;  // IO40

// ==== I2C device addresses ====
static const uint8_t MCP9600_ADDR = 0x67; // default
static const uint8_t PCF8575_ADDR = 0x20; // default
static const uint8_t ADS1115_ADDR = 0x48; // default

// ==== Objects ====
Adafruit_MCP9600 mcp;
PCF8575 pcf(PCF8575_ADDR, &Wire);

// ==== Timing ====
unsigned long lastMcpReadMs = 0;
const unsigned long MCP_PERIOD_MS = 1000;

unsigned long lastPcfToggleMs = 0;
const unsigned long PCF_PERIOD_MS = 1000;

// Optional: scan I2C bus
void scanI2CBus()
{
    Serial.println(F("I2C scan started..."));
    for (uint8_t addr = 1; addr < 127; addr++)
    {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
        {
            Serial.print(F("  Found I2C device at 0x"));
            Serial.println(addr, HEX);
        }
    }
    Serial.println(F("I2C scan done."));
}

void setup()
{
    Serial.begin(115200);
    delay(4000); // damit du sicher die Setup-Logs siehst

    Serial.println(F("=== ESP32-S3 I2C Test: PCF8575(0x20), ADS1115(0x48), MPC9600(0x67) ==="));

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);

    scanI2CBus();

    // ---- PCF8575 ----
    if (!pcf.begin())
    {
        Serial.println(F("ERROR: PCF8575 not found!"));
    }
    else
    {
        Serial.println(F("PCF8575 init OK."));
        pcf.write16(0xFFFF); // all high
    }

    // ---- MCP9600 ----
    if (!mcp.begin(MCP9600_ADDR, &Wire))
    {
        Serial.println(F("ERROR: MCP9600 not found!"));
        while (1)
            delay(100);
    }

    Serial.println(F("MCP9600 init OK. Configuring..."));

    mcp.setThermocoupleType(MCP9600_TYPE_K);
    mcp.setADCresolution(MCP9600_ADCRESOLUTION_18);
    mcp.setFilterCoefficient(3);
    mcp.enable(true);

    Serial.println(F("Setup complete.\n"));
}

void loop()
{
    unsigned long now = millis();

    // ---- MCP9600: read temperature every 1s ----
    if (now - lastMcpReadMs >= MCP_PERIOD_MS)
    {
        lastMcpReadMs = now;

        double hot = mcp.readThermocouple();
        double cold = mcp.readAmbient();

        Serial.print(F("[MCP9600] TH="));
        Serial.print(hot, 2);
        Serial.print(F(" °C | CJ="));
        Serial.print(cold, 2);
        Serial.println(F(" °C"));
    }

    // ---- PCF8575: toggle P0 every 1s ----
    if (now - lastPcfToggleMs >= PCF_PERIOD_MS)
    {
        lastPcfToggleMs = now;

        pcf.toggle(0); // toggle bit 0
        uint16_t val = pcf.read16();

        Serial.print(F("[PCF8575] PINS=0x"));
        Serial.println(val, HEX);
    }
}