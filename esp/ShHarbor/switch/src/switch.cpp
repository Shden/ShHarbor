/*
	Wall switch module based on ESP8266 SoC.

	Features:
	- Up to 3 channels (switch + light).
	- REST API to monitor/contol light.
	- Wall mounted switches support.
	- Multiple linked switches.
	- OTA firmware update.
	- Built in web UI to control switch settings.

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
#include <FS.h>
#include "OTA.h"
#include "config.h"
#include "wifi.h"

#define WEB_SERVER_PORT         80
#define CHECK_SW_UPDATES_EVERY	(60000L*5)	// every 5 min
#define UPDATE_POWER_EVERY	500		// every 500 ms
#define LINKED_SWITCH_ADDR_LEN	80
#define SW_LINES		3
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

const char* FW_URL_BASE = "http://192.168.1.200/firmware/ShHarbor/switch/";

void checkSoftwareUpdates();

struct ControllerData
{
	ESP8266WebServer*       switchServer;
	MDNSResponder*          mdns;
	Timer*                  timer;
	int			remoteControlBits[SW_LINES];	// remote control bits by channels
	int			switchPins[SW_LINES];		// switch pins by channels
	int 			powerPins[SW_LINES];		// power pins by channels
} GD;

// will have ssid, secret, initialised, MDNSHost plus what is defined here
struct ConfigurationData : ConnectedESPConfiguration
{
	char			linkedSwitchAddress[SW_LINES][LINKED_SWITCH_ADDR_LEN + 1];
	int			linkedSwitchLine[SW_LINES];
} config;

// HTTP GET /Status
void HandleHTTPGetStatus()
{
	ControllerData *gd = &GD;

	String json = String("{ \"Lines\" : [\n\r");

	for (int i=0; i<SW_LINES; i++)
	{
		json += String("\t{ \"Status\" : ") +
			String(digitalRead(gd->powerPins[i])) + ", " +
			"\"Link\" : { " +
				"\"Address\" : \"" + String(config.linkedSwitchAddress[i]) + "\" , " +
				"\"Line\" : " + String(config.linkedSwitchLine[i])+ " } }";
		if (i < SW_LINES - 1)
			json += String(",");
		else
			json += String(" ], ");
		json += String("\n\r");
	}

	json += String("\"Build\" : ") + String(FW_VERSION) + " }\n\r";

	gd->switchServer->send(200, "application/json", json);
}

// HTTP GET /ChangeLine
void HandleHTTPChangeLine()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	String line = gd->switchServer->arg("line");
	String newState = gd->switchServer->arg("state");

	int lineNum = line.toInt();
	int newStateVal = newState.toInt();

	if (lineNum >= 0 && lineNum <= 2)
	{
		if (newStateVal >= 0 && newStateVal <= 1)
		{
			int currentLineState =
				(digitalRead(gd->powerPins[lineNum]) == HIGH)
				? 1 : 0;
			if (currentLineState != newStateVal)
				gd->remoteControlBits[lineNum] =
					!gd->remoteControlBits[lineNum];

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

// HTTP GET /SetLinkedSwitch
void HandleHTTPSetLinkedSwitch()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	String line = gd->switchServer->arg("line");
	String linkedAddress = gd->switchServer->arg("linkedaddress");
	String linkedLine = gd->switchServer->arg("linkedline");

	int lineNum = line.toInt();
	int linkedLineNum = linkedLine.toInt();

	if (lineNum >= 0 && lineNum <= 2)
	{
		strncpy(
			config.linkedSwitchAddress[lineNum],
			linkedAddress.c_str(),
			LINKED_SWITCH_ADDR_LEN);

		config.linkedSwitchLine[lineNum] = linkedLineNum;

		saveConfiguration(&config, sizeof(ConfigurationData));
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

// HTTP GET /CheckSoftwareUpdates
void HandleHTTPCheckSoftwareUpdates()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	gd->switchServer->send(200, "text/html",
		"Checking new firmware availability...\r\n");
	checkSoftwareUpdates();

	// if there was new version we wouldn't get here, so
	// TODO: this never works as the connection seems to be closed already.
	gd->switchServer->send(200, "text/html",
		"No update available now.\r\n");
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
			int httpCode = httpClient.sendRequest("GET");
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

// Returns content type based on @filename extension.
String getContentType(String filename)
{
	/*if (server.hasArg("download")) return "application/octet-stream";
	else */if (filename.endsWith(".htm")) return "text/html";
	else if (filename.endsWith(".html")) return "text/html";
	else if (filename.endsWith(".css")) return "text/css";
	else if (filename.endsWith(".js")) return "application/javascript";
	else if (filename.endsWith(".png")) return "image/png";
	else if (filename.endsWith(".gif")) return "image/gif";
	else if (filename.endsWith(".jpg")) return "image/jpeg";
	else if (filename.endsWith(".ico")) return "image/x-icon";
	else if (filename.endsWith(".xml")) return "text/xml";
	else if (filename.endsWith(".pdf")) return "application/x-pdf";
	else if (filename.endsWith(".zip")) return "application/x-zip";
	else if (filename.endsWith(".gz")) return "application/x-gzip";
	return "text/plain";
}

