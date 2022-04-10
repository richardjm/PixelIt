#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <FS.h>
#endif

// WiFi & Web
#include <ESPAsyncWebServer.h>
#include <ESPAsync_WiFiManager.h>
#include <WebSocketsServer.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
// MQTT
#include <PubSubClient.h>
// Matrix
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
// Misc
#include <DFPlayerMini_Fast.h>
#include <SoftwareSerial.h>
#include "ColorConverterLib.h"
#include <TimeLib.h>
#include <ArduinoJson.h>
// PixelIT Stuff
#include "PixelItFont.h"
#include "Webinterface.h"
#include "Tools.h"
#include "Config.h"
#include "Sensors.h"

#define VERSION "0.4.00"

void FadeOut(int = 10, int = 0);
void FadeIn(int = 10, int = 0);

class SetGPIO
{
public:
    int gpio;
    ulong resetMillis;
};
#define SET_GPIO_SIZE 4
SetGPIO setGPIOReset[SET_GPIO_SIZE];

//// MQTT Config
bool mqttAktiv = false;
String mqttUser = "";
String mqttPassword = "";
String mqttServer = "";
String mqttMasterTopic = "Haus/PixelIt/";
int mqttPort = 1883;
unsigned long mqttLastReconnectAttempt = 0; // will store last time reconnect to mqtt broker
const int MQTT_RECONNECT_INTERVAL = 15000;
//#define MQTT_MAX_PACKET_SIZE 8000

//// LDR Config
#define LDR_PIN A0

//// GPIO Config
#if defined(ESP8266)
#define MATRIX_PIN D2
#elif defined(ESP32)
#define MATRIX_PIN 27
#endif

#if defined(ESP8266)
bool isESP8266 = true;
#else
bool isESP8266 = false;
#endif

Config config = Config(VERSION, isESP8266);
Sensors sensors = Sensors(config);

enum btnStates
{
    btnState_Released,
    btnState_PressedNew,
    btnState_PressedBefore,
};
btnStates btnState[] = {btnState_Released, btnState_Released, btnState_Released};
unsigned long btnLastPressedMillis[] = {0, 0, 0};

#define NUMMATRIX (32 * 8)
CRGB leds[NUMMATRIX];

FastLED_NeoMatrix *matrix;
WiFiClient espClient;
WiFiUDP udp;
PubSubClient client(espClient);

AsyncWebServer server(80);
DNSServer dns;
ESPAsync_WiFiManager wifiManager(&server, &dns);

WebSocketsServer webSocket = WebSocketsServer(81);
DFPlayerMini_Fast mp3Player;
SoftwareSerial *softSerial;
uint initialVolume = 10;

// Millis timestamp of the last receiving screen
unsigned long lastScreenMessageMillis = 0;

// Bmp Vars
uint16_t bmpArray[64];

// Timerserver Vars
time_t clockLastUpdate;
uint ntpRetryCounter = 0;
unsigned long ntpTimeOut = 0;
#define NTP_MAX_RETRYS 3
#define NTP_TIMEOUT_SEC 60

// Animate BMP Vars
uint16_t animationBmpList[10][64];
bool animateBMPAktivLoop = false;
unsigned long animateBMPPrevMillis = 0;
int animateBMPCounter = 0;
bool animateBMPReverse = false;
bool animateBMPRubberbandingAktiv = false;
uint animateBMPDelay;
int animateBMPLimitLoops = -1;
int animateBMPLoopCount = 0;
int animateBMPLimitFrames = -1;
int animateBMPFrameCount = 0;
unsigned long scrollTextPrevMillis = 0;

// Sensors Vars
unsigned long sendLuxPrevMillis = 0;
unsigned long sendSensorPrevMillis = 0;
unsigned long sendInfoPrevMillis = 0;
String oldGetMatrixInfo;
String oldGetLuxSensor;
String oldGetSensor;

// MP3Player Vars
String OldGetMP3PlayerInfo;

// Websoket Vars
String websocketConnection[10];

void SetCurrentMatrixBrightness(float newBrightness)
{
    matrix->setBrightness(newBrightness);
}

void EnteredHotspotCallback(ESPAsync_WiFiManager *myWiFiManager)
{
    Log(F("Hotspot"), "Waiting for WiFi configuration");
    matrix->clear();
    DrawTextHelper("HOTSPOT", false, false, false, false, false, false, NULL, 255, 255, 255, 3, 1);
    FadeIn();
}

void HandleScreen(AsyncWebServerRequest *request)
{
    DynamicJsonBuffer jsonBuffer;
    String args = String(request->arg("plain").c_str());
    JsonObject &json = jsonBuffer.parseObject(args.begin());

    // server.sendHeader(F("Connection"), F("close"));
    // server.sendHeader(F("Access-Control-Allow-Origin"), "*");

    if (json.success())
    {
        request->send(200, F("application/json"), F("{\"response\":\"OK\"}"));
        Log(F("HandleScreen"), "Incoming Json length: " + String(json.measureLength()));
        CreateFrames(json);
    }
    else
    {
        request->send(406, F("application/json"), F("{\"response\":\"Not Acceptable\"}"));
    }
}

String GetSensor()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    TempSensorData tempData = sensors.GetTempSensorData();
    if (tempData.hasTemperature)
    {
        root["temperature"] = tempData.temperature;
    }
    else
    {
        root["temperature"] = "Not installed";
    }

    if (tempData.hasHumidity)
    {
        root["humidity"] = tempData.humidity;
    }
    else
    {
        root["humidity"] = "Not installed";
    }

    if (tempData.hasPressure)
    {
        root["pressure"] = tempData.pressure;
    }
    else
    {
        root["pressure"] = "Not installed";
    }

    if (tempData.hasGas)
    {
        root["gas"] = tempData.gas;
    }
    else
    {
        root["gas"] = "Not installed";
    }
    root["humidity"] = tempData.hasHumidity ? String(tempData.humidity) : "Not installed";
    root["pressure"] = tempData.hasPressure ? String(tempData.pressure) : "Not installed";
    root["gas"] = tempData.hasGas ? String(tempData.gas) : "Not installed";

    String json;
    root.printTo(json);

    // Log(F("Sensor readings"), F("Hum/Temp/Press/Gas:"));
    // Log(F("Sensor readings"), json);
    return json;
}

// void Handle_factoryreset()
// {
// #if defined(ESP8266)
// 	File configFile = LittleFS.open("/config.json", "w");
// #elif defined(ESP32)
// 	File configFile = SPIFFS.open("/config.json", "w");
// #endif
// 	if (!configFile)
// 	{
// 		Log(F("Handle_factoryreset"), F("Failed to open config file for reset"));
// 	}
// 	configFile.println("");
// 	configFile.close();
// 	WifiSetup();
// 	ESP.restart();
// }

void HandleAndSendButtonPress(uint button)
{
    btnLastPressedMillis[button] = millis();
    Log(F("Buttons"), "Btn" + String(button) + " pressed");

    // Prüfen ob über MQTT versendet werden muss
    if (mqttAktiv == true && client.connected())
    {
        client.publish((mqttMasterTopic + "button" + String(button)).c_str(), String(btnLastPressedMillis[button]).c_str(), true);
    }
    // Prüfen ob über Websocket versendet werden muss
    if (webSocket.connectedClients() > 0)
    {
        for (uint i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            webSocket.sendTXT(i, "{\"buttons\":" + GetButtons() + "}");
        }
    }

    if (config.btnAction[button] == btnAction_ToggleSleepMode)
    {
        config.sleepMode = !config.sleepMode;
        if (config.sleepMode)
        {
            matrix->clear();
            DrawTextHelper("Zzz", false, true, false, false, false, false, NULL, 0, 0, 255, 0, 1);
            FadeOut(30, 0);
            matrix->setBrightness(0);
            matrix->show();
        }
        else
        {
            matrix->clear();
            DrawTextHelper("😀", false, true, false, false, false, false, NULL, 255, 200, 0, 0, 1);
            FadeIn(30, 0);
            delay(150);
            config.forceClock = true;
        }
    }
    if (config.btnAction[button] == btnAction_GotoClock)
    {
        config.forceClock = true;
    }
    if (config.btnAction[button] == btnAction_MP3PlayPrevious)
    {
        mp3Player.playPrevious();
    }
    if (config.btnAction[button] == btnAction_MP3PlayNext)
    {
        mp3Player.playNext();
    }
    if (config.btnAction[button] == btnAction_MP3PlayPause)
    {
        if (mp3Player.isPlaying())
        {
            mp3Player.pause();
        }
        else
        {
            delay(200);
            mp3Player.resume();
        }
    }
}

