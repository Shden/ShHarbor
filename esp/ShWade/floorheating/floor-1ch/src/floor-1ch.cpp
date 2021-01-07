/*
 *	Home: ShWade.
 *
 *	Floor heating module based on ESP8266 SoC.
 *
 *	Features:
 *	- DS1820 temperature control sensor.
 *	- AC 220V power control.
 *	- REST API to monitor/contol heating parameters.
 *	- OTA firmware update.
 *	- Built in configuration web UI at /config.
 *	- WiFi access point to configure and troubleshoot.
 *	- ShWade specific: check total power consumption, power balancing.
 *	- AWS IoT integration: temperature reporting and configuration control.
 *
 *	Toolchain: PlatformIO.
 *	Build commands:
 *	  pio run 		# for firmware
 *	  pio run -t buildfs	# for spiffs
 *
 *	By denis.afanassiev@gmail.com
 *
 *	REST API:
 *	curl <IP address>/status
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Timer.h>
#include <DS1820.h>
#include <OTA.h>
#include <ConnectedESPConfiguration.h>
#include <WiFiManager.h>
#include <ESPTemplateProcessor.h>
#include <ArduinoJson.h>
#include <ESP8266HttpClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <PubSubClient.h>
#include <MQTT.h>
#include "secrets.h"


#define ONE_WIRE_PIN            5
#define AC_CONTROL_PIN          D7
#define WEB_SERVER_PORT         80
#define UPDATE_TEMP_EVERY       (5000L)         // every 5 sec
#define CHECK_HEATING_EVERY     (15000L)        // every 15 sec
#define CHECK_SW_UPDATES_EVERY	(60000L*5)	// every 5 min
#define POST_TEMPERATURE_EVERY	(60000L*1)	// every minute
#define DEFAULT_TARGET_TEMP	28.0
#define DEFAULT_ACTIVE		0
#define OTA_URL_LEN		80
#define ONE_WIRE_ADDR_LEN	16
#define MAX_ALLOWED_POWER	16500		// 17 kW total

#define TEXT_HTML		"text/html"
#define TEXT_PLAIN		"text/plain"
#define APPLICATION_JSON	"application/json"
#define MQTT_BUFFER_SIZE	2048

const int MQTT_PORT = 8883;
const char MQTT_SUB_TOPIC[] = "$aws/things/" THINGNAME "/shadow/update";
const char MQTT_PUB_TOPIC[] = "$aws/things/" THINGNAME "/shadow/update";

uint8_t DST = 0;
WiFiClientSecure net;
PubSubClient client(net);

BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);

void checkSoftwareUpdates();
float getTemperature();

struct ControllerData
{
	TemperatureSensor*      temperatureSensor;
	char			sensorAddress[ONE_WIRE_ADDR_LEN + 1];
	ESP8266WebServer*       thermostatServer;
	Timer*                  timer;
	uint8_t                 heatingOn;
} GD;

/* will have ssid, secret, initialised, MDNSHost plus:
 *	- target temperature,
 *	- active flag,
 *	- OTA URL.
 */
struct ConfigurationData : ConnectedESPConfiguration
{
	float                   targetTemp;
	int8_t			active;
	char			OTA_URL[OTA_URL_LEN + 1];
	int			heaterPower;
} config;

time_t now;
time_t nowish = 1510592825;

void NTPConnect(void)
{
	Serial.print("Setting time using SNTP");
	configTime(TIME_ZONE * 3600, DST * 3600, "pool.ntp.org", "time.nist.gov");
	now = time(nullptr);
	while (now < nowish)
	{
		delay(500);
		Serial.print(".");
		now = time(nullptr);
	}
	Serial.println("done!");
	struct tm timeinfo;
	gmtime_r(&now, &timeinfo);
	Serial.print("Current time: ");
	Serial.print(asctime(&timeinfo));
}

