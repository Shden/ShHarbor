/*
	2 channel 220AC switch module based on ESP8266 SoC for ShWade switch
	module substitute.

	Features:
	- 2 x 220AC up to 300W power switch.
	- REST API to control each of the channels.
	- OTA firmware update.
	- Built in configuration web UI at /config.
	- WiFi access point to configure and troubleshoot.

	Toolchain: PlatformIO.

	By denis.afanassiev@gmail.com

	API:
	curl 192.168.1.15/status
	curl 192.168.1.15/LineA
	curl 192.168.1.15/LineB
	curl -X PUT 192.168.1.15/LineA?state=0
	curl -X PUT 192.168.1.15/LineB?state=1
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <Timer.h>
#include <OTA.h>
#include <ConnectedESPConfiguration.h>
#include <WiFiManager.h>
#include <ESPTemplateProcessor.h>

#define WEB_SERVER_PORT         80
#define CHECK_SW_UPDATES_EVERY	(60000L*5)	// every 5 min
#define LINE_A			0
#define LINE_B			1
#define LINE_A_PIN		12
#define LINE_B_PIN		14
#define WRONG_LINE_NUMBER	-1
#define OTA_URL_LEN		80

//const char* FW_URL_BASE = "http://192.168.1.162/firmware/ShWade/switch-2ch/";

void checkSoftwareUpdates();

struct ControllerData
{
	ESP8266WebServer*       switchServer;
	Timer*                  timer;
} GD;

// will have ssid, secret, initialised, MDNS host name.
struct ConfigurationData : ConnectedESPConfiguration
{
	char			OTA_URL[OTA_URL_LEN + 1];
} config;

// Returns line state by number
int getLine(int lineNo)
{
	if (LINE_A == lineNo)
	{
		return digitalRead(LINE_A_PIN);
	}
	else if (LINE_B == lineNo)
	{
		return digitalRead(LINE_B_PIN);
	}
	else return WRONG_LINE_NUMBER;
}

// Updates line state by number
void setLine(int lineNo, int newState)
{
	if (LINE_A == lineNo)
	{
		digitalWrite(LINE_A_PIN, newState);
	}
	else if (LINE_B == lineNo)
	{
		digitalWrite(LINE_B_PIN, newState);
	}
}

// HTTP GET /Status
void HandleHTTPGetStatus()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	String json =
		String("{ ") +
			"\"LineA\" : " + String(getLine(LINE_A)) + ", " +
			"\"LineB\" : " + String(getLine(LINE_B)) + ", " +
			"\"Build\" : " + String(FW_VERSION) +
		" }\r\n";

	gd->switchServer->send(200, "application/json", json);
}

// Handles GET & PUT by lineNo requests
void HandleLine(int lineNo)
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	switch (gd->switchServer->method())
	{
		case HTTPMethod::HTTP_GET:
			{
				String res = String(getLine(lineNo));
				gd->switchServer->send(
					200, "application/json", res);
			}
			break;

		case HTTPMethod::HTTP_PUT:
		case HTTPMethod::HTTP_POST:
			{
				String param = gd->switchServer->arg("state");
				int lineState = param.toInt();
				if (lineState == 0 || lineState == 1)
				{
					setLine(lineNo, lineState);
					gd->switchServer->send(
						200, "application/json",
						"Updated to: " +
						String(getLine(lineNo)) + "\r\n");
				}
				else
				{
					gd->switchServer->send(401, "text/html",
						"Wrong parameter: " + param + "\r\n");
				}
			}
			break;

		default:
			break;
	}
}

// Handles GET & POST Line A requests
void HandleLineA()
{
	return HandleLine(LINE_A);
}

// Handles GET & POST Line B requests
void HandleLineB()
{
	return HandleLine(LINE_B);
}

// Maps config.html parameters to configuration values.
String mapConfigParameters(const String& key)
{
	if (key == "SSID") return String(config.ssid); else
	if (key == "PASS") return String(config.secret); else
	if (key == "MDNS") return String(config.MDNSHost); else
	if (key == "IP") return WiFi.localIP().toString(); else
	if (key == "BUILD") return String(FW_VERSION); else
	if (key == "OTA_URL") return String(config.OTA_URL); else
	return "Mapping value undefined.";
}

// Handles HTTP GET & POST /config.html requests
void HandleConfig()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	// NETWORK_UPDATE
	if (gd->switchServer->hasArg("NETWORK_UPDATE"))
	{
		gd->switchServer->arg("SSID").toCharArray(config.ssid, SSID_LEN);
		gd->switchServer->arg("PASS").toCharArray(config.secret, SECRET_LEN);
		gd->switchServer->arg("MDNS").toCharArray(config.MDNSHost, MDNS_HOST_LEN);

		saveConfiguration(&config, sizeof(ConfigurationData));

		// redirect to the same page without arguments
		gd->switchServer->sendHeader("Location", String("/config"), true);
		gd->switchServer->send(302, "text/plain", "");

		// Try connecting with new credentials
		WiFi.disconnect();
		WiFiManager::handleWiFiConnectivity();
	}

	// GENERAL_UPDATE
	if (gd->switchServer->hasArg("GENERAL_UPDATE"))
	{
		gd->switchServer->arg("OTA_URL").toCharArray(config.OTA_URL, OTA_URL_LEN);
		saveConfiguration(&config, sizeof(ConfigurationData));
	}

	// CHECK_UPDATE_NOW
	if (gd->switchServer->hasArg("CHECK_UPDATE_NOW"))
	{
		Serial.println("Checking software updates available.");
		checkSoftwareUpdates();
	}

	ESPTemplateProcessor(*gd->switchServer).send(
		String("/config.html"),
		mapConfigParameters);
}

// Maps control.html parameters to lines status.
String mapControlParameters(const String& key)
{
	if (key == "LINE_A_CHECKED") return (getLine(LINE_A) ? "checked" : "?"); else
	if (key == "LINE_B_CHECKED") return (getLine(LINE_B) ? "checked" : "?"); else
	return "Mapping value undefined.";
}

// Handles HTTP GET & POST /control.html requests
void HandleControl()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	// STATUS_UPDATE
	if (gd->switchServer->hasArg("STATUS_UPDATE"))
	{
		// String sA = gd->switchServer->arg("LINE_A");
		// Serial.print("LINE_A value: "); Serial.println(sA);
		//
		// String sB = gd->switchServer->arg("LINE_B");
		// Serial.print("LINE_B value: "); Serial.println(sB);

		int lineAstatus = gd->switchServer->hasArg("LINE_A") ? 1 : 0;
		int lineBstatus = gd->switchServer->hasArg("LINE_B") ? 1 : 0;

		setLine(LINE_A, lineAstatus);
		setLine(LINE_B, lineBstatus);

		Serial.printf("LINE_A status: %d\n", lineAstatus);
		Serial.printf("LINE_B status: %d\n", lineBstatus);
	}

	ESPTemplateProcessor(*gd->switchServer).send(
		String("/control.html"),
		mapControlParameters);
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

	updateAll(FW_VERSION, spiffsVersion, config.OTA_URL);
}

void setup()
{
	Serial.begin(115200);
	Serial.println("Initialisation.");
	Serial.printf("2-channel switch build %d.\n", FW_VERSION);

	Serial.println("Configuration loading.");
	loadConfiguration(&config, sizeof(ConfigurationData));

	// Warning: uses global data
	ControllerData *gd = &GD;

	gd->switchServer = new ESP8266WebServer(WEB_SERVER_PORT);
	gd->timer = new Timer();

	// Initialise WiFi entity that will handle connectivity. We don't
	// care of WiFi anymore, all handled inside it
	WiFiManager::init(&config);

	if (SPIFFS.begin())
		Serial.println("SPIFFS mount succesfull.");
	else
		Serial.println("SPIFFS mount failed.");

	gd->switchServer->on("/status", HTTPMethod::HTTP_GET, HandleHTTPGetStatus);
	gd->switchServer->on("/config", HandleConfig);
	gd->switchServer->on("/control", HandleControl);
	gd->switchServer->on("/LineA", HandleLineA);
	gd->switchServer->on("/LineB", HandleLineB);

	// captive pages
	gd->switchServer->on("", HandleConfig);
	gd->switchServer->on("/", HandleConfig);

	// css served from SPIFFS
	gd->switchServer->serveStatic(
		"/bootstrap/4.0.0/css/bootstrap.min.css", SPIFFS,
		"/bootstrap.min.css");

	gd->switchServer->begin();
	Serial.println("HTTP server started.");

	// Set up regulars
	gd->timer->every(CHECK_SW_UPDATES_EVERY, checkSoftwareUpdates);

	pinMode(LINE_A_PIN, OUTPUT);
	pinMode(LINE_B_PIN, OUTPUT);
}

void loop()
{
	ControllerData *gd = &GD;

	gd->switchServer->handleClient();
	gd->timer->update();
	WiFiManager::update();
}
