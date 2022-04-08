#ifndef PixelIt_TemperatureSensor_h
#define PixelIt_TemperatureSensor_h
#include <Arduino.h>
#include <Wire.h>
#include "Config.h"

// Temperature
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_BME680.h>
#include <DHTesp.h>

// Lux
#include <BH1750.h>
#include <Max44009.h>
#include <LightDependentResistor.h>

//// LDR Config
#define LDR_PIN A0


enum TempSensor
{
	TempSensor_None,
	TempSensor_BME280,
	TempSensor_DHT,
	TempSensor_BME680,
	TempSensor_BMP280,
};

enum LuxSensor
{
	LuxSensor_LDR,
	LuxSensor_BH1750,
	LuxSensor_Max44009,
};

struct TempSensorData {
    bool hasTemperature = true;
    float temperature;

    bool hasHumidity = true;
    float humidity;

    bool hasPressure = true;
    float pressure;

    bool hasGas = true;
    float gas;
};

struct LuxSensorData {
    bool hasLux = true;
    float lux;
};

class Sensors {
    public:
        Sensors(Config &c);

        void Initialise();

        TempSensor tempSensor = TempSensor_None;
        LuxSensor luxSensor = LuxSensor_LDR;

        TempSensorData GetTempSensorData();

        LuxSensorData GetLuxSensorData();
        LuxSensorData LastLux;

        void SetLogDelegate(void (*log)(String, String));
        void SetBrightnessDelegate(void (*setBrightness)(float));
    private:
        Config *config;

        #if defined(ESP8266)
        TwoWire twowire;
        #elif defined(ESP32)
        TwoWire twowire(BME280_ADDRESS_ALTERNATE);
        #endif

        // Temperature
        Adafruit_BME280 *bme280;
        Adafruit_BMP280 *bmp280;
        Adafruit_BME680 *bme680;
        unsigned long lastBME680read = 0;
        DHTesp *dht;

        // Lux
        LightDependentResistor *photocell;
        BH1750 *bh1750;
        Max44009 *max44009;

        void (*_logDelegate)(String, String) = 0;
        void (*_setBrightnessDelegate)(float) = 0;

        void Log(String function, String Message);

        float CelsiusToFahrenheit(float celsius);
        LightDependentResistor::ePhotoCellKind TranslatePhotocell(String photocell);
};

#endif