void callback(char *topic, byte *payload, unsigned int length)
{
    if (payload[0] == '{')
    {
        payload[length] = '\0';
        String channel = String(topic);
        channel.replace(mqttMasterTopic, "");

        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.parseObject(payload);

        Log("MQTT_callback", "Incomming Json length: " + String(json.measureLength()));

        if (channel.equals("setScreen"))
        {
            CreateFrames(json);
        }
        else if (channel.equals("getLuxsensor"))
        {
            client.publish((mqttMasterTopic + "luxsensor").c_str(), GetLuxSensor().c_str());
        }
        else if (channel.equals("getMatrixinfo"))
        {
            client.publish((mqttMasterTopic + "matrixinfo").c_str(), GetMatrixInfo().c_str());
        }
        else if (channel.equals("getConfig"))
        {
            client.publish((mqttMasterTopic + "config").c_str(), GetConfig().c_str());
        }
        else if (channel.equals("setConfig"))
        {
            config.SetConfig(json);
        }
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_DISCONNECTED:
    {
        Log("WebSocketEvent", "[" + String(num) + "] Disconnected!");
        websocketConnection[num] = "";
        break;
    }
    case WStype_CONNECTED:
    {
        // Merken für was die Connection hergstellt wurde
        websocketConnection[num] = String((char *)payload);

        // IP der Connection abfragen
        IPAddress ip = webSocket.remoteIP(num);

        // Logausgabe
        Log(F("WebSocketEvent"), "[" + String(num) + "] Connected from " + ip.toString() + " url: " + websocketConnection[num]);

        // send message to client
        SendMatrixInfo(true);
        SendLDR(true);
        SendSensor(true);
        SendConfig();
        webSocket.sendTXT(num, "{\"buttons\":" + GetButtons() + "}");
        break;
    }
    case WStype_TEXT:
    {
        if (((char *)payload)[0] == '{')
        {
            DynamicJsonBuffer jsonBuffer;
            JsonObject &json = jsonBuffer.parseObject(payload);

            // Logausgabe
            Log(F("WebSocketEvent"), "Incoming Json length: " + String(json.measureLength()));

            if (json.containsKey("setScreen"))
            {
                CreateFrames(json["setScreen"]);
            }
            else if (json.containsKey("setConfig"))
            {
                config.SetConfig(json["setConfig"]);
                delay(500);
                ESP.restart();
            }
        }
        break;
    }
    case WStype_BIN:
        break;
    case WStype_FRAGMENT_BIN_START:
        break;
    case WStype_FRAGMENT_TEXT_START:
        break;
    case WStype_FRAGMENT:
        break;
    case WStype_FRAGMENT_FIN:
        break;
    case WStype_PING:
        break;
    case WStype_PONG:
        break;
    case WStype_ERROR:
        break;
    }
}

