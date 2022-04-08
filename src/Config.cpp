#include "Config.h"
#include "ColorConverterLib.h"
#if defined(ESP8266)
#include <LittleFS.h>
#elif defined(ESP32)
#include <FS.h>
#endif

String Config::_version = "";
bool Config::_isESP8266 = true;

Config::Config(String version, bool isESP8266) {
    _version = version;
    _isESP8266 = isESP8266;
}

void Config::SetLogDelegate(void (*log)(String, String)){
    _logDelegate = log;
}

void Config::SetBrightnessDelegate(void (*setBrightness)(float)){
    _setBrightnessDelegate = setBrightness;
}

void Config::Log(String function, String message) {
    if (0 != _logDelegate){
        (*_logDelegate)(function, message);
    }
}

void Config::SaveConfig()
{
	// save the custom parameters to FS
	DynamicJsonBuffer jsonBuffer;
	JsonObject &json = jsonBuffer.createObject();

	json["version"] = _version;
	json["isESP8266"] = _isESP8266;
	json["temperatureUnit"] = static_cast<int>(temperatureUnit);
	json["matrixBrightnessAutomatic"] = matrixBrightnessAutomatic;
	json["mbaDimMin"] = mbaDimMin;
	json["mbaDimMax"] = mbaDimMax;
	json["mbaLuxMin"] = mbaLuxMin;
	json["mbaLuxMax"] = mbaLuxMax;
	json["matrixBrightness"] = matrixBrightness;
	json["matrixType"] = matrixType;
	json["note"] = note;
	json["hostname"] = hostname;
	json["matrixTempCorrection"] = matrixTempCorrection;
	json["ntpServer"] = ntpServer;
	json["clockTimeZone"] = clockTimeZone;

	String clockColorHex;
	ColorConverter::RgbToHex(clockColorR, clockColorG, clockColorB, clockColorHex);
	json["clockColor"] = "#" + clockColorHex;

	json["clockSwitchAktiv"] = clockSwitchAktiv;
	json["clockSwitchSec"] = clockSwitchSec;
	json["clock24Hours"] = clock24Hours;
	json["clockDayLightSaving"] = clockDayLightSaving;
	json["clockWithSeconds"] = clockWithSeconds;
	json["clockAutoFallbackActive"] = clockAutoFallbackActive;
	json["clockAutoFallbackTime"] = clockAutoFallbackTime;
	json["clockAutoFallbackAnimation"] = clockAutoFallbackAnimation;
	json["clockDateDayMonth"] = clockDateDayMonth;
	json["clockDayOfWeekFirstMonday"] = clockDayOfWeekFirstMonday;
	json["scrollTextDefaultDelay"] = scrollTextDefaultDelay;
	json["bootScreenAktiv"] = bootScreenAktiv;
	json["mqttAktiv"] = mqttAktiv;
	json["mqttUser"] = mqttUser;
	json["mqttPassword"] = mqttPassword;
	json["mqttServer"] = mqttServer;
	json["mqttMasterTopic"] = mqttMasterTopic;
	json["mqttPort"] = mqttPort;
	json["luxOffset"] = luxOffset;
	json["temperatureOffset"] = temperatureOffset;
	json["humidityOffset"] = humidityOffset;
	json["pressureOffset"] = pressureOffset;
	json["gasOffset"] = gasOffset;

	json["dfpRXpin"] = dfpRXPin;
	json["dfpTXpin"] = dfpTXPin;
	json["onewirePin"] = onewirePin;
	json["SCLPin"] = SCLPin;
	json["SDAPin"] = SDAPin;
	for (uint b = 0; b < 3; b++)
	{
		json["btn" + String(b) + "Pin"] = btnPin[b];
		json["btn" + String(b) + "PressedLevel"] = btnPressedLevel[b];
		json["btn" + String(b) + "Enabled"] = btnEnabled[b];
		json["btn" + String(b) + "Action"] = static_cast<int>(btnAction[b]);
	}
	json["ldrDevice"] = ldrDevice;
	json["ldrPulldown"] = ldrPulldown;
	json["ldrSmoothing"] = ldrSmoothing;

	json["initialVolume"] = initialVolume;

#if defined(ESP8266)
	File configFile = LittleFS.open("/config.json", "w");
#elif defined(ESP32)
	File configFile = SPIFFS.open("/config.json", "w");
#endif
	json.printTo(configFile);
	configFile.close();
	Log("SaveConfig", "Saved");
	// end save
}

