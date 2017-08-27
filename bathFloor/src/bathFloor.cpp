
/*
	Bath floor temperature control module based on ESP8266 SoC.

	Features:
	- DS1820 temperature control sensor.
	- AC 220V power control.
	- REST API to monitor/contol heating paramenters.

	Toolchain: PlatformIO.

	By den@dataart.com
*/

#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <temperatureSensor.h>
#include <Timer.h>

#define ONE_WIRE_PIN            5
#define AC_CONTROL_PIN          13
#define WEB_SERVER_PORT         80
#define UPDATE_TEMP_EVERY       (5000L)         // every 5 sec
#define CHECK_HEATING_EVERY     (60000L)        // every 1 min
#define EEPROM_INIT_CODE        28465
#define SSID_LEN                80
#define SECRET_LEN              80
#define DEFAULT_FLOOR_TEMP      28.0
#define BUILD_VERSION           "0.4.1"
#define MDNS_HOST               "HB-BATH-FLOOR"

void saveConfiguration();

struct ControllerData
{
	TemperatureSensor*      temperatureSensor;
	ESP8266WebServer*       floorServer;
	MDNSResponder*          mdns;
	Timer*                  timer;
	uint8_t                 heatingOn;
} GD;

struct ConfigurationData
{
	int16_t                 initialised;
	char                    ssid[SSID_LEN + 1];
	char                    secret[SECRET_LEN + 1];
	float                   floorTargetTemp;
} config;

// Go to floor sensor and get current temperature.
float getFloorTemperature()
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

	float temp = getFloorTemperature();
	Serial.printf("Floor temperature: %d.%02d\n", (int)temp, (int)(temp*100)%100);
}

// This controls floor heating.
void floorHeatingControl()
{
	// Warning: uses global data.
	ControllerData *gd = &GD;

	gd->heatingOn = getFloorTemperature() < config.floorTargetTemp;
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
	"\"FloorTemperature\" : " + String(getFloorTemperature(), 2) +
	", " +
	"\"TargetTemperature\" : " + String(config.floorTargetTemp, 2) +
	", " +
	"\"Heating\" : " + String(gd->heatingOn) +
	" }\r\n";

	gd->floorServer->send(200, "application/json", json);
}

void HandleHTTPTargetTemperature()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	String param = gd->floorServer->arg("temp");
	float temperature = param.toFloat();
	if (temperature > 0.0 && temperature < 100.0)
	{
		config.floorTargetTemp = temperature;
		saveConfiguration();
		gd->floorServer->send(200, "application/json",
		"Updated to: " + param + "\r\n");
	}
	else
	{
		gd->floorServer->send(401, "text/html",
		"Wrong value: " + param + "\r\n");
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

	config.floorTargetTemp = DEFAULT_FLOOR_TEMP;

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

void setup()
{
	Serial.begin(115200);
	Serial.println("Initialisation.");
	Serial.printf("Bath floor controller version %s.\n", BUILD_VERSION);

	Serial.println("Configuration loading.");
	loadConfiguration();

	// Warning: uses global data
	ControllerData *gd = &GD;

	gd->floorServer = new ESP8266WebServer(WEB_SERVER_PORT);
	gd->temperatureSensor = new TemperatureSensor(ONE_WIRE_PIN);
	gd->timer = new Timer();
	gd->mdns = new MDNSResponder();

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
			//getUserConfiguration();
		}
	}
	Serial.println();
	Serial.printf("Connected to: %s\n", config.ssid);
	Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());

	if (gd->mdns->begin(MDNS_HOST, WiFi.localIP()))
	{
		Serial.println("MDNS responder started");
	}

	gd->floorServer->on("/Status", HTTPMethod::HTTP_GET, HandleHTTPGetStatus);
	gd->floorServer->on("/TargetTemperature", HTTPMethod::HTTP_PUT, HandleHTTPTargetTemperature);

	gd->floorServer->begin();
	Serial.println("HTTP server started.");

	// Set up regulars
	gd->timer->every(UPDATE_TEMP_EVERY, temperatureUpdate);
	gd->timer->every(CHECK_HEATING_EVERY, floorHeatingControl);

	pinMode(AC_CONTROL_PIN, OUTPUT);
}

void loop()
{
	ControllerData *gd = &GD;

	gd->floorServer->handleClient();
	gd->timer->update();
}