void CreateFrames(JsonObject &json)
{
    String logMessage = F("Json contains ");

    // Ist eine Display Helligkeit übergeben worden?
    if (json.containsKey("brightness"))
    {
        logMessage += F("Brightness Control, ");
        config.matrixBrightness = json["brightness"];
    }

    // Set GPIO
    if (json.containsKey("setGpio"))
    {
        logMessage += F("Set Gpio, ");
        if (json["setGpio"]["set"].is<bool>() && json["setGpio"]["gpio"].is<uint8_t>())
        {
            uint8_t gpio = json["setGpio"]["gpio"].as<uint8_t>();

            // If the GPIO is already present in the array?
            // has been found, this is to be replaced.
            if (json["setGpio"]["duration"].is<ulong>())
            {
                int arrayIndex = -1;
                for (int i = 0; i < SET_GPIO_SIZE; i++)
                {
                    if (setGPIOReset[i].gpio == gpio)
                    {
                        arrayIndex = i;
                        break;
                    }
                }
                // Search free place in array.
                if (arrayIndex == -1)
                {
                    for (int i = 0; i < SET_GPIO_SIZE; i++)
                    {
                        if (setGPIOReset[i].gpio == -1)
                        {
                            arrayIndex = i;
                            break;
                        }
                    }
                }

                if (arrayIndex == -1)
                {
                    Log(F("SetGPIO"), F("Error: no free place in array found!"));
                }
                else
                {
                    // Save data in array for the reset.
                    setGPIOReset[arrayIndex].gpio = gpio;
                    setGPIOReset[arrayIndex].resetMillis = (millis() + json["setGpio"]["duration"].as<ulong>());
                    Log(F("SetGPIO"), "Pos: " + String(arrayIndex) + ", GPIO: " + String(gpio) + ", Duration: " + String(json["setGpio"]["duration"].as<char *>()) + ", Value: " + json["setGpio"]["set"].as<char *>());
                }
            }
            else
            {
                Log(F("SetGPIO"), "GPIO: " + String(gpio) + ", Value: " + json["setGpio"]["set"].as<char *>());
            }
            // Set GPIO
            pinMode(gpio, OUTPUT);
            digitalWrite(gpio, json["setGpio"]["set"].as<bool>());
        }
    }

    // Sound
    if (json.containsKey("sound"))
    {
        logMessage += F("Sound, ");
        // Volume
        if (json["sound"]["volume"] != NULL && json["sound"]["volume"].is<int>())
        {
            mp3Player.volume(json["sound"]["volume"].as<int>());

            // Sometimes, mp3Player gets hickups. A brief delay might help - but also might hinder scrolling.
            // So, do it only if there are more commands to come.
            if (json["sound"]["control"].asString() > "")
            {
                Log(F("Sound"), F("Changing volume can prevent DFPlayer from executing a control command at the same time. Better make two separate API calls."));
                delay(200);
            }
        }
        // Play
        if (json["sound"]["control"] == "play")
        {
            if (json["sound"]["folder"])
            {
                mp3Player.playFolder(json["sound"]["folder"].as<int>(), json["sound"]["file"].as<int>());
            }
            else
            {
                mp3Player.play(json["sound"]["file"].as<int>());
            }
        }
        // Stop
        else if (json["sound"]["control"] == "pause")
        {
            mp3Player.pause();
        }
        // Play Next
        else if (json["sound"]["control"] == "next")
        {
            mp3Player.playNext();
        }
        // Play Previous
        else if (json["sound"]["control"] == "previous")
        {
            mp3Player.playPrevious();
        }
    }

    // SleepMode
    if (json.containsKey("sleepMode"))
    {
        logMessage += F("SleepMode Control, ");
        config.sleepMode = json["sleepMode"];
    }
    // SleepMode
    if (config.sleepMode)
    {
        matrix->setBrightness(0);
        matrix->show();
    }
    else
    {
        matrix->setBrightness(config.matrixBrightness);

        // Prüfung für die Unterbrechnung der lokalen Schleifen
        if (json.containsKey("bitmap") || json.containsKey("bitmaps") || json.containsKey("text") || json.containsKey("bar") || json.containsKey("bars") || json.containsKey("bitmapAnimation"))
        {
            lastScreenMessageMillis = millis();
            config.clockAktiv = false;
            config.scrollTextAktivLoop = false;
            animateBMPAktivLoop = false;
        }

        // Ist eine Switch Animation übergeben worden?
        bool fadeAnimationAktiv = false;
        bool coloredBarWipeAnimationAktiv = false;
        bool zigzagWipeAnimationAktiv = false;
        bool bitmapWipeAnimationAktiv = false;
        if (json.containsKey("switchAnimation"))
        {
            logMessage += F("SwitchAnimation, ");
            // Switch Animation aktiv?
            if (json["switchAnimation"]["aktiv"])
            {
                // Fade Animation aktiv?
                if (json["switchAnimation"]["animation"] == "fade")
                {
                    fadeAnimationAktiv = true;
                }
                else if (json["switchAnimation"]["animation"] == "coloredBarWipe")
                {
                    coloredBarWipeAnimationAktiv = true;
                }
                else if (json["switchAnimation"]["animation"] == "zigzagWipe")
                {
                    zigzagWipeAnimationAktiv = true;
                }
                else if (json["switchAnimation"]["animation"] == "bitmapWipe")
                {
                    bitmapWipeAnimationAktiv = true;
                }
                else if (json["switchAnimation"]["animation"] == "random")
                {
                    switch (millis() % 3)
                    {
                    case 0:
                        fadeAnimationAktiv = true;
                        break;
                    case 1:
                        coloredBarWipeAnimationAktiv = true;
                        break;
                    case 2:
                        zigzagWipeAnimationAktiv = true;
                        break;
                    }
                }
            }
        }

        // Fade aktiv?
        if (fadeAnimationAktiv)
        {
            FadeOut();
        }
        else if (coloredBarWipeAnimationAktiv)
        {
            ColoredBarWipe();
        }
        else if (zigzagWipeAnimationAktiv)
        {
            uint8_t r = 255;
            uint8_t g = 255;
            uint8_t b = 255;
            if (json["switchAnimation"]["hexColor"].as<char *>() != NULL)
            {
                ColorConverter::HexToRgb(json["switchAnimation"]["hexColor"].as<char *>(), r, g, b);
            }
            else if (json["switchAnimation"]["color"]["r"].as<char *>() != NULL)
            {
                r = json["switchAnimation"]["color"]["r"].as<uint8_t>();
                g = json["switchAnimation"]["color"]["g"].as<uint8_t>();
                b = json["switchAnimation"]["color"]["b"].as<uint8_t>();
            }
            ZigZagWipe(r, g, b);
        }
        else if (bitmapWipeAnimationAktiv)
        {
            BitmapWipe(json["switchAnimation"]["data"].as<JsonArray>(), json["switchAnimation"]["width"].as<uint8_t>());
        }

        // Clock
        if (json.containsKey("clock"))
        {
            logMessage += F("InternalClock Control, ");
            if (json["clock"]["show"])
            {
                config.scrollTextAktivLoop = false;
                animateBMPAktivLoop = false;
                config.clockAktiv = true;

                config.clockCounterClock = 0;
                config.clockCounterDate = 0;

                config.clockSwitchAktiv = json["clock"]["switchAktiv"];

                if (config.clockSwitchAktiv == true)
                {
                    config.clockSwitchSec = json["clock"]["switchSec"];
                }

                config.clockWithSeconds = json["clock"]["withSeconds"];

                if (json["clock"]["color"]["r"].as<char *>() != NULL)
                {
                    config.clockColorR = json["clock"]["color"]["r"].as<uint8_t>();
                    config.clockColorG = json["clock"]["color"]["g"].as<uint8_t>();
                    config.clockColorB = json["clock"]["color"]["b"].as<uint8_t>();
                }
                else if (json["clock"]["hexColor"].as<char *>() != NULL)
                {
                    ColorConverter::HexToRgb(json["clock"]["hexColor"].as<char *>(), config.clockColorR, config.clockColorG, config.clockColorB);
                }
                DrawClock(true);
            }
        }

        if (json.containsKey("bitmap") || json.containsKey("bitmaps") || json.containsKey("bitmapAnimation") || json.containsKey("text") || json.containsKey("bar") || json.containsKey("bars"))
        {
            // Alle Pixel löschen
            matrix->clear();
        }

        // Bar
        if (json.containsKey("bar"))
        {
            logMessage += F("Bar, ");
            uint8_t r, g, b;
            if (json["bar"]["hexColor"].as<char *>() != NULL)
            {
                ColorConverter::HexToRgb(json["clock"]["hexColor"].as<char *>(), r, g, b);
            }
            else
            {
                r = json["bar"]["color"]["r"].as<uint8_t>();
                g = json["bar"]["color"]["g"].as<uint8_t>();
                b = json["bar"]["color"]["b"].as<uint8_t>();
            }
            matrix->drawLine(json["bar"]["position"]["x"], json["bar"]["position"]["y"], json["bar"]["position"]["x2"], json["bar"]["position"]["y2"], matrix->Color(r, g, b));
        }

        // Bars
        if (json.containsKey("bars"))
        {
            logMessage += F("Bars, ");
            for (JsonVariant x : json["bars"].as<JsonArray>())
            {
                uint8_t r, g, b;
                if (json["bar"]["hexColor"].as<char *>() != NULL)
                {
                    ColorConverter::HexToRgb(json["clock"]["hexColor"].as<char *>(), r, g, b);
                }
                else
                {
                    r = x["color"]["r"].as<uint8_t>();
                    g = x["color"]["g"].as<uint8_t>();
                    b = x["color"]["b"].as<uint8_t>();
                }
                matrix->drawLine(x["position"]["x"], x["position"]["y"], x["position"]["x2"], x["position"]["y2"], matrix->Color(r, g, b));
            }
        }

        // Ist ein Bitmap übergeben worden?
        if (json.containsKey("bitmap"))
        {
            logMessage += F("Bitmap, ");
            DrawSingleBitmap(json["bitmap"]);
        }

        // Sind mehrere Bitmaps übergeben worden?
        if (json.containsKey("bitmaps"))
        {
            logMessage += F("Bitmaps (");
            for (JsonVariant singleBitmap : json["bitmaps"].as<JsonArray>())
            {
                DrawSingleBitmap(singleBitmap);
                logMessage += F("Bitmap,");
            }

            logMessage += F("), ");
        }

        // Ist eine BitmapAnimation übergeben worden?
        if (json.containsKey("bitmapAnimation"))
        {
            logMessage += F("BitmapAnimation, ");
            // animationBmpList zurücksetzten um das ende nacher zu finden -1 (habe aktuell keine bessere Lösung)
            for (int i = 0; i < 10; i++)
            {
                animationBmpList[i][0] = 2;
            }

            int counter = 0;
            for (JsonVariant x : json["bitmapAnimation"]["data"].as<JsonArray>())
            {
                // JsonArray in IntArray konvertieren
                x.as<JsonArray>().copyTo(bmpArray);
                // Speichern für die Ausgabe
                for (int i = 0; i < 64; i++)
                {
                    animationBmpList[counter][i] = bmpArray[i];
                }
                counter++;
            }

            animateBMPDelay = json["bitmapAnimation"]["animationDelay"];
            animateBMPRubberbandingAktiv = json["bitmapAnimation"]["rubberbanding"];

            animateBMPLimitLoops = 0;
            if (json["bitmapAnimation"]["limitLoops"])
            {
                animateBMPLimitLoops = json["bitmapAnimation"]["limitLoops"].as<int>();
            }

            // Hier einmal den Counter zurücksetzten
            // Reset the counter here
            animateBMPCounter = 0;
            animateBMPLoopCount = 0;
            animateBMPAktivLoop = true;
            animateBMPReverse = false;
            animateBMPPrevMillis = millis();
            AnimateBMP(false);
        }

        // Ist ein Text übergeben worden?
        // Has a text been handed over?
        bool scrollTextAktiv = false;
        if (json.containsKey("text"))
        {
            logMessage += F("Text");
            // Immer erstmal den Default Delay annehmen
            // Always accept the default delay first
            config.scrollTextDelay = config.scrollTextDefaultDelay;

            // Ist ScrollText auto oder true gewählt?
            // Is ScrollText auto or true selected?
            scrollTextAktiv = ((json["text"]["scrollText"] == "auto" || ((json["text"]["scrollText"]).is<bool>() && json["text"]["scrollText"])));

            uint8_t r, g, b;
            if (json["text"]["hexColor"].as<char *>() != NULL)
            {
                ColorConverter::HexToRgb(json["text"]["hexColor"].as<char *>(), r, g, b);
            }
            else
            {
                r = json["text"]["color"]["r"].as<uint8_t>();
                g = json["text"]["color"]["g"].as<uint8_t>();
                b = json["text"]["color"]["b"].as<uint8_t>();
            }

            if (json["text"]["centerText"])
            {
                bool withBMP = json.containsKey("bitmap") || json.containsKey("bitmapAnimation");

                DrawTextCenter(json["text"]["textString"], json["text"]["bigFont"], withBMP, r, g, b);
            }
            // Ist ScrollText auto oder true gewählt?
            else if (scrollTextAktiv)
            {

                bool withBMP = (json.containsKey("bitmap") || json.containsKey("bitmapAnimation"));

                bool fadeInRequired = ((json.containsKey("bars") || json.containsKey("bar") || json.containsKey("bitmap") || json.containsKey("bitmapAnimation")) && fadeAnimationAktiv);

                // Wurde ein Benutzerdefeniertes Delay übergeben?
                if (json["text"]["scrollTextDelay"])
                {
                    config.scrollTextDelay = json["text"]["scrollTextDelay"];
                }

                if (!(json["text"]["scrollText"]).is<bool>() && json["text"]["scrollText"] == "auto")
                {
                    DrawAutoTextScrolled(json["text"]["textString"], json["text"]["bigFont"], withBMP, fadeInRequired, bmpArray, r, g, b);
                }
                else
                {
                    DrawTextScrolled(json["text"]["textString"], json["text"]["bigFont"], withBMP, fadeInRequired, bmpArray, r, g, b);
                }
            }
            else
            {
                DrawText(json["text"]["textString"], json["text"]["bigFont"], r, g, b, json["text"]["position"]["x"], json["text"]["position"]["y"]);
            }
        }

        // Fade aktiv?
        if (!scrollTextAktiv && (fadeAnimationAktiv || coloredBarWipeAnimationAktiv || zigzagWipeAnimationAktiv || bitmapWipeAnimationAktiv))
        {
            FadeIn();
        }
        else
        {
            // Fade nicht aktiv!
            // Muss mich selbst um Show kümmern
            matrix->show();
        }
    }

    Log(F("CreateFrames"), logMessage);
}

String GetConfig()
{
#if defined(ESP8266)
    File configFile = LittleFS.open("/config.json", "r");
#elif defined(ESP32)
    File configFile = SPIFFS.open("/config.json", "r");
#endif

    if (configFile)
    {
        // Log(F("GetConfig"), F("Opened config file"));
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject &root = jsonBuffer.parseObject(buf.get());

        String json;
        root.printTo(json);

        return json;
    }
    return "";
}

String GetLuxSensor()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root["lux"] = sensors.LastLux.lux;

    String json;
    root.printTo(json);

    return json;
}

String GetBrightness()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root["brightness_255"] = config.matrixBrightness;
    root["brightness"] = map(config.matrixBrightness, 0, 255, 0, 100);

    String json;
    root.printTo(json);

    return json;
}

String GetMatrixInfo()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root["pixelitVersion"] = VERSION;
    //// Matrix Config
    root["note"] = config.note;
    root["hostname"] = config.hostname;
    root["freeSketchSpace"] = ESP.getFreeSketchSpace();
    root["wifiRSSI"] = WiFi.RSSI();
    root["wifiQuality"] = GetRSSIasQuality(WiFi.RSSI());
    root["wifiSSID"] = WiFi.SSID();
    root["ipAddress"] = WiFi.localIP().toString();
    root["freeHeap"] = ESP.getFreeHeap();