// IoT thing shadow messages recever
void messageReceived(char *topic, byte *payload, unsigned int length)
{
	Serial.printf("Received [%s]\n", topic);
	for (uint i = 0; i < length; i++)
		Serial.print((char)payload[i]);
	Serial.println();

	DynamicJsonDocument jsonMessage(4096);
	DeserializationError error = deserializeJson(jsonMessage, (char*)payload);
	
	// Test if parsing succeeded
	if (error)
	{
		Serial.print("deserializeJson() failed: ");
		Serial.println(error.c_str());
		return;
	}

	JsonVariant houseMode = 
		jsonMessage["state"]["reported"]["config"]["mode"];
	JsonVariant house1FloorTemp = 
		jsonMessage["state"]["reported"]["config"]["heating"]["house1FloorTemp"];

	if (houseMode && house1FloorTemp)
	{
		float temp = house1FloorTemp.as<float>();
		int active = (houseMode.as<String>() == "presence") ? 1 : 0;

		Serial.println("Configuration info received:");
		Serial.printf("Thermostat is active: %d\n", active);
		Serial.printf("Thermostat setpoint: %f\n", temp);

		if (config.targetTemp != temp || config.active != active)
		{
			Serial.println("Configuration will be updated...");
			config.targetTemp = temp;
			config.active = active;
			saveConfiguration(&config, sizeof(ConfigurationData));
			Serial.println("Configuration updated.");
		}
	}
}

// MQTT errors printing
void pubSubErr(int8_t MQTTErr)
{
	if (MQTTErr == MQTT_CONNECTION_TIMEOUT)
		Serial.print("Connection timeout");
	else if (MQTTErr == MQTT_CONNECTION_LOST)
		Serial.print("Connection lost");
	else if (MQTTErr == MQTT_CONNECT_FAILED)
		Serial.print("Connect failed");
	else if (MQTTErr == MQTT_DISCONNECTED)
		Serial.print("Disconnected");
	else if (MQTTErr == MQTT_CONNECTED)
		Serial.print("Connected");
	else if (MQTTErr == MQTT_CONNECT_BAD_PROTOCOL)
		Serial.print("Connect bad protocol");
	else if (MQTTErr == MQTT_CONNECT_BAD_CLIENT_ID)
		Serial.print("Connect bad Client-ID");
	else if (MQTTErr == MQTT_CONNECT_UNAVAILABLE)
		Serial.print("Connect unavailable");
	else if (MQTTErr == MQTT_CONNECT_BAD_CREDENTIALS)
		Serial.print("Connect bad credentials");
	else if (MQTTErr == MQTT_CONNECT_UNAUTHORIZED)
		Serial.print("Connect unauthorized");
}

// Connect to AWS MQTT
void connectToMqtt(bool nonBlocking = false)
{
	Serial.print("MQTT connecting... ");
	while (!client.connected())
	{
		// MDNS host name is used as MQTT client name(?)
		if (client.connect(config.MDNSHost))
		{
			Serial.println("done!");
			if (!client.subscribe(MQTT_SUB_TOPIC))
				// lwMQTTErr(client.lastError());
				pubSubErr(client.state());
		}
		else
		{
			Serial.print("failed, reason -> ");
			pubSubErr(client.state());
			// lwMQTTErrConnection(client.returnCode());
			if (!nonBlocking)
			{
				Serial.println(" < try again in 5 seconds");
				delay(5000);
			}
			else
			{
				Serial.println(" <");
			}
		}
		if (nonBlocking) break;
	}
}

HTTPClient httpRequest;
WiFiClient wifiClient;

// Check current power consumption via API
float getPowerConsumption()
{
	if (WiFi.status() == WL_CONNECTED)
	{
		httpRequest.begin(wifiClient, "http://192.168.1.162:3001/API/1.2/consumption/electricity/GetPowerMeterData");
		int httpCode = httpRequest.GET();

		if (httpCode > 0)
		{
			DynamicJsonDocument jsonResponce(2048);
			deserializeJson(jsonResponce, httpRequest.getString());
			float power = jsonResponce["P"]["sum"];
			return power;
		}
		else
		{
			Serial.printf("[GET failed, error: %s\n", httpRequest.errorToString(httpCode).c_str());
			return -1;
		}
		
		httpRequest.end();
	}
	return -1;
}

// Post temperature update to MQTT topic
void postTemperature()
{
	if (WL_CONNECTED == WiFi.status())
	{
		// Prepare payload
		String temperaturePayload =
			String("{\"state\": { \"reported\": { \"ESP\": {") +
			"\"hall_floor_3\": " + String(getTemperature(), 2) +
			"}}}}";
			
		if (!client.publish(MQTT_PUB_TOPIC, temperaturePayload.c_str(), false))
			pubSubErr(client.state());		
	}
}