void Config::LoadConfig()
{
	// file exists, reading and loading
#if defined(ESP8266)
	if (LittleFS.exists("/config.json"))
	{
		File configFile = LittleFS.open("/config.json", "r");
#elif defined(ESP32)
	if (SPIFFS.exists("/config.json"))
	{
		File configFile = SPIFFS.open("/config.json", "r");
#endif
		if (configFile)
		{
			// Serial.println("opened config file");

			DynamicJsonBuffer jsonBuffer;
			JsonObject &json = jsonBuffer.parseObject(configFile);

			if (json.success())
			{
				SetConfigVariables(json);
				Log("LoadConfig", "Loaded");
			}
		}
	}
	else
	{
		Log("LoadConfig", "No Configfile, init new file");
		SaveConfig();
	}
}

void Config::SetConfig(JsonObject &json)
{
	SetConfigVariables(json);
	SaveConfig();
}

void Config::SetConfigVariables(JsonObject &json)
{
	if (json.containsKey("version"))
	{
		optionsVersion = json["version"].as<String>();
	}

	if (json.containsKey("temperatureUnit"))
	{
		temperatureUnit = static_cast<TemperatureUnit>(json["temperatureUnit"].as<int>());
	}

	if (json.containsKey("matrixBrightnessAutomatic"))
	{
		matrixBrightnessAutomatic = json["matrixBrightnessAutomatic"].as<bool>();
	}

	if (json.containsKey("mbaDimMin"))
	{
		mbaDimMin = json["mbaDimMin"].as<int>();
	}

	if (json.containsKey("mbaDimMax"))
	{
		mbaDimMax = json["mbaDimMax"].as<int>();
	}

	if (json.containsKey("mbaLuxMin"))
	{
		mbaLuxMin = json["mbaLuxMin"].as<int>();
	}

	if (json.containsKey("mbaLuxMax"))
	{
		mbaLuxMax = json["mbaLuxMax"].as<int>();
	}

	if (json.containsKey("matrixBrightness"))
	{
        matrixBrightness = json["matrixBrightness"].as<float>();
		if (0 != _setBrightnessDelegate) {
			(*_setBrightnessDelegate)(matrixBrightness);
		}
	}

	if (json.containsKey("matrixType"))
	{
		matrixType = json["matrixType"].as<int>();
	}

	if (json.containsKey("note"))
	{
		note = json["note"].as<char *>();
	}

	if (json.containsKey("hostname"))
	{
		String hostname_raw = json["hostname"].as<char *>();
		hostname = "";
		for (uint8_t n = 0; n < hostname_raw.length(); n++)
		{
			if ((hostname_raw.charAt(n) >= '0' && hostname_raw.charAt(n) <= '9') || (hostname_raw.charAt(n) >= 'A' && hostname_raw.charAt(n) <= 'Z') || (hostname_raw.charAt(n) >= 'a' && hostname_raw.charAt(n) <= 'z') || (hostname_raw.charAt(n) == '_') || (hostname_raw.charAt(n) == '-'))
				hostname += hostname_raw.charAt(n);
		}
		if (hostname.isEmpty())
		{
			hostname = "PixelIt";
		}
	}

	if (json.containsKey("matrixTempCorrection"))
	{
		matrixTempCorrection = json["matrixTempCorrection"].as<char *>();
	}

	if (json.containsKey("ntpServer"))
	{
		ntpServer = json["ntpServer"].as<char *>();
	}

	if (json.containsKey("clockTimeZone"))
	{
		clockTimeZone = json["clockTimeZone"].as<float>();
	}

	if (json.containsKey("clockColor"))
	{
		ColorConverter::HexToRgb(json["clockColor"].as<String>(), clockColorR, clockColorG, clockColorB);
	}

	if (json.containsKey("clockSwitchAktiv"))
	{
		clockSwitchAktiv = json["clockSwitchAktiv"].as<bool>();
	}

	if (json.containsKey("clockSwitchSec"))
	{
		clockSwitchSec = json["clockSwitchSec"].as<uint>();
	}

	if (json.containsKey("clock24Hours"))
	{
		clock24Hours = json["clock24Hours"].as<bool>();
	}

	if (json.containsKey("clockDayLightSaving"))
	{
		clockDayLightSaving = json["clockDayLightSaving"].as<bool>();
	}

	if (json.containsKey("clockWithSeconds"))
	{
		clockWithSeconds = json["clockWithSeconds"].as<bool>();
	}

	if (json.containsKey("clockAutoFallbackActive"))
	{
		clockAutoFallbackActive = json["clockAutoFallbackActive"].as<bool>();
	}

	if (json.containsKey("clockAutoFallbackAnimation"))
	{
		clockAutoFallbackAnimation = json["clockAutoFallbackAnimation"].as<uint>();
	}

	if (json.containsKey("clockAutoFallbackTime"))
	{
		clockAutoFallbackTime = json["clockAutoFallbackTime"].as<uint>();
	}

	if (json.containsKey("clockDateDayMonth"))
	{
		clockDateDayMonth = json["clockDateDayMonth"].as<bool>();
	}

	if (json.containsKey("clockDayOfWeekFirstMonday"))
	{
		clockDayOfWeekFirstMonday = json["clockDayOfWeekFirstMonday"].as<bool>();
	}
	
	if (json.containsKey("scrollTextDefaultDelay"))
	{
		scrollTextDefaultDelay = json["scrollTextDefaultDelay"].as<uint>();
	}

	if (json.containsKey("bootScreenAktiv"))
	{
		bootScreenAktiv = json["bootScreenAktiv"].as<bool>();
	}

	if (json.containsKey("mqttAktiv"))
	{
		mqttAktiv = json["mqttAktiv"].as<bool>();
	}

	if (json.containsKey("mqttUser"))
	{
		mqttUser = json["mqttUser"].as<char *>();
	}

	if (json.containsKey("mqttPassword"))
	{
		mqttPassword = json["mqttPassword"].as<char *>();
	}

	if (json.containsKey("mqttServer"))
	{
		mqttServer = json["mqttServer"].as<char *>();
	}

	if (json.containsKey("mqttMasterTopic"))
	{
		mqttMasterTopic = json["mqttMasterTopic"].as<char *>();
	}

	if (json.containsKey("mqttPort"))
	{
		mqttPort = json["mqttPort"].as<int>();
	}

	if (json.containsKey("luxOffset"))
	{
		luxOffset = json["luxOffset"].as<float>();
	}

	if (json.containsKey("temperatureOffset"))
	{
		temperatureOffset = json["temperatureOffset"].as<float>();
	}

	if (json.containsKey("humidityOffset"))
	{
		humidityOffset = json["humidityOffset"].as<float>();
	}

	if (json.containsKey("pressureOffset"))
	{
		pressureOffset = json["pressureOffset"].as<float>();
	}

	if (json.containsKey("gasOffset"))
	{
		gasOffset = json["gasOffset"].as<float>();
	}

	if (json.containsKey("dfpRXpin"))
	{
		dfpRXPin = json["dfpRXpin"].as<char *>();
	}

	if (json.containsKey("dfpTXpin"))
	{
		dfpTXPin = json["dfpTXpin"].as<char *>();
	}

	if (json.containsKey("onewirePin"))
	{
		onewirePin = json["onewirePin"].as<char *>();
	}

	if (json.containsKey("SCLPin"))
	{
		SCLPin = json["SCLPin"].as<char *>();
	}

	if (json.containsKey("SDAPin"))
	{
		SDAPin = json["SDAPin"].as<char *>();
	}

	for (uint b = 0; b < 3; b++)
	{
		if (json.containsKey("btn" + String(b) + "Pin"))
		{
			btnPin[b] = json["btn" + String(b) + "Pin"].as<char *>();
		}
		if (json.containsKey("btn" + String(b) + "PressedLevel"))
		{
			btnPressedLevel[b] = json["btn" + String(b) + "PressedLevel"].as<int>();
		}
		if (json.containsKey("btn" + String(b) + "Enabled"))
		{
			btnEnabled[b] = json["btn" + String(b) + "Enabled"].as<bool>();
		}
		if (json.containsKey("btn" + String(b) + "Action"))
		{
			btnAction[b] = static_cast<btnActions>(json["btn" + String(b) + "Action"].as<int>());
		}
	}

	if (json.containsKey("ldrDevice"))
	{
		ldrDevice = json["ldrDevice"].as<char *>();
	}

	if (json.containsKey("ldrPulldown"))
	{
		ldrPulldown = json["ldrPulldown"].as<unsigned long>();
	}

	if (json.containsKey("ldrSmoothing"))
	{
		ldrSmoothing = json["ldrSmoothing"].as<uint>();
	}

	if (json.containsKey("initialVolume"))
	{
		initialVolume = json["initialVolume"].as<uint>();
	}
}