#if defined(ESP8266)
    root["sketchSize"] = ESP.getSketchSize();
    root["chipID"] = ESP.getChipId();
#elif defined(ESP32)
    root["chipID"] = uint64ToString(ESP.getEfuseMac());
#endif

    root["cpuFreqMHz"] = ESP.getCpuFreqMHz();
    root["sleepMode"] = config.sleepMode;

    String json;
    root.printTo(json);

    return json;
}

String GetButtons()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    for (uint b = 0; b < 3; b++)
    {
        root["button" + String(b)] = btnLastPressedMillis[b];
    }

    String json;
    root.printTo(json);

    return json;
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
void DrawText(String text, bool bigFont, int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
    DrawTextHelper(text, bigFont, false, false, false, false, false, NULL, colorRed, colorGreen, colorBlue, posX, posY);
}

void DrawTextCenter(String text, bool bigFont, bool withBMP, int colorRed, int colorGreen, int colorBlue)
{
    DrawTextHelper(text, bigFont, true, false, false, withBMP, false, NULL, colorRed, colorGreen, colorBlue, 0, 1);
}

void DrawTextScrolled(String text, bool bigFont, bool withBMP, bool fadeInRequired, uint16_t bmpArray[64], int colorRed, int colorGreen, int colorBlue)
{
    DrawTextHelper(text, bigFont, false, true, false, withBMP, fadeInRequired, bmpArray, colorRed, colorGreen, colorBlue, 0, 1);
}

void DrawAutoTextScrolled(String text, bool bigFont, bool withBMP, bool fadeInRequired, uint16_t bmpArray[64], int colorRed, int colorGreen, int colorBlue)
{
    DrawTextHelper(text, bigFont, false, false, true, withBMP, fadeInRequired, bmpArray, colorRed, colorGreen, colorBlue, 0, 1);
}

void DrawTextHelper(String text, bool bigFont, bool centerText, bool scrollText, bool autoScrollText, bool withBMP, bool fadeInRequired, uint16_t bmpArray[64], int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
    uint16_t xPixelText, xPixel;

    text = Utf8ToAscii(text);

    if (withBMP)
    {
        // Verfügbare Text Pixelanzahl in der Breite (X) mit Bild
        xPixel = 24;
        posX = 8;
    }
    else
    {
        // Verfügbare Text Pixelanzahl in der Breite (X) ohne Bild
        xPixel = 32;
    }

    if (bigFont)
    {
        // Grosse Schrift setzten
        matrix->setFont();
        xPixelText = text.length() * 6;
        // Positions Korrektur
        posY = posY - 1;
    }
    else
    {
        // Kleine Schrift setzten
        matrix->setFont(&PixelItFont);
        xPixelText = text.length() * 4;
        // Positions Korrektur
        posY = posY + 5;
    }

    // matrix->getTextBounds(text, 0, 0, &x1, &y1, &xPixelText, &h);

    if (centerText)
    {
        // Kein Offset benötigt.
        int offset = 0;

        // Mit BMP berechnen
        if (withBMP)
        {
            // Offset um nicht das BMP zu überschreiben
            // + 1 weil wir der erste pixel 0 ist!!!!
            offset = 8 + 1;
        }

        // Berechnung für das erste Pixel des Textes
        posX = ((xPixel - xPixelText) / 2) + offset;
    }

    matrix->setCursor(posX, posY);

    matrix->setTextColor(matrix->Color(colorRed, colorGreen, colorBlue));

    if (scrollText || (autoScrollText && xPixelText > xPixel))
    {

        matrix->setBrightness(config.matrixBrightness);

        config.scrollTextString = text;
        config.scrollposY = posY;
        config.scrollwithBMP = withBMP;
        config.scrollxPixelText = xPixelText;
        // + 8 Pixel sonst scrollt er mitten drinn los!
        config.scrollPos = 33;

        config.scrollTextAktivLoop = true;
        scrollTextPrevMillis = millis();
        ScrollText(fadeInRequired);
    }
    // Fall doch der Text auf denm Display passt!
    else if (autoScrollText)
    {
        matrix->print(text);
        // Hier muss geprüfter weden ob ausgefadet wurde,
        // dann ist nämlich die Brightness zu weit unten (0),
        // aber auch nur wenn nicht animateBMPAktivLoop aktiv ist,
        // denn sonst hat er das schon erledigt.
        if (fadeInRequired && !animateBMPAktivLoop)
        {
            FadeIn();
        }
        else
        {
            matrix->show();
        }
    }
    else
    {
        matrix->print(text);
    }
}

void ScrollText(bool isFadeInRequired)
{
    int tempxPixelText = 0;

    if (config.scrollxPixelText < 32 && !config.scrollwithBMP)
    {
        config.scrollxPixelText = 32;
    }
    else if (config.scrollxPixelText < 24 && config.scrollwithBMP)
    {
        config.scrollxPixelText = 24;
    }
    else if (config.scrollwithBMP)
    {
        tempxPixelText = 8;
    }

    if (config.scrollPos > ((config.scrollxPixelText - tempxPixelText) * -1))
    {
        matrix->clear();
        matrix->setCursor(--config.scrollPos, config.scrollposY);
        matrix->print(config.scrollTextString);

        if (config.scrollwithBMP)
        {
            matrix->drawRGBBitmap(0, 0, bmpArray, 8, 8);
        }

        if (isFadeInRequired)
        {
            FadeIn();
        }
        else
        {
            matrix->show();
        }
    }
    else
    {
        // + 8 Pixel sonst scrollt er mitten drinn los!
        config.scrollPos = 33;
    }
}

void AnimateBMP(bool isShowRequired)
{
    // Prüfen auf 2, 2 ist mein Platzhalter für leeres obj!
    if (animationBmpList[animateBMPCounter][0] == 2)
    {
        // Ist ein Repeat Limit übergeben worden.
        if (animateBMPLimitLoops > 0 && !animateBMPRubberbandingAktiv)
        {
            animateBMPLoopCount++;

            // Ist der Repeat Limit erreicht den AnimateBMP Loop deaktiveren.
            if (animateBMPLoopCount == animateBMPLimitLoops)
            {
                animateBMPAktivLoop = false;
                return;
            }
        }

        // Prüfen ob Rubberbanding aktiv und mehr wie 1 Frame vorhanden ist.
        if (animateBMPRubberbandingAktiv && animateBMPCounter > 1)
        {
            animateBMPReverse = true;

            // 2 abziehen da sonst der letzte Frame doppelt angezeigt wird.
            if (animateBMPCounter > 0)
            {
                animateBMPCounter = animateBMPCounter - 2;
            }
        }
        else
        {
            animateBMPCounter = 0;
        }
    }

    if (animateBMPCounter < 0)
    {
        // Auf 1 setzten da sons der erste Frame doppelt angezeigt wird.
        animateBMPCounter = 1;
        animateBMPReverse = false;

        // Ist ein Repeat Limit übergeben worden.
        if (animateBMPLimitLoops > 0)
        {
            animateBMPLoopCount++;
            // Ist der Repeat Limit erreicht den AnimateBMP Loop deaktiveren.
            if (animateBMPLoopCount >= animateBMPLimitLoops)
            {
                animateBMPAktivLoop = false;
                return;
            }
        }
    }

    ClearBMPArea();

    matrix->drawRGBBitmap(0, 0, animationBmpList[animateBMPCounter], 8, 8);

    for (int y = 0; y < 64; y++)
    {
        bmpArray[y] = animationBmpList[animateBMPCounter][y];
    }

    // Soll der Loop wieder zurücklaufen?
    if (animateBMPReverse)
    {
        animateBMPCounter--;
    }
    else
    {
        animateBMPCounter++;
    }

    if (isShowRequired)
    {
        matrix->show();
    }
}

void DrawSingleBitmap(JsonObject &json)
{
    int16_t h = json["size"]["height"].as<int16_t>();
    int16_t w = json["size"]["width"].as<int16_t>();
    int16_t x = json["position"]["x"].as<int16_t>();
    int16_t y = json["position"]["y"].as<int16_t>();

    // Hier kann leider nicht die Funktion matrix->drawRGBBitmap() genutzt werde da diese Fehler in der Anzeige macht wenn es mehr wie 8x8 Pixel werden.
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            matrix->drawPixel(x + i, y, json["data"][j * w + i].as<uint16_t>());
        }
    }

    // JsonArray in IntArray konvertieren
    // dies ist nötig für diverse kleine Logiken z.B. Scrolltext
    // bei Multibitmaps landet hier nur eine der Bitmaps - das ist aber egal, da dann eh nicht gescrollt wird
    json["data"].as<JsonArray>().copyTo(bmpArray);
}