// Go to sensor and get current temperature.
float getTemperature()
{
	// Warning: uses global data.
	ControllerData *gd = &GD;

	return gd->temperatureSensor->getTemperature(0);
}

// Update temperatureSensor internal data
void temperatureUpdate()
{
	float temp = getTemperature();

	// Here goes workaround for %f which wasnt working right.
	Serial.printf("Temperature: %d.%02d\n", (int)temp, (int)(temp*100)%100);
}

// Heating control.
void controlHeating()
{
	// Warning: uses global data.
	ControllerData *gd = &GD;

	float currentPower = getPowerConsumption();

	uint8_t neededState = config.active && getTemperature() < config.targetTemp;
	uint8_t currentState = digitalRead(AC_CONTROL_PIN);

	float powerDelta = 0;
	if (neededState > currentState) powerDelta = config.heaterPower;
	if (neededState < currentState) powerDelta = -config.heaterPower;

	uint8_t canChangePower = !neededState || (neededState && currentPower + powerDelta < MAX_ALLOWED_POWER);
	
	if (canChangePower)
	{
		gd->heatingOn = neededState;
		digitalWrite(AC_CONTROL_PIN, gd->heatingOn);
		Serial.printf("Heating state: %d\n", gd->heatingOn);
	}
	else
	{
		Serial.printf("Requred power (%f) is over limit, can't on.\n", currentPower + powerDelta);
	}
	
	
}

// HTTP GET /status
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

	gd->thermostatServer->sendHeader("Access-Control-Allow-Origin", "*");
	gd->thermostatServer->send(200, APPLICATION_JSON, json);
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
		saveConfiguration(&config, sizeof(ConfigurationData));
		gd->thermostatServer->send(200, APPLICATION_JSON,
			"Updated to: " + param + "\r\n");
	}
	else
	{
		gd->thermostatServer->send(401, TEXT_HTML,
			"Wrong value: " + param + "\r\n");
	}
}

// HTTP PUT /Active
// to test:
//	$ curl -X PUT -d 'active=0' 192.168.1.120/Active
//	$ curl -X PUT -d 'active=1' 192.168.1.120/Active
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
		gd->thermostatServer->send(200, APPLICATION_JSON,
			"Updated to: " + activeParam + "\r\n");
	}
	else
	{
		gd->thermostatServer->send(401, TEXT_HTML,
			"Wrong value: " + activeParam + "\r\n");
	}
}

// Get the current version of the firmware (from SPIFFS file)
int getFWCurrentVersion()
{
	int spiffsVersion = -1;
	File versionInfo = SPIFFS.open("/version.info", "r");
	if (versionInfo)
	{
		spiffsVersion = versionInfo.parseInt();
		versionInfo.close();
	}

	return spiffsVersion;
 }

// Maps config.html parameters to configuration values.
String mapConfigParameters(const String& key)
{
	// Warning: uses global data.
	ControllerData *gd = &GD;

	if (key == "SSID") return String(config.ssid); else
	if (key == "PASS") return String(config.secret); else
	if (key == "MDNS") return String(config.MDNSHost); else
	if (key == "IP") return WiFi.localIP().toString(); else
	if (key == "BUILD") return String(FW_VERSION); else
	if (key == "DS1820ID") return String(gd->sensorAddress); else
	if (key == "HEATING_STATUS") return (gd->heatingOn) ? "On" :  "Off"; else
	if (key == "VERSION") return (String(getFWCurrentVersion())); else
	if (key == "T_TEMP") return String(config.targetTemp); else
	if (key == "T_POWER") return String(config.heaterPower); else
	if (key == "CHECKED") return config.active ? "checked" : ""; else
	if (key == "OTA_URL") return String(config.OTA_URL); else
	return "Mapping value undefined.";
}

