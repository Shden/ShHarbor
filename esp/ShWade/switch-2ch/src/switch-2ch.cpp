/*
	2 channel 220AC switch module based on ESP8266 SoC for ShWade switch
	module substitute.

	Features:
	- 2 x 220AC up to 300W power switch.
	- REST API to control each of the channels.
	- OTA firmware update.
	- Built in configuration web UI at /config.

	Toolchain: PlatformIO.

	By denis.afanassiev@gmail.com

	API:
	curl 192.168.1.15/status
	curl 192.168.1.15/PIOA
	curl 192.168.1.15/PIOB
	curl -X PUT 192.168.1.15/PIOA?state=0
	curl -X PUT 192.168.1.15/PIOB?state=1
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Timer.h>
#include <OTA.h>
#include <config.h>
#include <wifi.h>
#include <ESPTemplateProcessor.h>
#include <ESP8266HTTPClient.h>

#define WEB_SERVER_PORT         80
#define CHECK_SW_UPDATES_EVERY	(60000L*5)	// every 5 min
#define PIO_A			0
#define PIO_B			1
#define PIO_A_PIN		12
#define PIO_B_PIN		14
#define PIO_ERROR		-1
#define OTA_URL_LEN		80

//const char* FW_URL_BASE = "http://192.168.1.162/firmware/ShWade/switch-2ch/";

void checkSoftwareUpdates();

struct ControllerData
{
	ESP8266WebServer*       switchServer;
	MDNSResponder*          mdns;
	Timer*                  timer;
} GD;

// will have ssid, secret, initialised, MDNSHost. No build specific data thus far.
struct ConfigurationData : ConnectedESPConfiguration
{
	char			OTA_URL[OTA_URL_LEN + 1];
} config;

// Maps config.html parameters to configuration values.
String mapConfigParameters(const String& key)
{
	if (key == "SSID") return String(config.ssid); else
	if (key == "PASS") return String(config.secret); else
	if (key == "MDNS") return String(config.MDNSHost); else
	if (key == "IP") return WiFi.localIP().toString(); else
	if (key == "BUILD") return String(FW_VERSION); else
	if (key == "OTA_URL") return String(config.OTA_URL);
}

// Returns channel state by number
int getPIO(int channelNo)
{
	if (PIO_A == channelNo)
	{
		return digitalRead(PIO_A_PIN);
	}
	else if (PIO_B == channelNo)
	{
		return digitalRead(PIO_B_PIN);
	}
	else return PIO_ERROR;
}

// Updates channel state by number
void setPIO(int channelNo, int newState)
{
	if (PIO_A == channelNo)
	{
		digitalWrite(PIO_A_PIN, newState);
	}
	else if (PIO_B == channelNo)
	{
		digitalWrite(PIO_B_PIN, newState);
	}
}

// HTTP GET /Status
void HandleHTTPGetStatus()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	String json =
		String("{ ") +
			"\"PIO_A\" : " + String(getPIO(PIO_A)) + ", " +
			"\"PIO_B\" : " + String(getPIO(PIO_B)) + ", " +
			"\"Build\" : " + String(FW_VERSION) +
		" }\r\n";

	gd->switchServer->send(200, "application/json", json);
}

// Handles GET & PUT by channelNo requests
void HandlePIO(int channelNo)
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	switch (gd->switchServer->method())
	{
		case HTTPMethod::HTTP_GET:
			{
				String res = String(getPIO(channelNo));
				gd->switchServer->send(
					200, "application/json", res);
			}
			break;

		case HTTPMethod::HTTP_PUT:
		case HTTPMethod::HTTP_POST:
			{
				String param = gd->switchServer->arg("state");
				int channelState = param.toInt();
				if (channelState == 0 || channelState == 1)
				{
					setPIO(channelNo, channelState);
					gd->switchServer->send(
						200, "application/json",
						"Updated to: " +
						String(getPIO(channelNo)) + "\r\n");
				}
				else
				{
					gd->switchServer->send(401, "text/html",
						"Wrong value: " +
						String(channelNo) + "\r\n");
				}
			}
			break;
	}
}

// Handles GET & POST PIOA requests
void HandlePIOA()
{
	return HandlePIO(PIO_A);
}

// Handles GET & POST PIOB requests
void HandlePIOB()
{
	return HandlePIO(PIO_B);
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

		WiFi.disconnect();
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
	gd->mdns = new MDNSResponder();

	makeSureWiFiConnected(&config, gd->mdns);

	if (SPIFFS.begin())
		Serial.println("SPIFFS mount succesfull.");
	else
		Serial.println("SPIFFS mount failed.");

	gd->switchServer->on("/status", HTTPMethod::HTTP_GET, HandleHTTPGetStatus);
	gd->switchServer->on("/config", HandleConfig);
	gd->switchServer->on("/PIOA", HandlePIOA);
	gd->switchServer->on("/PIOB", HandlePIOB);

	gd->switchServer->begin();
	Serial.println("HTTP server started.");

	// Set up regulars
	gd->timer->every(CHECK_SW_UPDATES_EVERY, checkSoftwareUpdates);

	pinMode(PIO_A_PIN, OUTPUT);
	pinMode(PIO_B_PIN, OUTPUT);

	// Use blue led to indicate wifi connection. Thus far off (high level).
	pinMode(BLUE_LED_PIN, OUTPUT);
	digitalWrite(BLUE_LED_PIN, HIGH);
}

void loop()
{
	ControllerData *gd = &GD;

	makeSureWiFiConnected(&config, gd->mdns);
	gd->switchServer->handleClient();
	gd->timer->update();
}
