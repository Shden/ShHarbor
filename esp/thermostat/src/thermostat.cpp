/*
	Thermostat module based on ESP8266 SoC.

	Features:
	- DS1820 temperature control sensor.
	- AC 220V power control.
	- REST API to monitor/contol heating parameters.
	- OTA firmware update.
	- Built in configuration web UI at /config

	Toolchain: PlatformIO.

	By denis.afanassiev@gmail.com
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Timer.h>
#include <DS1820.h>
#include <OTA.h>
#include <config.h>
#include <wifi.h>
#include <ESPTemplateProcessor.h>

#define ONE_WIRE_PIN            5
#define AC_CONTROL_PIN          13
#define WEB_SERVER_PORT         80
#define UPDATE_TEMP_EVERY       (5000L)         // every 5 sec
#define CHECK_HEATING_EVERY     (60000L)        // every 1 min
#define CHECK_SW_UPDATES_EVERY	(60000L*5)	// every 5 min
#define DEFAULT_TARGET_TEMP	28.0
#define DEFAULT_ACTIVE		0
#define MDNS_HOST               "HB-THERMOSTAT"

const char* FW_URL_BASE = "http://192.168.1.200/firmware/ShHarbor/thermostat/";

void checkSoftwareUpdates();

struct ControllerData
{
	TemperatureSensor*      temperatureSensor;
	ESP8266WebServer*       thermostatServer;
	MDNSResponder*          mdns;
	Timer*                  timer;
	uint8_t                 heatingOn;
} GD;

// will have ssid, secret, initialised, MDNSHost plus what is defined here
struct ConfigurationData : ConnectedESPConfiguration
{
	float                   targetTemp;
	int8_t			active;
} config;

// Go to sensor and get current temperature.
float getTemperature()
{
	// Warning: uses global data.
	ControllerData *gd = &GD;

	return gd->temperatureSensor->getTemperature();
}

// Update temperatureSensor internal data
void temperatureUpdate()
{
	// Warning: uses global data.
	ControllerData *gd = &GD;

	gd->temperatureSensor->updateTemperature();

	float temp = getTemperature();

	// Here goes workaround for %f which wasnt working right.
	Serial.printf("Temperature: %d.%02d\n", (int)temp, (int)(temp*100)%100);
}

// Heating control.
void controlHeating()
{
	// Warning: uses global data.
	ControllerData *gd = &GD;

	gd->heatingOn = config.active && getTemperature() < config.targetTemp;
	digitalWrite(AC_CONTROL_PIN, gd->heatingOn);
	Serial.printf("Heating state: %d\n", gd->heatingOn);
}

// HTTP GET /Status
void HandleHTTPGetStatus()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	String json =
	String("{ ") +
	"\"CurrentTemperature\" : " + String(getTemperature(), 2) +
	", " +
	"\"TargetTemperature\" : " + String(config.targetTemp, 2) +
	", " +
	"\"Active\" : " + String(config.active) +
	", " +
	"\"Heating\" : " + String(gd->heatingOn) +
	", " +
	"\"Build\" : " + String(FW_VERSION) +
	" }\r\n";

	//gd->thermostatServer->sendHeader("Access-Control-Allow-Origin", "*");
	gd->thermostatServer->send(200, "application/json", json);
}

// HTTP PUT /TargetTemperature
void HandleHTTPTargetTemperature()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	String param = gd->thermostatServer->arg("temp");
	float temperature = param.toFloat();
	if (temperature > 0.0 && temperature < 100.0)
	{
		config.targetTemp = temperature;
		saveConfiguration(&config, sizeof(config));
		gd->thermostatServer->send(200, "application/json",
			"Updated to: " + param + "\r\n");
	}
	else
	{
		gd->thermostatServer->send(401, "text/html",
			"Wrong value: " + param + "\r\n");
	}
}

// HTTP PUT /Active
void HandleHTTPActive()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	String activeParam = gd->thermostatServer->arg("active");
	int active = activeParam.toInt();
	if (active == 0 || active == 1)
	{
		config.active = active;
		saveConfiguration(&config, sizeof(config));
		gd->thermostatServer->send(200, "application/json",
			"Updated to: " + activeParam + "\r\n");
	}
	else
	{
		gd->thermostatServer->send(401, "text/html",
			"Wrong value: " + activeParam + "\r\n");
	}
}

// Maps config.html parameters to configuration values.
String mapConfigParameters(const String& key)
{
	if (key == "SSID") return String(config.ssid); else
	if (key == "PASS") return String(config.secret); else
	if (key == "MDNS") return String(config.MDNSHost); else
	if (key == "IP") return WiFi.localIP().toString(); else
	if (key == "BUILD") return String(FW_VERSION); else
	if (key == "T_TEMP") return String(config.targetTemp); else
	if (key == "CHECKED") return config.active ? "checked" : "";
}

// Debug request arguments printout.
void dbgPostPrintout()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	for (int i=0; i < gd->thermostatServer->args(); i++)
	{
		Serial.print(gd->thermostatServer->argName(i));
		Serial.print(": ");
		Serial.print(gd->thermostatServer->arg(i));
		Serial.println();
	}
}

// Handles HTTP GET /config.html request
void UpdateConfig()
{
	dbgPostPrintout();

	// Warning: uses global data
	ControllerData *gd = &GD;

	// NETWORK_UPDATE
	if (gd->thermostatServer->hasArg("NETWORK_UPDATE"))
	{
		gd->thermostatServer->arg("SSID").toCharArray(config.ssid, SSID_LEN);
		gd->thermostatServer->arg("PASS").toCharArray(config.secret, SECRET_LEN);
		gd->thermostatServer->arg("MDNS").toCharArray(config.MDNSHost, MDNS_HOST_LEN);

		saveConfiguration(&config, sizeof(config));

		// redirect to the same page without arguments
		gd->thermostatServer->sendHeader("Location", String("/config"), true);
		gd->thermostatServer->send(302, "text/plain", "");

		WiFi.disconnect();
	}

	// HEATING_UPDATE
	if (gd->thermostatServer->hasArg("HEATING_UPDATE"))
	{
		String param = gd->thermostatServer->arg("T_TEMP");
		float temperature = param.toFloat();
		if (temperature > 0.0 && temperature < 100.0)
			config.targetTemp = temperature;

		config.active = gd->thermostatServer->hasArg("ACTIVE");

		saveConfiguration(&config, sizeof(config));
	}

	ESPTemplateProcessor(*gd->thermostatServer).send(
		String("/config.html"),
		mapConfigParameters);
}

// Go check if there is a new firmware or SPIFFS got available.
void checkSoftwareUpdates()
{
	int spiffsVersion = 0;
	File versionInfo = SPIFFS.open("/version.info", "r");
	if (versionInfo)
	{
		spiffsVersion = versionInfo.parseInt();
		versionInfo.close();
	}

	updateAll(FW_VERSION, spiffsVersion, FW_URL_BASE);
}

void setup()
{
	Serial.begin(115200);
	Serial.println("Initialisation.");
	Serial.printf("Thermostat build %d.\n", FW_VERSION);

	Serial.println("Configuration loading.");
	loadConfiguration(&config, sizeof(config));

	// Warning: uses global data
	ControllerData *gd = &GD;

	gd->thermostatServer = new ESP8266WebServer(WEB_SERVER_PORT);
	gd->temperatureSensor = new TemperatureSensor(ONE_WIRE_PIN);
	gd->timer = new Timer();
	gd->mdns = new MDNSResponder();

	makeSureWiFiConnected(&config, gd->mdns);

	if (SPIFFS.begin())
		Serial.println("SPIFFS mount succesfull.");
	else
		Serial.println("SPIFFS mount failed.");

	gd->thermostatServer->on("/Status", HTTPMethod::HTTP_GET, HandleHTTPGetStatus);
	gd->thermostatServer->on("/TargetTemperature", HTTPMethod::HTTP_PUT, HandleHTTPTargetTemperature);
	gd->thermostatServer->on("/Active", HTTPMethod::HTTP_PUT, HandleHTTPActive);
	gd->thermostatServer->on("/config", UpdateConfig);

	gd->thermostatServer->begin();
	Serial.println("HTTP server started.");

	// Set up regulars
	gd->timer->every(UPDATE_TEMP_EVERY, temperatureUpdate);
	gd->timer->every(CHECK_HEATING_EVERY, controlHeating);
	gd->timer->every(CHECK_SW_UPDATES_EVERY, checkSoftwareUpdates);

	pinMode(AC_CONTROL_PIN, OUTPUT);
}

void loop()
{
	ControllerData *gd = &GD;

	makeSureWiFiConnected(&config, gd->mdns);
	gd->thermostatServer->handleClient();
	gd->timer->update();
}
