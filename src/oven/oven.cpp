#include "oven.h"

Oven::Oven(uint8_t scl, uint8_t sda, uint8_t addr)
    : _scl(scl), _sda(sda), _addr(addr)
{
}
Oven::~Oven()
{
}
void Oven::begin()
{
    _initialized = true;
    OVEN_INFO("Oven initialized (SCL=%d, SDA=%d, ADDR=0x%02X)\n", _scl, _sda, _addr);

    _pcf = new PCF8574(_scl, _sda, _addr);

    _pcf->pinMode(OVEN_P0_FAN12V, OUTPUT);
    _pcf->pinMode(OVEN_P1_FAN230V, OUTPUT);
    _pcf->pinMode(OVEN_P2_FAN230V_SLOW, OUTPUT);
    _pcf->pinMode(OVEN_P3_LAMP, OUTPUT);
    _pcf->pinMode(OVEN_P4_SILICAT_MOTOR, OUTPUT);
    _pcf->pinMode(OVEN_P5_DOOR_SENSOR, OUTPUT);
    _pcf->pinMode(OVEN_P6_HEATER, OUTPUT);

    if (_pcf->begin())
    {
        OVEN_INFO("PCF8574 connected at address 0x%02X\n", _addr);
    }
    else
    {
        OVEN_ERR("PCF8574 not found at address 0x%02X\n", _addr);
    }
}

//----------------------------------------------------------
// Temperatur-Methoden
//----------------------------------------------------------
void Oven::setTemperature(float temperature)
{
    _currentTemperature = temperature;
}
float Oven::getTemperature() const
{
    return _currentTemperature;
}
void Oven::setTargetTemperature(float target)
{
    _targetTemperature = target;
}
float Oven::getTargetTemperature() const
{
    return _targetTemperature;
}

//----------------------------------------------------------
// FAN12V-Methoden
//----------------------------------------------------------
void Oven::fan12VOn()
{
    _fan12VState = true;
    _pcf->digitalWrite(OVEN_P0_FAN12V, HIGH);
    delay(100);
    OVEN_INFO("FAN12V turned on\n");
}
void Oven::fan12VOff()
{
    _fan12VState = false;
    _pcf->digitalWrite(OVEN_P0_FAN12V, LOW);
    delay(100);
    OVEN_INFO("FAN12V turned off\n");
}

//----------------------------------------------------------
// FAN230V-Methoden
//----------------------------------------------------------
void Oven::fan230VOn()
{
    _fan230VState = true;
    _pcf->digitalWrite(OVEN_P1_FAN230V, HIGH);
    delay(100);
    OVEN_INFO("FAN230V turned on\n");
}

void Oven::fan230VOff()
{
    _fan230VState = false;
    _pcf->digitalWrite(OVEN_P1_FAN230V, LOW);
    delay(100);
    OVEN_INFO("FAN230V turned off\n");
}

void Oven::fan230VSlowOn()
{
    _fan230VState = true;
    _pcf->digitalWrite(OVEN_P2_FAN230V_SLOW, HIGH);
    delay(100);
    OVEN_INFO("FAN230V-slow turned on\n");
}
void Oven::fan230VSlowOff()
{
    _fan230VState = false;
    _pcf->digitalWrite(OVEN_P2_FAN230V_SLOW, LOW);
    delay(100);
    OVEN_INFO("FAN230V-slow turned off\n");
}

//----------------------------------------------------------
// LAMP-Methoden
//----------------------------------------------------------
void Oven::lampOn()
{
    _lampState = true;
    _pcf->digitalWrite(OVEN_P3_LAMP, HIGH);
    delay(100);
    OVEN_INFO("Lamp turned on\n");
}

void Oven::lampOff()
{
    _lampState = false;
    _pcf->digitalWrite(OVEN_P3_LAMP, LOW);
    delay(100);
    OVEN_INFO("Lamp turned off\n");
}

//----------------------------------------------------------
// HEATER-Methoden
//----------------------------------------------------------
void Oven::heaterOn()
{
    _heaterState = true;
    _pcf->digitalWrite(OVEN_P6_HEATER, HIGH);
    delay(100);
    OVEN_INFO("Heater turned on\n");
}
void Oven::heaterOff()
{
    _heaterState = false;
    _pcf->digitalWrite(OVEN_P6_HEATER, LOW);
    delay(100);
    OVEN_INFO("Heater turned off\n");
}

//----------------------------------------------------------
// SilicatMotor-Methoden
//----------------------------------------------------------
void Oven::silicatMotorOn()
{
    _silicatMotorState = true;
    _pcf->digitalWrite(OVEN_P4_SILICAT_MOTOR, HIGH);
    delay(100);
    OVEN_INFO("Silicat motor turned on\n");
}
void Oven::silicatMotorOff()
{
    _silicatMotorState = false;
    _pcf->digitalWrite(OVEN_P4_SILICAT_MOTOR, LOW);
    delay(100);
    OVEN_INFO("Silicat motor turned off\n");
}

//----------------------------------------------------------
// DOOR-Methoden
//----------------------------------------------------------
void Oven::setDoorClosed()
{
    _doorClosed = true;
    _doorOpen = !_doorClosed;
    _pcf->digitalWrite(OVEN_P5_DOOR_SENSOR, HIGH);
    delay(100);
    OVEN_INFO("DOOR closed\n");
}
void Oven::setDoorOpen()
{
    _doorClosed = false;
    _doorOpen = !_doorClosed;
    OVEN_INFO("DOOR opened\n");
}

bool Oven::isDoorClosed() const
{
    OVEN_INFO("isDoorClosed=%s\n", _doorClosed ? "true" : "false");
    return _doorClosed;
}
bool Oven::isDoorOpen() const
{
    OVEN_INFO("isDoorOpen=%s\n", _doorOpen ? "true" : "false");
    return _doorOpen;
}
