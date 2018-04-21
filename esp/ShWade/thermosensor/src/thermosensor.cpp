/*
	Thermosensor module based on ESP8266 SoC for ShWade temperature
	sensor socket substitute.

	Features:
	- DS1820 temperature control sensor.
	- REST API to get sensor data points.
	- OTA firmware update.
	- Post temperature update to configured upstream API endpoint.
	- Built in configuration web UI at /config.

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
#include <ESP8266HTTPClient.h>

#define ONE_WIRE_PIN            5
#define WEB_SERVER_PORT         80
#define POST_TEMP_EVERY       	(10000L)        // every 10 sec
#define CHECK_SW_UPDATES_EVERY	(60000L*5)	// every 5 min
#define ENDPOINT_URL_LENGTH	80

const char* FW_URL_BASE = "http://192.168.1.162/firmware/ShWade/thermosensor/";

void checkSoftwareUpdates();

struct ControllerData
{
	TemperatureSensor*      temperatureSensor;
	char			sensorAddress[20];
	ESP8266WebServer*       thermosensorServer;
	MDNSResponder*          mdns;
	Timer*                  timer;
} GD;

// will have ssid, secret, initialised, MDNSHost + API endpoit to post data.
struct ConfigurationData : ConnectedESPConfiguration
{
	char			postDataAPIEndpoint[ENDPOINT_URL_LENGTH];
} config;

// Go to sensor and get current temperature.
float getTemperature()
{
	// Warning: uses global data.
	ControllerData *gd = &GD;

	return gd->temperatureSensor->getTemperature(0);
}

// Post temperature update to the linked upstream server
void postTemperatureUpdate()
{
	// Warning: uses global data.
	ControllerData *gd = &GD;

	float temp = getTemperature();

	// Here goes workaround for %f which wasnt working right.
	Serial.printf("Temperature: %d.%02d\n", (int)temp, (int)(temp*100)%100);

	if (strlen(config.postDataAPIEndpoint))
	{
		// linked swich update HTTP reqiest
		String postDataURL =
			String("http://") +
			String(config.postDataAPIEndpoint) +
			String("?temperature=") + String(temp, 2) +
			String("&sensor=") + String(gd->sensorAddress);

		Serial.print("Invocation URL: ");
		Serial.println(postDataURL);

		HTTPClient httpClient;
		httpClient.begin(postDataURL);
		int httpCode = httpClient.GET();
		Serial.printf("Responce code: %d\n", httpCode);
		httpClient.end();
	}
}

// HTTP GET /Status
void HandleHTTPGetStatus()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	String json =
	String("{ ") +
		"\"CurrentTemperature\" : " + String(getTemperature(), 2) + ", " +
		"\"Build\" : " + String(FW_VERSION) +
	" }\r\n";

	gd->thermosensorServer->send(200, "application/json", json);
}

// Maps config.html parameters to configuration values.
String mapConfigParameters(const String& key)
{
	if (key == "SSID") return String(config.ssid); else
	if (key == "PASS") return String(config.secret); else
	if (key == "MDNS") return String(config.MDNSHost); else
	if (key == "IP") return WiFi.localIP().toString(); else
	if (key == "BUILD") return String(FW_VERSION); else
	if (key == "API") return String(config.postDataAPIEndpoint);
}

// // Debug request arguments printout.
// void dbgPostPrintout()
// {
// 	// Warning: uses global data
// 	ControllerData *gd = &GD;
//
// 	for (int i=0; i < gd->thermosensorServer->args(); i++)
// 	{
// 		Serial.print(gd->thermosensorServer->argName(i));
// 		Serial.print(": ");
// 		Serial.print(gd->thermosensorServer->arg(i));
// 		Serial.println();
// 	}
// }

// Handles HTTP GET & POST /config.html requests
void HandleConfig()
{
	// dbgPostPrintout();

	// Warning: uses global data
	ControllerData *gd = &GD;

	// NETWORK_UPDATE
	if (gd->thermosensorServer->hasArg("NETWORK_UPDATE"))
	{
		gd->thermosensorServer->arg("SSID").toCharArray(config.ssid, SSID_LEN);
		gd->thermosensorServer->arg("PASS").toCharArray(config.secret, SECRET_LEN);
		gd->thermosensorServer->arg("MDNS").toCharArray(config.MDNSHost, MDNS_HOST_LEN);

		saveConfiguration(&config, sizeof(ConfigurationData));

		// redirect to the same page without arguments
		gd->thermosensorServer->sendHeader("Location", String("/config"), true);
		gd->thermosensorServer->send(302, "text/plain", "");

		WiFi.disconnect();
	}

	// GENERAL_UPDATE
	if (gd->thermosensorServer->hasArg("GENERAL_UPDATE"))
	{
		gd->thermosensorServer->arg("API").toCharArray(
			config.postDataAPIEndpoint, ENDPOINT_URL_LENGTH
		);
		saveConfiguration(&config, sizeof(ConfigurationData));
	}

	ESPTemplateProcessor(*gd->thermosensorServer).send(
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
	Serial.printf("Themperature sensor build %d.\n", FW_VERSION);

	Serial.println("Configuration loading.");
	loadConfiguration(&config, sizeof(ConfigurationData));

	// Warning: uses global data
	ControllerData *gd = &GD;

	gd->temperatureSensor = new TemperatureSensor(ONE_WIRE_PIN);
	pinMode(ONE_WIRE_PIN, INPUT_PULLUP);
	delay(500);
	gd->temperatureSensor->getAddress(0, gd->sensorAddress);
	Serial.printf("Sensor address: %s\n", gd->sensorAddress);

	gd->thermosensorServer = new ESP8266WebServer(WEB_SERVER_PORT);
	gd->timer = new Timer();
	gd->mdns = new MDNSResponder();

	makeSureWiFiConnected(&config, gd->mdns);

	if (SPIFFS.begin())
		Serial.println("SPIFFS mount succesfull.");
	else
		Serial.println("SPIFFS mount failed.");

	gd->thermosensorServer->on("/status", HTTPMethod::HTTP_GET, HandleHTTPGetStatus);
	gd->thermosensorServer->on("/config", HandleConfig);

	gd->thermosensorServer->begin();
	Serial.println("HTTP server started.");

	// Set up regulars
	gd->timer->every(POST_TEMP_EVERY, postTemperatureUpdate);
	gd->timer->every(CHECK_SW_UPDATES_EVERY, checkSoftwareUpdates);
}

void loop()
{
	ControllerData *gd = &GD;

	makeSureWiFiConnected(&config, gd->mdns);
	gd->thermosensorServer->handleClient();
	gd->timer->update();
}
