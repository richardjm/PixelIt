#ifndef Config_h
#define Config_h

#include <Arduino.h>
#include <ArduinoJson.h>

enum TemperatureUnit
{
	TemperatureUnit_Celsius,
	TemperatureUnit_Fahrenheit
};

enum btnActions
{
	btnAction_DoNothing = 0,
	btnAction_GotoClock = 1,
	btnAction_ToggleSleepMode = 2,
	btnAction_MP3PlayPause = 3,
	btnAction_MP3PlayPrevious = 4,
	btnAction_MP3PlayNext = 5,
};

class Config {
    public:
        Config(String version, bool isESP8266);
        
        void SaveConfig();
        void SetConfig(JsonObject &json);
        void LoadConfig();

        void SetLogDelegate(void (*log)(String, String));
        void SetBrightnessDelegate(void (*setBrightness)(float));

        uint8_t TranslatePin(String pin);

        TemperatureUnit temperatureUnit;

        // Matrix Vars
        int matrixBrightness = 127;
        bool matrixBrightnessAutomatic = true;
        int mbaDimMin = 20;
        int mbaDimMax = 100;
        int mbaLuxMin = 0;
        int mbaLuxMax = 400;
        int matrixType = 1;
        String note;
        String hostname;
        String matrixTempCorrection = "default";

        // System Vars
        bool sleepMode = false;
        bool bootScreenAktiv = true;
        String optionsVersion = "";

        //// MQTT Config
        bool mqttAktiv = false;
        String mqttUser = "";
        String mqttPassword = "";
        String mqttServer = "";
        String mqttMasterTopic = "Haus/PixelIt/";
        int mqttPort = 1883;

        // Timerserver Vars
        String ntpServer = "de.pool.ntp.org";

        // Clock Vars
        bool clockBlink = false;
        bool clockAktiv = true;
        bool clock24Hours = true;
        bool clockDayLightSaving = true;
        bool clockSwitchAktiv = true;
        bool clockWithSeconds = false;
        bool clockAutoFallbackActive = false;
        uint clockAutoFallbackAnimation = 1;
        uint clockSwitchSec = 7;
        uint clockCounterClock = 0;
        uint clockCounterDate = 0;
        float clockTimeZone = 1;
        uint8_t clockColorR = 255, clockColorG = 255, clockColorB = 255;
        uint clockAutoFallbackTime = 30;
        bool clockDateDayMonth = true;
        bool clockDayOfWeekFirstMonday = true;
        bool forceClock = false;

        // Scrolltext Vars
        bool scrollTextAktivLoop = false;
        uint scrollTextDefaultDelay = 100;
        uint scrollTextDelay;
        int scrollPos;
        int scrollposY;
        bool scrollwithBMP;
        int scrollxPixelText;
        String scrollTextString;

        // Sensor Vars
        float currentLux = 0.0f;
        float luxOffset = 0.0f;
        float temperatureOffset = 0.0f;
        float humidityOffset = 0.0f;
        float pressureOffset = 0.0f;
        float gasOffset = 0.0f;

        uint initialVolume = 10;

        // PIN Vars
        String dfpRXPin = "Pin_D7";
        String dfpTXPin = "Pin_D8";
        String onewirePin = "Pin_D1";
        String SCLPin = "Pin_D1";
        String SDAPin = "Pin_D3";
        String ldrDevice = "GL5516";
        unsigned long ldrPulldown = 10000; // 10k pulldown-resistor
        unsigned int ldrSmoothing = 0;

        String btnPin[3] = {"Pin_D0", "Pin_D4", "Pin_D5"};
        bool btnEnabled[3] = {false, false, false};
        int btnPressedLevel[3] = {LOW, LOW, LOW};


        btnActions btnAction[3] = {btnAction_ToggleSleepMode, btnAction_GotoClock, btnAction_DoNothing};

    private:
        void SetConfigVariables(JsonObject &json);
        static String _version;
        static bool _isESP8266;

        void (*_logDelegate)(String, String) = 0;
        void (*_setBrightnessDelegate)(float) = 0;

        void Log(String function, String Message);
};
#endif