// Checks SPIFFS for the file requested and handles file streaming if exists.
void handleSPIFFSFileRead()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	String path = gd->switchServer->uri();
	Serial.println("handleFileRead: " + path);

	if (path.endsWith("/"))
		path += "index.html";

	String contentType = getContentType(path);
	String pathWithGz = path + ".gz";
	if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
	{
		if (SPIFFS.exists(pathWithGz))
		{
			Serial.println("Using GZIP version: " + pathWithGz);
			path = pathWithGz;
			//gd->switchServer->sendHeader("Content-Encoding", "gzip");
		}
		File spiffsFile = SPIFFS.open(path, "r");
		gd->switchServer->streamFile(spiffsFile, contentType);
		spiffsFile.close();
	}
	gd->switchServer->send(404, "text/plain", "File not found.");
}

void setup()
{
	Serial.begin(115200);
	Serial.println("Initialisation.");
	Serial.printf("Switch controller build %d.\n", FW_VERSION);

	Serial.println("Configuration loading.");
	loadConfiguration(&config, sizeof(ConfigurationData));

	// Warning: uses global data
	ControllerData *gd = &GD;

	gd->switchServer = new ESP8266WebServer(WEB_SERVER_PORT);
	gd->timer = new Timer();
	gd->mdns = new MDNSResponder();

	makeSureWiFiConnected(&config, gd->mdns);

	if (SPIFFS.begin())
		Serial.println("SPIFFS mount succesfull.");
	else
		Serial.println("SPIFFS mount failed.");

	gd->switchServer->on("/Status", HTTPMethod::HTTP_GET, HandleHTTPGetStatus);
	gd->switchServer->on(CHANGE_LINE_METHOD, HTTPMethod::HTTP_GET, HandleHTTPChangeLine);
	gd->switchServer->on("/SetLinkedSwitch", HTTPMethod::HTTP_GET, HandleHTTPSetLinkedSwitch);
	gd->switchServer->on("/CheckSoftwareUpdates", HTTPMethod::HTTP_GET, HandleHTTPCheckSoftwareUpdates);

	//called when the url is not defined here to load content from SPIFFS
	gd->switchServer->onNotFound(handleSPIFFSFileRead);

	// Switch pins          Power pins
	gd->switchPins[0] = I1; gd->powerPins[0] = O1; gd->remoteControlBits[0] = 0;
	gd->switchPins[1] = I2; gd->powerPins[1] = O2; gd->remoteControlBits[1] = 0;
	gd->switchPins[2] = I3; gd->powerPins[2] = O3; gd->remoteControlBits[2] = 0;

	gd->switchServer->begin();
	Serial.println("HTTP server started.");

	// Set up regulars
	gd->timer->every(CHECK_SW_UPDATES_EVERY, checkSoftwareUpdates);
	gd->timer->every(UPDATE_POWER_EVERY, updateLines);

	// outputs
	pinMode(O1, OUTPUT);
	pinMode(O2, OUTPUT);
	pinMode(O3, OUTPUT);
}

void loop()
{
	ControllerData *gd = &GD;

	makeSureWiFiConnected(&config, gd->mdns);
	gd->switchServer->handleClient();
	gd->timer->update();
}