void DrawClock(bool fromJSON)
{
    matrix->clear();

    char date[14];
    char time[14];

    int xPosTime = 0;

    if (config.clockDateDayMonth)
    {
        sprintf_P(date, PSTR("%02d.%02d."), day(), month());
    }
    else
    {
        sprintf_P(date, PSTR("%02d/%02d"), month(), day());
    }

    if (config.clock24Hours && config.clockWithSeconds)
    {
        xPosTime = 2;
        sprintf_P(time, PSTR("%02d:%02d:%02d"), hour(), minute(), second());
    }
    else if (!config.clock24Hours)
    {
        xPosTime = 2;

        if (config.clockBlink)
        {
            config.clockBlink = false;
            sprintf_P(time, PSTR("%02d:%02d %s"), hourFormat12(), minute(), isAM() ? "AM" : "PM");
        }
        else
        {
            config.clockBlink = !config.clockBlink;
            sprintf_P(time, PSTR("%02d %02d %s"), hourFormat12(), minute(), isAM() ? "AM" : "PM");
        }
    }
    else
    {
        xPosTime = 7;

        if (config.clockBlink)
        {
            config.clockBlink = false;
            sprintf_P(time, PSTR("%02d:%02d"), hour(), minute());
        }
        else
        {
            config.clockBlink = !config.clockBlink;
            sprintf_P(time, PSTR("%02d %02d"), hour(), minute());
        }
    }

    if (!config.clockSwitchAktiv || (config.clockSwitchAktiv && config.clockCounterClock <= config.clockSwitchSec))
    {
        if (config.clockSwitchAktiv)
        {
            config.clockCounterClock++;
        }

        if (config.clockCounterClock > config.clockSwitchSec)
        {
            config.clockCounterDate = 0;

            int counter = 0;
            while (counter <= 6)
            {
                counter++;
                matrix->clear();
                DrawText(String(time), false, config.clockColorR, config.clockColorG, config.clockColorB, xPosTime, (1 + counter));
                DrawText(String(date), false, config.clockColorR, config.clockColorG, config.clockColorB, 7, (-6 + counter));
                matrix->drawLine(0, 7, 33, 7, 0);
                DrawWeekDay();
                matrix->show();
                delay(35);
            }
        }
        else
        {
            DrawText(String(time), false, config.clockColorR, config.clockColorG, config.clockColorB, xPosTime, 1);
        }
    }
    else
    {
        config.clockCounterDate++;

        if (config.clockCounterDate == config.clockSwitchSec)
        {
            config.clockCounterClock = 0;

            int counter = 0;
            while (counter <= 6)
            {
                counter++;
                matrix->clear();
                DrawText(String(date), false, config.clockColorR, config.clockColorG, config.clockColorB, 7, (1 + counter));
                DrawText(String(time), false, config.clockColorR, config.clockColorG, config.clockColorB, xPosTime, (-6 + counter));
                matrix->drawLine(0, 7, 33, 7, 0);
                DrawWeekDay();
                matrix->show();
                delay(35);
            }
        }
        else
        {
            DrawText(String(date), false, config.clockColorR, config.clockColorG, config.clockColorB, 7, 1);
        }
    }

    DrawWeekDay();

    // Wenn der Aufruf nicht über JSON sondern über den Loop kommt
    // muss ich mich selbst ums Show kümmern.
    if (!fromJSON)
    {
        matrix->show();
    }
}

void DrawWeekDay()
{
    for (int i = 0; i <= 6; i++)
    {
        int weekDayNumber = 0;

        if (config.clockDayOfWeekFirstMonday)
        {
            weekDayNumber = DayOfWeekFirstMonday(dayOfWeek(now()));
        }
        else
        {
            weekDayNumber = dayOfWeek(now());
        }

        if (i == weekDayNumber - 1)
        {
            matrix->drawLine(2 + i * 4, 7, i * 4 + 4, 7, matrix->Color(config.clockColorR, config.clockColorG, config.clockColorB));
        }
        else
        {
            matrix->drawLine(2 + i * 4, 7, i * 4 + 4, 7, 21162);
        }
    }
}

