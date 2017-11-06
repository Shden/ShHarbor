/*
	Wall switch module based on ESP8266 SoC.

	Features:
	- Up to 3 channels (switch + light).
	- REST API to monitor/contol heating parameters.
	- OTA firmware update.

	Toolchain: PlatformIO.

	By denis.afanassiev@gmail.com
*/

#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Timer.h>
#include <OTA.h>

#define WEB_SERVER_PORT         80
#define CHECK_FIRMWARE_EVERY	(60000L*5)	// every 5 min
#define UPDATE_POWER_EVERRY	500		// every 500 ms
#define EEPROM_INIT_CODE        28465
#define SSID_LEN                80
#define SECRET_LEN              80
#define LINKED_SWITCH_ADDR_LEN	80
#define MDNS_HOST               "HB-SWITCH"
#define SW_LINES		3

// ESP-12 pins:
// inputs: U6, U7, U8
#define U6			5		// these two numbering
#define U7			4		// swapped for some reason
#define U8			2
// outputs: U3, U4, U5
#define U3			13
#define U4			12
#define U5			14

#define I1			U6
#define I2			U7
#define I3			U8

#define O1			U3
#define O2			U4
#define O3			U5

const char* fwUrlBase = "http://192.168.1.200/firmware/ShHarbor/switch/";
const int FW_VERSION = 600;

void saveConfiguration();

struct ControllerData
{
	ESP8266WebServer*       switchServer;
	MDNSResponder*          mdns;
	Timer*                  timer;
	int			remoteControlBits[SW_LINES];	// remote control bits by channels
	int			switchPins[SW_LINES];		// switch pins by channels
	int 			powerPins[SW_LINES];		// power pins by channels
} GD;

struct ConfigurationData
{
	int16_t                 initialised;
	char                    ssid[SSID_LEN + 1];
	char                    secret[SECRET_LEN + 1];
	char			linkedSwitchAddress[SW_LINES][LINKED_SWITCH_ADDR_LEN + 1];
} config;

// HTTP GET /Status
void HandleHTTPGetStatus()
{
	ControllerData *gd = &GD;

	String json =
		String("{ ") +
		"\"L1\" : { " +
			"\"Status\" : " + String(digitalRead(gd->powerPins[0])) + ", " +
			"\"Link\" : " + String(config.linkedSwitchAddress[0]) + ", } " +
		"\"L2\" : { " +
			"\"Status\" : " + String(digitalRead(gd->powerPins[1])) + ", " +
			"\"Link\" : " + String(config.linkedSwitchAddress[1]) + ", } " +
		"\"L3\" : { " +
			"\"Status\" : " + String(digitalRead(gd->powerPins[2])) + ", " +
			"\"Link\" : " + String(config.linkedSwitchAddress[2]) + ", } " +
		"\"Build\" : " + String(FW_VERSION) +
		" }\r\n";

	gd->switchServer->send(200, "application/json", json);
}

// HTTP PUT /ChangeLine
void HandleHTTPChangeLine()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	String line = gd->switchServer->arg("line");
	String newState = gd->switchServer->arg("state");

	int lineNum = line.toInt();
	int newStateVal = newState.toInt();

	if (lineNum >= 1 && lineNum <= 3)
	{
		if (newStateVal >= 0 && newStateVal <= 1)
		{
			int currentLineState =
				(digitalRead(gd->powerPins[lineNum - 1]) == HIGH)
				? 1 : 0;
			if (currentLineState != newStateVal)
				gd->remoteControlBits[lineNum - 1] =
					!gd->remoteControlBits[lineNum - 1];

			gd->switchServer->send(200, "application/json",
				"Updated to: " + newState + "\r\n");
		}
		else
		{
			gd->switchServer->send(401, "text/html",
				"Wrong line state: " + newState + "\r\n");
		}
	}
	else
	{
		gd->switchServer->send(401, "text/html",
			"Wrong line number: " + line + "\r\n");
	}
}

// HTTP PUT /SetLinkedSwitch
void HandleHTTPSetLinkedSwitch()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	String line = gd->switchServer->arg("line");
	String address = gd->switchServer->arg("address");

	int lineNum = line.toInt();

	if (lineNum >= 1 && lineNum <= 3)
	{
		strncpy(
			config.linkedSwitchAddress[lineNum - 1],
			address.c_str(),
			LINKED_SWITCH_ADDR_LEN);
		saveConfiguration();
		gd->switchServer->send(200, "application/json",
			"Updated to: " + address + "\r\n");
	}
	else
	{
		gd->switchServer->send(401, "text/html",
			"Wrong line number: " + line + "\r\n");
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
	for (int i=0; i<SW_LINES; i++)
		config.linkedSwitchAddress[i][0] = '\0';

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

// Map switch input and remote control bit to power output, fire linked changes
void updateLine(int lineNumber)
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	int lineState = digitalRead(gd->switchPins[lineNumber]);
	if (gd->remoteControlBits[lineNumber])
		lineState = (lineState == HIGH) ? LOW : HIGH;

	if (lineState != digitalRead(gd->powerPins[lineNumber]))
	{
		digitalWrite(gd->powerPins[lineNumber], lineState);
		// TODO: add linked switch update here
	}
}

// Map all line's outputs to input and remote control bits
void updateLines()
{
	for (int i=0; i < SW_LINES; i++)
		updateLine(i);
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

	gd->switchServer = new ESP8266WebServer(WEB_SERVER_PORT);
	gd->timer = new Timer();
	gd->mdns = new MDNSResponder();

	makeSureWiFiConnected();

	gd->switchServer->on("/Status", HTTPMethod::HTTP_GET, HandleHTTPGetStatus);
	gd->switchServer->on("/ChangeLine", HTTPMethod::HTTP_PUT, HandleHTTPChangeLine);
	gd->switchServer->on("/SetLinkedSwitch", HTTPMethod::HTTP_PUT, HandleHTTPSetLinkedSwitch);

	// Switch pins
	gd->switchPins[0] = I1;
	gd->switchPins[1] = I2;
	gd->switchPins[2] = I3;

	// Power pins
	gd->powerPins[0] = O1;
	gd->powerPins[1] = O2;
	gd->powerPins[2] = O3;

	gd->remoteControlBits[0] = gd->remoteControlBits[1] = gd->remoteControlBits[2] = 0;

	gd->switchServer->begin();
	Serial.println("HTTP server started.");

	// Set up regulars
	gd->timer->every(CHECK_FIRMWARE_EVERY, checkFirmwareUpdates);
	gd->timer->every(UPDATE_POWER_EVERRY, updateLines);

	// outputs
	pinMode(O1, OUTPUT);
	pinMode(O2, OUTPUT);
	pinMode(O3, OUTPUT);
}

void loop()
{
	ControllerData *gd = &GD;

	makeSureWiFiConnected();
	gd->timer->update();
}
