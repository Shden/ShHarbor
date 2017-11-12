/*
	Wall switch module based on ESP8266 SoC.

	Features:
	- Up to 3 channels (switch + light).
	- REST API to monitor/contol light.
	- Wall mounted switches support.
	- Multiple linked switches.
	- OTA firmware update.

	Toolchain: PlatformIO.

	By denis.afanassiev@gmail.com
*/

#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
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
#define SW_LINES		3
#define MDNS_HOST               "HB-SWITCH"
#define CHANGE_LINE_METHOD	"/ChangeLine"

// ESP-12 pins:
// inputs: U6, U7, U8
#define U6			5		// these two line numbers are
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
const int FW_VERSION = 603;

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
	int			linkedSwitchLine[SW_LINES];
} config;

// HTTP GET /Status
void HandleHTTPGetStatus()
{
	ControllerData *gd = &GD;

	String json =
		String("{ ") +
		"\"L1\" : { " +
			"\"Status\" : " + String(digitalRead(gd->powerPins[0])) + ", " +
			"\"Link\" : { " +
				"\"Address\" : " + String(config.linkedSwitchAddress[0]) + ", " +
				"\"Line\" : " + String(config.linkedSwitchLine[0])+ ", } } " +
		"\"L2\" : { " +
			"\"Status\" : " + String(digitalRead(gd->powerPins[1])) + ", " +
			"\"Link\" : { " +
				"\"Address\" : " + String(config.linkedSwitchAddress[1]) + ", " +
				"\"Line\" : " + String(config.linkedSwitchLine[1])+ ", } } " +
		"\"L3\" : { " +
			"\"Status\" : " + String(digitalRead(gd->powerPins[2])) + ", " +
			"\"Link\" : { " +
				"\"Address\" : " + String(config.linkedSwitchAddress[2]) + ", " +
				"\"Line\" : " + String(config.linkedSwitchLine[2])+ ", } } " +
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
	String linkedAddress = gd->switchServer->arg("linkedaddress");
	String linkedLine = gd->switchServer->arg("linkedline");

	int lineNum = line.toInt();
	int linkedLineNum = linkedLine.toInt();

	if (lineNum >= 1 && lineNum <= 3)
	{
		strncpy(
			config.linkedSwitchAddress[lineNum - 1],
			linkedAddress.c_str(),
			LINKED_SWITCH_ADDR_LEN);

		config.linkedSwitchLine[lineNum - 1] = linkedLineNum;

		saveConfiguration();
		gd->switchServer->send(200, "application/json",
			"Updated to: " + linkedAddress +
			" and line: " + linkedLine + "\r\n");
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
		Serial.printf("Updating line %d to new state %d\n", lineNumber + 1, lineState);

		digitalWrite(gd->powerPins[lineNumber], lineState);

		if (config.linkedSwitchAddress[lineNumber][0] != '\0')
		{

			// linked swich update HTTP reqiest
			String changeLinkedLineURL =
				String("http://") +

				String(config.linkedSwitchAddress[lineNumber]) +
				String(CHANGE_LINE_METHOD) +
				String("?line=") + String(config.linkedSwitchLine[lineNumber]) +
				String("&state=") + String(lineState);

			Serial.print("Linked update reqiest url: ");
			Serial.println(changeLinkedLineURL);

			HTTPClient httpClient;
			httpClient.begin(changeLinkedLineURL);
			int httpCode = httpClient.sendRequest("PUT");
			Serial.printf("Responce code: %d\n", httpCode);
			httpClient.end();
		}
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
	Serial.printf("Switch controller build %d.\n", FW_VERSION);

	Serial.println("Configuration loading.");
	loadConfiguration();

	// Warning: uses global data
	ControllerData *gd = &GD;

	gd->switchServer = new ESP8266WebServer(WEB_SERVER_PORT);
	gd->timer = new Timer();
	gd->mdns = new MDNSResponder();

	makeSureWiFiConnected();

	gd->switchServer->on("/Status", HTTPMethod::HTTP_GET, HandleHTTPGetStatus);
	gd->switchServer->on(CHANGE_LINE_METHOD, HTTPMethod::HTTP_PUT, HandleHTTPChangeLine);
	gd->switchServer->on("/SetLinkedSwitch", HTTPMethod::HTTP_PUT, HandleHTTPSetLinkedSwitch);

	// Switch pins          Power pins
	gd->switchPins[0] = I1; gd->powerPins[0] = O1; gd->remoteControlBits[0] = 0;
	gd->switchPins[1] = I2; gd->powerPins[1] = O2; gd->remoteControlBits[1] = 0;
	gd->switchPins[2] = I3; gd->powerPins[2] = O3; gd->remoteControlBits[2] = 0;

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
	gd->switchServer->handleClient();
	gd->timer->update();
}