boolean MQTTreconnect()
{

    bool connected = false;
    if (mqttUser != NULL && mqttUser.length() > 0 && mqttPassword != NULL && mqttPassword.length() > 0)
    {
        Log(F("MQTTreconnect"), F("MQTT connecting to broker with user and password"));
        connected = client.connect(config.hostname.c_str(), mqttUser.c_str(), mqttPassword.c_str(), (mqttMasterTopic + "state").c_str(), 0, true, "disconnected");
    }
    else
    {
        Log(F("MQTTreconnect"), F("MQTT connecting to broker without user and password"));
        connected = client.connect(config.hostname.c_str(), (mqttMasterTopic + "state").c_str(), 0, true, "disconnected");
    }

    if (connected)
    {
        Log(F("MQTTreconnect"), F("MQTT connected!"));
        // Subscribe to topics ...
        client.subscribe((mqttMasterTopic + "setScreen").c_str());
        client.subscribe((mqttMasterTopic + "getLuxsensor").c_str());
        client.subscribe((mqttMasterTopic + "getMatrixinfo").c_str());
        client.subscribe((mqttMasterTopic + "getConfig").c_str());
        client.subscribe((mqttMasterTopic + "setConfig").c_str());
        // ... and publish state ....
        client.publish((mqttMasterTopic + "state").c_str(), "connected", true);

        // ... and provide discovery information
        // Create discovery information for Homeassistant
        // Can also be processed by ioBroker, OpenHAB etc.
        String deviceID = config.hostname;
        if (deviceID.isEmpty())
            deviceID = "pixelit";
#if defined(ESP8266)
        deviceID += ESP.getChipId();
#elif defined(ESP32)
        deviceID += uint64ToString(ESP.getEfuseMac());
#endif

        String configTopicTemplate = String(F("homeassistant/sensor/#DEVICEID#/#DEVICEID##SENSORNAME#/config"));
        configTopicTemplate.replace(F("#DEVICEID#"), deviceID);
        String configPayloadTemplate = String(F(
            "{ \
            \"device\":{ \
                \"identifiers\":\"#DEVICEID#\", \
                \"name\":\"#config.HOSTNAME#\", \
                \"model\":\"PixelIt\", \
                \"sw_version\":\"#VERSION#\" \
            }, \
            \"availability_topic\":\"#MASTERTOPIC#state\", \
            \"payload_available\":\"connected\", \
            \"payload_not_available\":\"disconnected\", \
            \"unique_id\":\"#DEVICEID##SENSORNAME#\", \
            \"device_class\":\"#CLASS#\", \
            \"name\":\"#SENSORNAME#\", \
            \"state_topic\":\"#MASTERTOPIC##STATETOPIC#\", \
            \"unit_of_measurement\":\"#UNIT#\", \
            \"value_template\":\"{{value_json.#VALUENAME#}}\" \
        }"));
        configPayloadTemplate.replace(" ", "");
        configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
        configPayloadTemplate.replace(F("#config.HOSTNAME#"), config.hostname);
        configPayloadTemplate.replace(F("#VERSION#"), VERSION);
        configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttMasterTopic);

        String topic;
        String payload;

        if (sensors.tempSensor != TempSensor_None)
        {
            topic = configTopicTemplate;
            topic.replace(F("#SENSORNAME#"), F("Temperature"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORNAME#"), F("Temperature"));
            payload.replace(F("#CLASS#"), F("temperature"));
            payload.replace(F("#STATETOPIC#"), F("sensor"));
            payload.replace(F("#UNIT#"), "°C");
            payload.replace(F("#VALUENAME#"), F("temperature"));
            client.publish(topic.c_str(), payload.c_str(), true);

            topic = configTopicTemplate;
            topic.replace(F("#SENSORNAME#"), F("Humidity"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORNAME#"), F("Humidity"));
            payload.replace(F("#CLASS#"), F("humidity"));
            payload.replace(F("#STATETOPIC#"), F("sensor"));
            payload.replace(F("#UNIT#"), "%");
            payload.replace(F("#VALUENAME#"), F("humidity"));
            client.publish(topic.c_str(), payload.c_str(), true);
        }
        if (sensors.tempSensor == TempSensor_BME280 || sensors.tempSensor == TempSensor_BMP280 || sensors.tempSensor == TempSensor_BME680)
        {
            topic = configTopicTemplate;
            topic.replace(F("#SENSORNAME#"), F("Pressure"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORNAME#"), F("Pressure"));
            payload.replace(F("#CLASS#"), F("pressure"));
            payload.replace(F("#STATETOPIC#"), F("sensor"));
            payload.replace(F("#UNIT#"), "hPa");
            payload.replace(F("#VALUENAME#"), F("pressure"));
            client.publish(topic.c_str(), payload.c_str(), true);
        }

        if (sensors.tempSensor == TempSensor_BME680)
        {
            topic = configTopicTemplate;
            topic.replace(F("#SENSORNAME#"), F("VOC"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORNAME#"), F("VOC"));
            payload.replace(F("#CLASS#"), F("volatile_organic_compounds"));
            payload.replace(F("#STATETOPIC#"), F("sensor"));
            payload.replace(F("#UNIT#"), "kOhm");
            payload.replace(F("#VALUENAME#"), F("gas"));
            client.publish(topic.c_str(), payload.c_str(), true);
        }
        topic = configTopicTemplate;
        topic.replace(F("#SENSORNAME#"), F("Illuminance"));

        payload = configPayloadTemplate;
        payload.replace(F("#SENSORNAME#"), F("Illuminance"));
        payload.replace(F("#CLASS#"), F("illuminance"));
        payload.replace(F("#STATETOPIC#"), F("luxsensor"));
        payload.replace(F("#UNIT#"), "lx");
        payload.replace(F("#VALUENAME#"), F("lux"));
        client.publish(topic.c_str(), payload.c_str(), true);

        configPayloadTemplate = String(F(
            "{ \
            \"device\":{ \
                \"identifiers\":\"#DEVICEID#\", \
                \"name\":\"#config.HOSTNAME#\", \
                \"model\":\"PixelIt\", \
                \"sw_version\":\"#VERSION#\" \
            }, \
            \"availability_topic\":\"#MASTERTOPIC#state\", \
            \"payload_available\":\"connected\", \
            \"payload_not_available\":\"disconnected\", \
            \"unique_id\":\"#DEVICEID##SENSORNAME#\", \
            \"device_class\":\"timestamp\", \
            \"name\":\"#SENSORNAME#\", \
            \"state_topic\":\"#MASTERTOPIC##STATETOPIC#\" \
        }"));

        configPayloadTemplate.replace(" ", "");
        configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
        configPayloadTemplate.replace(F("#config.HOSTNAME#"), config.hostname);
        configPayloadTemplate.replace(F("#VERSION#"), VERSION);
        configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttMasterTopic);

        for (uint8_t n = 0; n < sizeof(config.btnEnabled) / sizeof(config.btnEnabled[0]); n++)
        {
            if (config.btnEnabled[n])
            {
                topic = configTopicTemplate;
                topic.replace(F("#SENSORNAME#"), String(F("Button")) + String(n));

                payload = configPayloadTemplate;
                payload.replace(F("#SENSORNAME#"), String(F("Button")) + String(n));
                payload.replace(F("#STATETOPIC#"), String(F("button")) + String(n));
                client.publish(topic.c_str(), payload.c_str(), true);
            }
        }
        Log(F("MQTTreconnect"), F("MQTT discovery information published"));
    }
    else
    {
        Log(F("MQTTreconnect"), F("MQTT connect failed! Retry in a few seconds..."));
    }

    return connected;
}
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
// Effekte

void FadeOut(int dealy, int minBrightness)
{
    int currentFadeBrightness = config.matrixBrightness;

    int counter = 25;
    while (counter >= 0)
    {
        currentFadeBrightness = map(counter, 0, 25, minBrightness, config.matrixBrightness);
        matrix->setBrightness(currentFadeBrightness);
        matrix->show();
        counter--;
        delay(dealy);
    }
}

void FadeIn(int dealy, int minBrightness)
{
    int currentFadeBrightness = minBrightness;

    int counter = 0;
    while (counter <= 25)
    {
        currentFadeBrightness = map(counter, 0, 25, minBrightness, config.matrixBrightness);
        matrix->setBrightness(currentFadeBrightness);
        matrix->show();
        counter++;
        delay(dealy);
    }
}

void ColoredBarWipe()
{
    for (uint16_t i = 0; i < 32 + 1; i++)
    {
        matrix->fillRect(0, 0, i - 1, 8, matrix->Color(0, 0, 0));

        matrix->drawFastVLine(i, 0, 8, ColorWheel((i * 8) & 255, 0));
        matrix->drawFastVLine(i + 1, 0, 8, ColorWheel((i * 9) & 255, 0));
        matrix->drawFastVLine(i - 1, 0, 8, 0);
        matrix->drawFastVLine(i - 2, 0, 8, 0);
        matrix->show();
        delay(15);
    }
}

void ZigZagWipe(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint16_t row = 0; row <= 7; row += 2)
    {
        for (uint16_t col = 0; col <= 31; col++)
        {
            if (row == 0 || row == 4)
            {
                matrix->fillRect(0, row, col - 1, 2, matrix->Color(0, 0, 0));
                matrix->drawFastVLine(col - 1, row, 2, matrix->Color(r, g, b));
                matrix->drawFastVLine(col, row, 2, matrix->Color(r, g, b));
            }
            else
            {
                matrix->fillRect(32 - col, row, col, 2, matrix->Color(0, 0, 0));
                matrix->drawFastVLine(32 - col, row, 2, matrix->Color(r, g, b));
                matrix->drawFastVLine(32 - col - 1, row, 2, matrix->Color(r, g, b));
            }
            matrix->show();
            delay(5);
        }
        matrix->fillRect(0, row, 32, 2, matrix->Color(0, 0, 0));
        if (row == 0 || row == 4)
        {
            matrix->drawFastVLine(30, row + 1, 2, matrix->Color(r, g, b));
            matrix->drawFastVLine(31, row + 1, 2, matrix->Color(r, g, b));
        }
        else
        {
            matrix->drawFastVLine(0, row + 1, 2, matrix->Color(r, g, b));
            matrix->drawFastVLine(1, row + 1, 2, matrix->Color(r, g, b));
        }
        matrix->show();
        delay(5);
        matrix->fillRect(0, row, 32, 2, matrix->Color(0, 0, 0));
    }
    matrix->fillRect(0, 0, 32, 8, matrix->Color(0, 0, 0));
    matrix->show();
}

void BitmapWipe(JsonArray &data, int16_t w)
{
    for (int16_t x = -w + 1; x <= 31; x++)
    {
        int16_t y = 0;
        for (int16_t j = 0; j < 8; j++, y++)
        {
            for (int16_t i = 0; i < w; i++)
            {
                matrix->drawPixel(x + i, y, data[j * w + i].as<uint16_t>());
            }
        }
        matrix->show();
        delay(18);
        matrix->fillRect(0, 0, x, 8, matrix->Color(0, 0, 0));
        matrix->show();
    }
}

void ColorFlash(int red, int green, int blue)
{
    for (uint16_t row = 0; row < 8; row++)
    {
        for (uint16_t column = 0; column < 32; column++)
        {
            matrix->drawPixel(column, row, matrix->Color(red, green, blue));
        }
    }
    matrix->show();
}

uint ColorWheel(byte wheelPos, int pos)
{
    if (wheelPos < 85)
    {
        return matrix->Color((wheelPos * 3) - pos, (255 - wheelPos * 3) - pos, 0);
    }
    else if (wheelPos < 170)
    {
        wheelPos -= 85;
        return matrix->Color((255 - wheelPos * 3) - pos, 0, (wheelPos * 3) - pos);
    }
    else
    {
        wheelPos -= 170;
        return matrix->Color(0, (wheelPos * 3) - pos, (255 - wheelPos * 3) - pos);
    }
}

void ShowBootAnimation()
{
    DrawTextHelper("PIXEL IT", false, false, false, false, false, false, NULL, 255, 255, 255, 3, 1);
    FadeIn(60, 10);
    FadeOut(60, 10);
    FadeIn(60, 10);
    FadeOut(60, 10);
    FadeIn(60, 10);
}

ColorTemperature GetUserColorTemp()
{
    if (config.matrixTempCorrection == "tungsten40w")
    {
        return Tungsten40W;
    }

    if (config.matrixTempCorrection == "tungsten100w")
    {
        return Tungsten100W;
    }

    if (config.matrixTempCorrection == "halogen")
    {
        return Halogen;
    }

    if (config.matrixTempCorrection == "carbonarc")
    {
        return CarbonArc;
    }

    if (config.matrixTempCorrection == "highnoonsun")
    {
        return HighNoonSun;
    }

    if (config.matrixTempCorrection == "directsunlight")
    {
        return DirectSunlight;
    }

    if (config.matrixTempCorrection == "overcastsky")
    {
        return OvercastSky;
    }

    if (config.matrixTempCorrection == "clearbluesky")
    {
        return ClearBlueSky;
    }

    if (config.matrixTempCorrection == "warmfluorescent")
    {
        return WarmFluorescent;
    }

    if (config.matrixTempCorrection == "standardfluorescent")
    {
        return StandardFluorescent;
    }

    if (config.matrixTempCorrection == "coolwhitefluorescent")
    {
        return CoolWhiteFluorescent;
    }
    if (config.matrixTempCorrection == "fullspectrumfluorescent")
    {
        return FullSpectrumFluorescent;
    }
    if (config.matrixTempCorrection == "growlightfluorescent")
    {
        return GrowLightFluorescent;
    }
    if (config.matrixTempCorrection == "blacklightfluorescent")
    {
        return BlackLightFluorescent;
    }
    if (config.matrixTempCorrection == "mercuryvapor")
    {
        return MercuryVapor;
    }
    if (config.matrixTempCorrection == "sodiumvapor")
    {
        return SodiumVapor;
    }
    if (config.matrixTempCorrection == "metalhalide")
    {
        return MetalHalide;
    }
    if (config.matrixTempCorrection == "highpressuresodium")
    {
        return HighPressureSodium;
    }

    return UncorrectedTemperature;
}

LEDColorCorrection GetUserColorCorrection()
{
    if (config.matrixTempCorrection == "default")
    {
        return TypicalSMD5050;
    }

    if (config.matrixTempCorrection == "typicalsmd5050")
    {
        return TypicalSMD5050;
    }

    if (config.matrixTempCorrection == "typical8mmpixel")
    {
        return Typical8mmPixel;
    }

    return UncorrectedColor;
}

int *GetUserCutomCorrection()
{
    String rgbString = config.matrixTempCorrection;
    rgbString.trim();

    // R,G,B / 255,255,255
    static int rgbArray[3];

    // R
    rgbArray[0] = rgbString.substring(0, 3).toInt();
    // G
    rgbArray[1] = rgbString.substring(4, 7).toInt();
    // B
    rgbArray[2] = rgbString.substring(8, 11).toInt();

    return rgbArray;
}

void ClearTextArea()
{
    int16_t h = 8;
    int16_t w = 24;
    int16_t x = 8;
    int16_t y = 0;

    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            matrix->drawPixel(x + i, y, (uint16_t)0);
        }
    }
}

void ClearBMPArea()
{
    int16_t h = 8;
    int16_t w = 8;
    int16_t x = 0;
    int16_t y = 0;

    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            matrix->drawPixel(x + i, y, (uint16_t)0);
        }
    }
}

int DayOfWeekFirstMonday(int OrigDayofWeek)
{
    int idx = 7 + OrigDayofWeek - 1;
    if (idx > 6) // week ends at 6, because Enum.DayOfWeek is zero-based
    {
        idx -= 7;
    }
    return idx;
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
void setup()
{
    Serial.begin(115200);

    // Mounting FileSystem
    Serial.println(F("Mounting file system..."));
#if defined(ESP8266)
    if (LittleFS.begin())
#elif defined(ESP32)
    if (SPIFFS.begin())
#endif
    {
        Serial.println(F("Mounted file system."));
        config.SetLogDelegate(Log);
        config.SetBrightnessDelegate(SetCurrentMatrixBrightness);
        config.LoadConfig();
        // If new version detected, create new variables in config if necessary.
        if (config.optionsVersion != VERSION)
        {
            Log(F("LoadConfig"), F("New version detected, create new variables in config if necessary"));
            config.SaveConfig();
            config.LoadConfig();
        }
    }
    else
    {
        Serial.println(F("Failed to mount FS"));
    }

    sensors.Initialise();

    // Init SetGPIO Array
    for (int i = 0; i < SET_GPIO_SIZE; i++)
    {
        setGPIOReset[i].gpio = -1;
        setGPIOReset[i].resetMillis = -1;
    }

    // Matix Type 1 (Colum major)
    if (config.matrixType == 1)
    {
        matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);
    }
    // Matix Type 2 (Row major)
    else if (config.matrixType == 2)
    {
        matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);
    }
    // Matix Type 3 (Tiled 4x 8x8 CJMCU)
    else if (config.matrixType == 3)
    {
        matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_BOTTOM + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_PROGRESSIVE);
    }

    ColorTemperature userColorTemp = GetUserColorTemp();
    LEDColorCorrection userLEDCorrection = GetUserColorCorrection();

    // Matrix Color Correction
    if (userLEDCorrection != UncorrectedColor)
    {
        FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, NUMMATRIX).setCorrection(userLEDCorrection);
    }
    else if (userColorTemp != UncorrectedTemperature)
    {
        FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, NUMMATRIX).setTemperature(userColorTemp);
    }
    else
    {
        int *rgbArray = GetUserCutomCorrection();
        FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, NUMMATRIX).setCorrection(matrix->Color(rgbArray[0], rgbArray[1], rgbArray[2]));
    }

    matrix->begin();
    matrix->setTextWrap(false);
    matrix->setBrightness(config.matrixBrightness);
    matrix->clear();

    // Bootscreen
    if (config.bootScreenAktiv)
    {
        ShowBootAnimation();
    }

    // config.Hostname
    if (config.hostname.isEmpty())
    {
        config.hostname = "PixelIt";
    }

	wifiManager.setAPCallback(EnteredHotspotCallback);
    // wifiManager.setSaveConfigCallback(WifiSetup);
	wifiManager.setMinimumSignalQuality();
	// Config menu timeout 180 seconds.
	wifiManager.setConfigPortalTimeout(180);

    if (!wifiManager.autoConnect("PIXEL_IT"))
    {
        Log(F("Setup"), F("Wifi failed to connect and hit timeout"));
        delay(3000);
        // Reset and try again, or maybe put it to deep sleep
        ESP.restart();
        delay(5000);
    }

    Log(F("Setup"), F("Wifi connected...yay :)"));

    Log(F("Setup"), F("Local IP"));
    Log(F("Setup"), WiFi.localIP().toString());
    Log(F("Setup"), WiFi.gatewayIP().toString());
    Log(F("Setup"), WiFi.subnetMask().toString());

    Log(F("Setup"), F("Starting UDP"));
    udp.begin(2390);
    // Log(F("Setup"), "Local port: " + String(udp.localPort()));

    server.on(
        "/update", HTTP_POST, [](AsyncWebServerRequest *request)
        {
      if (Update.hasError())
      {
          request->send(500, F("Update failed!"), F("Please check your file and retry!"));
          return;
      }
      request->send(200, F("Update successful!"), F("Rebooting...")); 
    delay(500);
    ESP.restart(); },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
        {
            if (!index)
            {
                Log(F("Update"), F("OTA Update Start"));
#ifdef ESP8266
                Update.runAsync(true);
#endif
                Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
            }
            if (!Update.hasError())
                Update.write(data, len);
            if (final)
            {
                if (Update.end(true))
                {
                    Log(F("Update"), F("Update Success"));
                }
                else
                {
                    Log(F("Update"), F("Update Failed"));
                }
            }
        });

    server.on("/api/screen", HTTP_POST, HandleScreen);
    server.on("/api/luxsensor", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", GetLuxSensor()); });
    server.on("/api/brightness", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", GetBrightness()); });
    // Legacy
    server.on("/api/dhtsensor", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", GetSensor()); });
    server.on("/api/sensor", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", GetSensor()); });
    server.on("/api/buttons", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", GetButtons()); });
    server.on("/api/matrixinfo", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", GetMatrixInfo()); });
    // server.on(F("/api/soundinfo"), HTTP_GET, HandleGetSoundInfo);
    server.on("/api/config", HTTP_POST, HandleSetConfig);
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", GetConfig()); });
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, F("text/html"), mainPage); });

    // Just testing I can serve the non-working files from littlefs
    server.on("/littlefs", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", String(), false);
    });
    server.on("/littlefs/assets/index.63517168.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/assets/index.63517168.js", String(), false);
    });
    server.on("/littlefs/assets/index.c4033380.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/assets/index.c4033380.css", String(), false);
    });

    server.onNotFound(HandleNotFound);

    server.begin();

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    Log(F("Setup"), F("Webserver started"));

    if (mqttAktiv == true)
    {
        client.setServer(mqttServer.c_str(), mqttPort);
        client.setCallback(callback);
        client.setBufferSize(8000);
        Log(F("Setup"), F("MQTT started"));
    }

    softSerial = new SoftwareSerial(config.TranslatePin(config.dfpRXPin), config.TranslatePin(config.dfpTXPin));

    softSerial->begin(9600);
    Log(F("Setup"), F("Software Serial started"));

    mp3Player.begin(*softSerial);
    Log(F("Setup"), F("DFPlayer started"));
    mp3Player.volume(initialVolume);
}

