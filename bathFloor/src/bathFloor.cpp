/*
	Thermostat module based on ESP8266 SoC.

	Features:
	- DS1820 temperature control sensor.
	- AC 220V power control.
	- REST API to monitor/contol heating parameters.

	Toolchain: PlatformIO.

	By denis.afanassiev@gmail.com
*/

#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <temperatureSensor.h>
#include <Timer.h>
#include <OTA.h>

#define ONE_WIRE_PIN            5
#define AC_CONTROL_PIN          13
#define WEB_SERVER_PORT         80
#define UPDATE_TEMP_EVERY       (5000L)         // every 5 sec
#define CHECK_HEATING_EVERY     (60000L)        // every 1 min
#define CHECK_FIRMWARE_EVERY	(5000L)		// debug: every 5 sec
#define EEPROM_INIT_CODE        28465
#define SSID_LEN                80
#define SECRET_LEN              80
#define DEFAULT_FLOOR_TEMP      28.0
#define DEFAULT_ACTIVE		0
#define MDNS_HOST               "HB-THERMOSTAT"

const char* fwUrlBase = "http://192.168.1.200/firmware/ShHarbor/thermostat/";
const int FW_VERSION = 810;

void saveConfiguration();

struct ControllerData
{
	TemperatureSensor*      temperatureSensor;
	ESP8266WebServer*       thermostatServer;
	MDNSResponder*          mdns;
	Timer*                  timer;
	uint8_t                 heatingOn;
} GD;

struct ConfigurationData
{
	int16_t                 initialised;
	char                    ssid[SSID_LEN + 1];
	char                    secret[SECRET_LEN + 1];
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
		saveConfiguration();
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
		saveConfiguration();
		gd->thermostatServer->send(200, "application/json",
			"Updated to: " + activeParam + "\r\n");
	}
	else
	{
		gd->thermostatServer->send(401, "text/html",
			"Wrong value: " + activeParam + "\r\n");
	}
}

// Get character sting from terminal.
int readString(char* buff, size_t buffSize)
{
	memset(buff, '\0', buffSize);

	size_t count = 0;
	while (count < buffSize)
	{
		if (Serial.available())
		{
			char c = Serial.read();
			if (isprint(c))
			{
				buff[count++] = c;
				Serial.print(c);
			}
			else if (count > 0 && (c == '\r' || c == '\n'))
			{
				Serial.println();
				return 1;
			}
		}
		yield();
	}
	return 0; // reached end of buffer
}

// Save controller configuration to EEPROM
void saveConfiguration()
{
	EEPROM.begin(sizeof(config));
	EEPROM.put(0, config);
	EEPROM.commit();
	EEPROM.end();
}

// Get configuration data interactively and save to EEPROM.
void getUserConfiguration()
{
	// Ask user for config values
	Serial.print("SSID: ");
	readString(config.ssid, SSID_LEN);
	Serial.print("Secret: ");
	readString(config.secret, SECRET_LEN);

	// Defaults
	config.targetTemp = DEFAULT_FLOOR_TEMP;
	config.active = DEFAULT_ACTIVE;

	// Initisalised flag
	config.initialised = EEPROM_INIT_CODE;

	// And put it back to EEPROM for the next time
	saveConfiguration();

	Serial.println("Restarting...");
	ESP.restart();
}

// Getting configuration either from EEPROM or from console.
void loadConfiguration()
{
	// Read what's in EEPROM
	EEPROM.begin(sizeof(config));
	EEPROM.get(0, config);
	EEPROM.end();

	// // Debug:
	// Serial.printf("SSID configured: %s\n", config.ssid);
	// Serial.printf("Secret conigured: %s\n", config.secret);

	// Check if it has a proper signature
	Serial.println("Press any key to start configuration...");
	if (EEPROM_INIT_CODE != config.initialised || Serial.readString() != "")
	{
		getUserConfiguration();
	}
	Serial.printf("SSID to connect: %s\n", config.ssid);
}

// Either WiFi is connected or restart.
void makeSureWiFiConnected()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	if (WL_CONNECTED != WiFi.status())
	{
		Serial.println("Disconnected.");
		WiFi.begin(config.ssid, config.secret);
		Serial.print("Connecting to WiFi hotspot: ");

		// Wait for connection
		int connectionAttempts = 0;
		while (WiFi.status() != WL_CONNECTED)
		{
			delay(500);
			Serial.print(".");
			if (++connectionAttempts > 40)
			{
				Serial.println("Unable to connect.");
				ESP.restart();
			}
		}
		Serial.println();
		Serial.printf("Connected to: %s\n", config.ssid);
		Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());

		if (gd->mdns->begin(MDNS_HOST, WiFi.localIP()))
		{
			Serial.println("MDNS responder started.");
		}
	}
}

// Go check if there is a new firmware version got available.
void checkFirmwareUpdates()
{
	// pass current FW version and base URL to lookup
	checkForUpdates(FW_VERSION, fwUrlBase);
}

void setup()
{
	Serial.begin(115200);
	Serial.println("Initialisation.");
	Serial.printf("Bath floor controller build %d.\n", FW_VERSION);

	Serial.println("Configuration loading.");
	loadConfiguration();

	// Warning: uses global data
	ControllerData *gd = &GD;

	gd->thermostatServer = new ESP8266WebServer(WEB_SERVER_PORT);
	gd->temperatureSensor = new TemperatureSensor(ONE_WIRE_PIN);
	gd->timer = new Timer();
	gd->mdns = new MDNSResponder();

	makeSureWiFiConnected();

	gd->thermostatServer->on("/Status", HTTPMethod::HTTP_GET, HandleHTTPGetStatus);
	gd->thermostatServer->on("/TargetTemperature", HTTPMethod::HTTP_PUT, HandleHTTPTargetTemperature);
	gd->thermostatServer->on("/Active", HTTPMethod::HTTP_PUT, HandleHTTPActive);

	gd->thermostatServer->begin();
	Serial.println("HTTP server started.");

	// Set up regulars
	gd->timer->every(UPDATE_TEMP_EVERY, temperatureUpdate);
	gd->timer->every(CHECK_HEATING_EVERY, controlHeating);
	gd->timer->every(CHECK_FIRMWARE_EVERY, checkFirmwareUpdates);

	pinMode(AC_CONTROL_PIN, OUTPUT);
}

void loop()
{
	ControllerData *gd = &GD;

	makeSureWiFiConnected();
	gd->thermostatServer->handleClient();
	gd->timer->update();
}
