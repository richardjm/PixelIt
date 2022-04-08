#include "Sensors.h"

Sensors::Sensors(Config &c)
{
    config = &c;
}

void Sensors::SetLogDelegate(void (*log)(String, String))
{
    _logDelegate = log;
}

void Sensors::SetBrightnessDelegate(void (*setBrightness)(float))
{
    _setBrightnessDelegate = setBrightness;
}

void Sensors::Log(String function, String message)
{
    if (0 != _logDelegate)
    {
        (*_logDelegate)(function, message);
    }
}

void Sensors::Initialise()
{

    // I2C Sensors
    twowire.begin(config->TranslatePin(config->SDAPin), config->TranslatePin(config->SCLPin));

    // Init LightSensor
    bh1750 = new BH1750();
    if (bh1750->begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &twowire))
    {
        Log(F("Setup"), F("BH1750 started"));
        luxSensor = LuxSensor_BH1750;
    }
    else
    {
        delete bh1750;
        max44009 = new Max44009(Max44009::Boolean::False);
        max44009->configure(MAX44009_DEFAULT_ADDRESS, &twowire, Max44009::Boolean::False);
        if (max44009->isConnected())
        {
            Log(F("Setup"), F("Max44009/GY-049 started"));
            luxSensor = LuxSensor_Max44009;
        }
        else
        {
            delete max44009;
            photocell = new LightDependentResistor(LDR_PIN, config->ldrPulldown, TranslatePhotocell(config->ldrDevice), 10, config->ldrSmoothing);
            photocell->setPhotocellPositionOnGround(false);
            luxSensor = LuxSensor_LDR;
        }
    }

    // Init Temp Sensors
    bme280 = new Adafruit_BME280();
    if (bme280->begin(BME280_ADDRESS_ALTERNATE, &twowire))
    {
        Log(F("Setup"), F("BME280 started"));
        tempSensor = TempSensor_BME280;
    }
    else
    {
        delete bme280;
        bmp280 = new Adafruit_BMP280(&twowire);
        Log(F("Setup"), F("BMP280 Trying"));
        if (bmp280->begin(BMP280_ADDRESS_ALT, 0x58))
        {
            Log(F("Setup"), F("BMP280 started"));
            tempSensor = TempSensor_BMP280;
        }
        else
        {
            delete bmp280;
            bme680 = new Adafruit_BME680(&twowire);
            if (bme680->begin())
            {
                Log(F("Setup"), F("BME680 started"));
                tempSensor = TempSensor_BME680;
            }
            else
            {
                delete bme680;
                // AM2320 needs a delay to be reliably initialized
                delay(800);
                dht = new DHTesp();
                dht->setup(config->TranslatePin(config->onewirePin), DHTesp::DHT22);
                if (!isnan(dht->getHumidity()) && !isnan(dht->getTemperature()))
                {
                    Log(F("Setup"), F("DHT started"));
                    tempSensor = TempSensor_DHT;
                }
                else
                {
                    Log(F("Setup"), F("No BMP280, BME280, BME 680 or DHT Sensor found"));
                    delete dht;
                }
            }
        }
    }
}