void HandleNotFound(AsyncWebServerRequest *request)
{
    if (request->method() == HTTP_OPTIONS)
    {
        request->send(204);
    }

    // server.sendHeader("Location", "/update", true);
    request->send(302, F("text/plain"), "");
}

void HandleSetConfig(AsyncWebServerRequest *request)
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.parseObject(request->arg("plain"));

    // server.sendHeader(F("Connection"), F("close"));

    if (json.success())
    {
        Log(F("SetConfig"), "Incoming Json length: " + String(json.measureLength()));
        config.SetConfig(json);
        request->send(200, F("application/json"), F("{\"response\":\"OK\"}"));
        delay(500);
        ESP.restart();
    }
    else
    {
        request->send(406, F("application/json"), F("{\"response\":\"Not Acceptable\"}"));
    }
}

void loop()
{
    server.begin();
    webSocket.loop();

    // Reset GPIO based on the array, as far as something is present in the array.
    for (int i = 0; i < SET_GPIO_SIZE; i++)
    {
        if (setGPIOReset[i].gpio != -1)
        {
            if (setGPIOReset[i].resetMillis <= millis())
            {
                Log(F("ResetSetGPIO"), "Pos: " + String(i) + ", GPIO: " + String(setGPIOReset[i].gpio) + ", Value: false");
                digitalWrite(setGPIOReset[i].gpio, false);
                setGPIOReset[i].gpio = -1;
                setGPIOReset[i].resetMillis = -1;
            }
        }
    }

    if (mqttAktiv == true)
    {
        if (!client.connected())
        {
            // MQTT connect
            if (mqttLastReconnectAttempt == 0 || (millis() - mqttLastReconnectAttempt) >= MQTT_RECONNECT_INTERVAL)
            {
                mqttLastReconnectAttempt = millis();

                // try to reconnect
                if (MQTTreconnect())
                {
                    mqttLastReconnectAttempt = 0;
                }
            }
        }
        else
        {
            client.loop();
        }
    }

    // Check buttons
    for (uint b = 0; b < 3; b++)
    {
        if (config.btnEnabled[b])
        {
            if ((btnState[b] == btnState_Released) && (digitalRead(config.TranslatePin(config.btnPin[b])) == config.btnPressedLevel[b]))
            {
                btnState[b] = btnState_PressedNew;
            }
            if ((btnState[b] == btnState_PressedBefore) && (digitalRead(config.TranslatePin(config.btnPin[b])) != config.btnPressedLevel[b]))
            {
                btnState[b] = btnState_Released;
            }
            if (btnState[b] == btnState_PressedNew)
            {
                btnState[b] = btnState_PressedBefore;
                HandleAndSendButtonPress(b);
            }
        }
    }

    // Clock Auto Fallback
    if (!config.sleepMode && ((config.clockAutoFallbackActive && !config.clockAktiv && millis() - lastScreenMessageMillis >= (config.clockAutoFallbackTime * 1000)) || config.forceClock))
    {
        config.forceClock = false;
        config.scrollTextAktivLoop = false;
        animateBMPAktivLoop = false;

        int performWipe = 0;

        switch (config.clockAutoFallbackAnimation)
        {
        case 1:
        case 2:
        case 3:
            performWipe = config.clockAutoFallbackAnimation;
            break;
        case 4:
            performWipe = (millis() % 3) + 1;
            break;
        default:;
        }

        if (performWipe == 1)
        {
            FadeOut();
        }
        else if (performWipe == 2)
        {
            ColoredBarWipe();
        }
        else if (performWipe == 3)
        {
            ZigZagWipe(config.clockColorR, config.clockColorG, config.clockColorB);
        }
        config.clockAktiv = true;
        config.clockCounterClock = 0;
        config.clockCounterDate = 0;
        DrawClock(true);

        if (performWipe != 0)
        {
            FadeIn();
        }
    }

    if (config.clockAktiv && now() != clockLastUpdate)
    {
        if (timeStatus() == timeNotSet && ntpTimeOut <= millis())
        {
            if (ntpRetryCounter >= NTP_MAX_RETRYS)
            {
                ntpTimeOut = (millis() + (NTP_TIMEOUT_SEC * 1000));
                ntpRetryCounter = 0;
                Log(F("Sync TimeServer"), "sync timeout for " + String(NTP_TIMEOUT_SEC) + " seconds!");
            }
            else
            {
                Log(F("Sync TimeServer"), config.ntpServer + " waiting for sync");
                setSyncProvider(getNtpTime);
            }
        }
        clockLastUpdate = now();
        DrawClock(false);
    }

    if (millis() - sendLuxPrevMillis >= 1000)
    {
        sendLuxPrevMillis = millis();

        LuxSensorData luxData = sensors.GetLuxSensorData();

        SendLDR(false);

        if (!config.sleepMode && config.matrixBrightnessAutomatic)
        {
            float newBrightness = map(luxData.lux, config.mbaLuxMin, config.mbaLuxMax, config.mbaDimMin, config.mbaDimMax);
            // Max brightness 255
            if (newBrightness > 255)
            {
                newBrightness = 255;
            }
            // Min brightness 0
            if (newBrightness < 0)
            {
                newBrightness = 0;
            }

            if (newBrightness != config.matrixBrightness)
            {
                SetCurrentMatrixBrightness(newBrightness);
                Log(F("Auto Brightness"), "Lux: " + String(luxData.lux) + " set brightness to " + String(config.matrixBrightness));
            }
        }
    }

    if (millis() - sendSensorPrevMillis >= 3000)
    {
        sendSensorPrevMillis = millis();
        SendSensor(false);
    }

    if (millis() - sendInfoPrevMillis >= 3000)
    {
        sendInfoPrevMillis = millis();
        SendMatrixInfo(false);
        // SendMp3PlayerInfo(false);
    }

    if (animateBMPAktivLoop && millis() - animateBMPPrevMillis >= animateBMPDelay)
    {
        animateBMPPrevMillis = millis();
        AnimateBMP(true);
    }

    if (config.scrollTextAktivLoop && millis() - scrollTextPrevMillis >= config.scrollTextDelay)
    {
        scrollTextPrevMillis = millis();
        ScrollText(false);
    }
}