// Handles HTTP GET & POST /config.html requests
void HandleConfig()
{
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
		gd->thermostatServer->send(302, TEXT_PLAIN, "");

		// Try connecting with new credentials
		WiFi.disconnect();
		WiFiManager::handleWiFiConnectivity();
	}

	// Reboot
	if (gd->thermostatServer->hasArg("REBOOT"))
	{
		gd->thermostatServer->send(200, TEXT_PLAIN, "Restarting...");
		ESP.restart();
	}

	// GENERAL_UPDATE
	if (gd->thermostatServer->hasArg("GENERAL_UPDATE"))
	{
		String param = gd->thermostatServer->arg("T_TEMP");
		float temperature = param.toFloat();
		if (temperature > 0.0 && temperature < 100.0)
			config.targetTemp = temperature;

		param = gd->thermostatServer->arg("T_POWER");
		int power = param.toInt();
		if (power > 0 && power < 3000)
			config.heaterPower = power;

		config.active = gd->thermostatServer->hasArg("ACTIVE");
		gd->thermostatServer->arg("OTA_URL").toCharArray(
			config.OTA_URL, OTA_URL_LEN);

		saveConfiguration(&config, sizeof(ConfigurationData));
	}

	// CHECK_UPDATE_NOW
	if (gd->thermostatServer->hasArg("CHECK_UPDATE_NOW"))
	{
		Serial.println("Checking software updates available.");
		checkSoftwareUpdates();
	}

	ESPTemplateProcessor(*gd->thermostatServer).send(
		String("/config.html"),
		mapConfigParameters);
}

// Go check if there is a new firmware or SPIFFS got available.
void checkSoftwareUpdates()
{
	updateAll(FW_VERSION, getFWCurrentVersion(), config.OTA_URL);
}

void setup()
{
	Serial.begin(115200);
	Serial.println("Initialisation.");
	Serial.printf("ShHarbor thermostat build %d.\n", FW_VERSION);

	Serial.println("Configuration loading.");
	loadConfiguration(&config, sizeof(ConfigurationData));

	// Warning: uses global data
	ControllerData *gd = &GD;

	// Initialise DS1820 temperature sensor
	gd->temperatureSensor = new TemperatureSensor(ONE_WIRE_PIN);
	pinMode(ONE_WIRE_PIN, INPUT_PULLUP);
	delay(500);
	gd->temperatureSensor->getAddress(0, gd->sensorAddress);

	// Initialise WiFi entity that will handle connectivity. We don't
	// care of WiFi anymore, all handled inside it
	WiFiManager::init(&config);

	gd->thermostatServer = new ESP8266WebServer(WEB_SERVER_PORT);
	gd->timer = new Timer();

	if (SPIFFS.begin())
		Serial.println("SPIFFS mount succesfull.");
	else
		Serial.println("SPIFFS mount failed.");

	gd->thermostatServer->on("/status", HTTPMethod::HTTP_GET, HandleHTTPGetStatus);
	gd->thermostatServer->on("/TargetTemperature", HTTPMethod::HTTP_PUT, HandleHTTPTargetTemperature);
	gd->thermostatServer->on("/Active", HTTPMethod::HTTP_PUT, HandleHTTPActive);
	gd->thermostatServer->on("/config", HandleConfig);

	// captive pages
	gd->thermostatServer->on("", HandleConfig);
	gd->thermostatServer->on("/", HandleConfig);

	// css served from SPIFFS
	gd->thermostatServer->serveStatic(
		"/bootstrap.min.css", SPIFFS, "/bootstrap.min.css");

	gd->thermostatServer->begin();
	Serial.println("HTTP server started.");

	NTPConnect();

	net.setTrustAnchors(&cert);
	net.setClientRSACert(&client_crt, &key);

	client.setServer(MQTT_HOST, MQTT_PORT);
	client.setBufferSize(MQTT_BUFFER_SIZE);
	client.setCallback(messageReceived);
	connectToMqtt();

	// Set up regulars
	gd->timer->every(UPDATE_TEMP_EVERY, temperatureUpdate);
	gd->timer->every(CHECK_HEATING_EVERY, controlHeating);
	gd->timer->every(CHECK_SW_UPDATES_EVERY, checkSoftwareUpdates);
	gd->timer->every(POST_TEMPERATURE_EVERY, postTemperature);

	pinMode(AC_CONTROL_PIN, OUTPUT);
	digitalWrite(AC_CONTROL_PIN, LOW);
}

void loop()
{
	ControllerData *gd = &GD;

	gd->thermostatServer->handleClient();
	gd->timer->update();
	WiFiManager::update();

	if (WL_CONNECTED == WiFi.status())
	{
		client.loop();

		if (!client.connected())
			connectToMqtt(true);
	}
}