TempSensorData Sensors::GetTempSensorData()
{
    TempSensorData result;
    if (tempSensor == TempSensor_BME280)
    {
        result.temperature = bme280->readTemperature();
        result.humidity = bme280->readHumidity();
        result.pressure = bme280->readPressure() / 100.0F;
        result.hasGas = false;
    }
    else if (tempSensor == TempSensor_DHT)
    {
        result.temperature = dht->getTemperature();
        result.humidity = roundf(dht->getHumidity());
        result.hasPressure = false;
        result.hasGas = false;
    }
    else if (tempSensor == TempSensor_BME680)
    {
        /***************************************************************************************************
        // BME680 requires about 100ms for a read (heating the gas sensor). A blocking read can hinder
        // animations and scrolling. Therefore, we will use asynchronous reading in most cases.
        //
        // First call: starts measuring sequence, returns previous values.
        // Second call: performs read, returns current values.
        //
        // As long as there are more than ~200ms between the calls, there won't be blocking.
        // PixelIt usually uses a 3000ms loop.
        //
        // When there's no loop (no Websock connection, no MQTT) but only HTTP API calls, this would result
        // in only EVERY OTHER call return new values (which have been taken shortly after the previous call).
        // This is okay when you are polling very frequently, but might be undesirable when polling every
        // couple of minutes or so. Therefore: if previous reading is more than 20000ms old, perform
        // read in any case, even if it might become blocking.
        //
        // Please note: the gas value not only depends on gas, but also on the time since last read.
        // Frequent reads will yield higher values than infrequent reads. There will be a difference
        // even if we switch from 6secs to 3secs! So, do not attempt to compare values of readings
        // with an interval of 3 secs to values of readings with an interval of 60 secs!
        */

        const int elapsedSinceLastRead = millis() - lastBME680read;
        const int remain = bme680->remainingReadingMillis();

        if (remain == -1) // no current values available
        {
            bme680->beginReading(); // start measurement process
            // return previous values
            result.temperature = bme680->temperature;
            result.humidity = bme680->humidity;
            result.pressure = bme680->pressure / 100.0F;
            result.gas = bme680->gas_resistance / 1000.0F;
        }

        if (remain >= 0 || elapsedSinceLastRead > 20000)
        // remain==0: measurement completed, not read yet
        // remain>0: measurement still running, but as we already are in the next loop call, block and read
        // elapsedSinceLastRead>20000: obviously, remain==-1. But as there haven't been loop calls recently, this seems to be an "infrequent" API call. Perform blocking read.
        {
            if (bme680->endReading()) // will become blocking if measurement not complete yet
            {
                lastBME680read = millis();
                result.temperature = bme680->temperature;
                result.humidity = bme680->humidity;
                result.pressure = (bme680->pressure / 100.0F);
                result.gas = (bme680->gas_resistance / 1000.0F);
            }
            else
            {
                Log(F("GetTempSensorData"), F("Error while reading bme680"));
                result.hasTemperature = false;
                result.hasHumidity = false;
                result.hasPressure = false;
                result.hasGas = false;
            }
        }
    }
    else if (tempSensor == TempSensor_BMP280)
    {
        result.temperature = bmp280->readTemperature();
        // result.humidity = bmp280->readHumidity() + humidityOffset;
        result.hasHumidity = false;
        result.pressure = bmp280->readPressure() / 100.0F;
        result.hasGas = false;
    }

    else
    {
        result.hasTemperature = false;
        result.hasHumidity = false;
        result.hasPressure = false;
        result.hasGas = false;
    }

    // Add offset and do temperature conversion in one place
    if (result.hasTemperature)
    {
        result.temperature += config->temperatureOffset;
    }
    if (result.hasHumidity)
    {
        result.humidity += config->humidityOffset;
    }
    if (result.hasPressure)
    {
        result.pressure += config->pressureOffset;
    }
    if (result.hasGas)
    {
        result.gas += config->gasOffset;
    }

    if (result.hasTemperature && config->temperatureUnit == TemperatureUnit_Fahrenheit)
    {
        result.temperature = CelsiusToFahrenheit(result.temperature);
    }

    return result;
}

LuxSensorData Sensors::GetLuxSensorData()
{
    LuxSensorData result;

    if (luxSensor == LuxSensor_BH1750)
    {
        result.lux = bh1750->readLightLevel();
    }
    else if (luxSensor == LuxSensor_Max44009)
    {
        result.lux = max44009->getLux();
    }
    else
    {
        result.lux = roundf(photocell->getSmoothedLux() * 1000) / 1000;
    }

    result.lux += config->luxOffset;
    LastLux = result;
    return result;
}

/// <summary>
/// Convert celsius to fahrenheit
/// </summary>
float Sensors::CelsiusToFahrenheit(float celsius)
{
    return (celsius * 9 / 5) + 32;
}

LightDependentResistor::ePhotoCellKind Sensors::TranslatePhotocell(String photocell)
{
    if (photocell == "GL5516")
        return LightDependentResistor::GL5516;
    if (photocell == "GL5528")
        return LightDependentResistor::GL5528;
    if (photocell == "GL5537_1")
        return LightDependentResistor::GL5537_1;
    if (photocell == "GL5537_2")
        return LightDependentResistor::GL5537_2;
    if (photocell == "GL5539")
        return LightDependentResistor::GL5539;
    if (photocell == "GL5549")
        return LightDependentResistor::GL5549;

    Log(F("TranslatePhotocell"), F("Unknown LDR-Typ"));
    return LightDependentResistor::GL5528;
}