void SendMatrixInfo(bool force)
{
    if (force)
    {
        oldGetMatrixInfo = "";
    }

    String matrixInfo;

    // Prüfen ob die ermittlung der MatrixInfo überhaupt erforderlich ist
    if ((mqttAktiv == true && client.connected()) || (webSocket.connectedClients() > 0))
    {
        matrixInfo = GetMatrixInfo();
    }
    // Prüfen ob über MQTT versendet werden muss
    if (mqttAktiv == true && client.connected() && oldGetMatrixInfo != matrixInfo)
    {
        client.publish((mqttMasterTopic + "matrixinfo").c_str(), matrixInfo.c_str(), true);
    }
    // Prüfen ob über Websocket versendet werden muss
    if (webSocket.connectedClients() > 0 && oldGetMatrixInfo != matrixInfo)
    {
        for (uint i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            webSocket.sendTXT(i, "{\"sysinfo\":" + matrixInfo + "}");
        }
    }

    oldGetMatrixInfo = matrixInfo;
}

void SendLDR(bool force)
{
    if (force)
    {
        oldGetLuxSensor = "";
    }

    String luxSensor;

    // Prüfen ob die Abfrage des LuxSensor überhaupt erforderlich ist
    if ((mqttAktiv == true && client.connected()) || (webSocket.connectedClients() > 0))
    {
        luxSensor = GetLuxSensor();
    }
    // Prüfen ob über MQTT versendet werden muss
    if (mqttAktiv == true && client.connected() && oldGetLuxSensor != luxSensor)
    {
        client.publish((mqttMasterTopic + "luxsensor").c_str(), luxSensor.c_str(), true);
    }
    // Prüfen ob über Websocket versendet werden muss
    if (webSocket.connectedClients() > 0 && oldGetLuxSensor != luxSensor)
    {
        for (unsigned int i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            webSocket.sendTXT(i, "{\"sensor\":" + luxSensor + "}");
        }
    }

    oldGetLuxSensor = luxSensor;
}

void SendSensor(bool force)
{
    if (force)
    {
        oldGetSensor = "";
    }

    String Sensor;

    // Prüfen ob die Abfrage des LuxSensor überhaupt erforderlich ist
    if ((mqttAktiv == true && client.connected()) || (webSocket.connectedClients() > 0))
    {
        Sensor = GetSensor();
    }
    // Prüfen ob über MQTT versendet werden muss
    if (mqttAktiv == true && client.connected() && oldGetSensor != Sensor)
    {
        client.publish((mqttMasterTopic + "dhtsensor").c_str(), Sensor.c_str(), true); // Legancy
        client.publish((mqttMasterTopic + "sensor").c_str(), Sensor.c_str(), true);
    }
    // Prüfen ob über Websocket versendet werden muss
    if (webSocket.connectedClients() > 0 && oldGetSensor != Sensor)
    {
        for (uint i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            webSocket.sendTXT(i, "{\"sensor\":" + Sensor + "}");
        }
    }

    oldGetSensor = Sensor;
}

void SendConfig()
{
    if (webSocket.connectedClients() > 0)
    {
        for (uint i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            String config = GetConfig();
            webSocket.sendTXT(i, "{\"config\":" + config + "}");
        }
    }
}

/////////////////////////////////////////////////////////////////////
/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

time_t getNtpTime()
{
    while (udp.parsePacket() > 0)
        ;
    sendNTPpacket(config.ntpServer);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500)
    {
        int size = udp.parsePacket();
        if (size >= NTP_PACKET_SIZE)
        {
            udp.read(packetBuffer, NTP_PACKET_SIZE);
            time_t secsSince1900;

            secsSince1900 = (time_t)packetBuffer[40] << 24;
            secsSince1900 |= (time_t)packetBuffer[41] << 16;
            secsSince1900 |= (time_t)packetBuffer[42] << 8;
            secsSince1900 |= (time_t)packetBuffer[43];
            time_t secsSince1970 = secsSince1900 - 2208988800UL;
            float totalOffset = config.clockTimeZone;
            if (config.clockDayLightSaving)
            {
                totalOffset = (config.clockTimeZone + DSToffset(secsSince1970, config.clockTimeZone));
            }
            return secsSince1970 + (time_t)(totalOffset * SECS_PER_HOUR);
            ntpRetryCounter = 0;
        }
        yield();
    }
    ntpRetryCounter++;
    return 0;
}
void sendNTPpacket(String &address)
{
    memset(packetBuffer, 0, NTP_PACKET_SIZE);

    packetBuffer[0] = 0b11100011;
    packetBuffer[1] = 0;
    packetBuffer[2] = 6;
    packetBuffer[3] = 0xEC;

    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    udp.beginPacket(address.c_str(), 123);
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
}

void Log(String function, String message)
{

    String timeStamp = IntFormat(year()) + "-" + IntFormat(month()) + "-" + IntFormat(day()) + "T" + IntFormat(hour()) + ":" + IntFormat(minute()) + ":" + IntFormat(second());

    Serial.println("[" + timeStamp + "] " + function + ": " + message);

    // Prüfen ob über Websocket versendet werden muss
    if (webSocket.connectedClients() > 0)
    {
        for (unsigned int i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            String payload = "{\"log\":{\"timeStamp\":\"" + timeStamp + "\",\"function\":\"" + function + "\",\"message\":\"" + message + "\"}}";
            webSocket.sendTXT(i, payload);
        }
    }
}
