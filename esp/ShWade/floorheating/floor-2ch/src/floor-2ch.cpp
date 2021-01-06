/*
	Home: ShWade.

	2-channel floor heating module based on ESP8266 SoC.

	Features:
	- DS1820 temperature control sensor.
	- AC 220V power control.
	- REST API to monitor/contol heating parameters.
	- OTA firmware update.
	- Built in configuration web UI at /config.
	- WiFi access point to configure and troubleshoot.
	- ShWade specific: check total power consumption, power balancing.

	Toolchain: PlatformIO.
	Build commands:
	  pio run 		# for firmware
	  pio run -t buildfs	# for spiffs

	By denis.afanassiev@gmail.com

	API:
	curl 192.168.1.15/Status
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
#include <MQTT.h>
#include "secrets.h"

const int MQTT_PORT = 8883;
const char MQTT_SUB_TOPIC[] = "$aws/things/" THINGNAME "/shadow/update";
const char MQTT_PUB_TOPIC[] = "$aws/things/" THINGNAME "/shadow/update";

uint8_t DST = 0;
WiFiClientSecure net;
MQTTClient client;

BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);

#define ONE_WIRE_PIN            5

// outputs: U3, U4, U5
#define U3			D7		// GPIO_13
#define U4			D6		// GPIO_12
#define U5			D5		// GPIO_14

#define AC_CONTROL_PIN_1        U3		// first channel
#define AC_CONTROL_PIN_2	U5		// thrid channel

#define WEB_SERVER_PORT         80
#define UPDATE_TEMP_EVERY       (5000L)         // every 5 sec
#define CHECK_HEATING_EVERY     (15000L)        // every 15 sec
#define CHECK_SW_UPDATES_EVERY	(60000L*5)	// every 5 min
#define CHECK_1WIRE_SENSORS	(30000L)	// every 30 sec
#define POST_TEMPERATURE_EVERY	(60000L)	// every minute
#define DEFAULT_TARGET_TEMP	28.0
#define DEFAULT_ACTIVE		0
#define OTA_URL_LEN		80
#define ONE_WIRE_ADDR_LEN	16
#define MAX_ALLOWED_POWER	16500		// max power

#define TEXT_HTML		"text/html"
#define TEXT_PLAIN		"text/plain"
#define APPLICATION_JSON	"application/json"

typedef char DeviceAddressChar[ONE_WIRE_ADDR_LEN + 1];

void checkSoftwareUpdates();
float getTemperature(DeviceAddress);

struct ControllerData
{
	TemperatureSensor*      temperatureSensors;
	ESP8266WebServer*       thermostatServer;
	Timer*                  timer;
} GD;

/* heating channel, includes:
 * 	- DS1820 sensor address
 * 	- segment's power consumption
 */
struct HeatingChannel
{
	DeviceAddress		sensorAddress;
	int			heatingPower;
};

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
	HeatingChannel		heatingChannel[2];
} config;

unsigned long lastMillis = 0;
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

// MQTT errors printing
void lwMQTTErr(lwmqtt_err_t reason)
{
	if (reason == lwmqtt_err_t::LWMQTT_SUCCESS)
		Serial.print("Success");
	else if (reason == lwmqtt_err_t::LWMQTT_BUFFER_TOO_SHORT)
		Serial.print("Buffer too short");
	else if (reason == lwmqtt_err_t::LWMQTT_VARNUM_OVERFLOW)
		Serial.print("Varnum overflow");
	else if (reason == lwmqtt_err_t::LWMQTT_NETWORK_FAILED_CONNECT)
		Serial.print("Network failed connect");
	else if (reason == lwmqtt_err_t::LWMQTT_NETWORK_TIMEOUT)
		Serial.print("Network timeout");
	else if (reason == lwmqtt_err_t::LWMQTT_NETWORK_FAILED_READ)
		Serial.print("Network failed read");
	else if (reason == lwmqtt_err_t::LWMQTT_NETWORK_FAILED_WRITE)
		Serial.print("Network failed write");
	else if (reason == lwmqtt_err_t::LWMQTT_REMAINING_LENGTH_OVERFLOW)
		Serial.print("Remaining length overflow");
	else if (reason == lwmqtt_err_t::LWMQTT_REMAINING_LENGTH_MISMATCH)
		Serial.print("Remaining length mismatch");
	else if (reason == lwmqtt_err_t::LWMQTT_MISSING_OR_WRONG_PACKET)
		Serial.print("Missing or wrong packet");
	else if (reason == lwmqtt_err_t::LWMQTT_CONNECTION_DENIED)
		Serial.print("Connection denied");
	else if (reason == lwmqtt_err_t::LWMQTT_FAILED_SUBSCRIPTION)
		Serial.print("Failed subscription");
	else if (reason == lwmqtt_err_t::LWMQTT_SUBACK_ARRAY_OVERFLOW)
		Serial.print("Suback array overflow");
	else if (reason == lwmqtt_err_t::LWMQTT_PONG_TIMEOUT)
		Serial.print("Pong timeout");
}

void lwMQTTErrConnection(lwmqtt_return_code_t reason)
{
	if (reason == lwmqtt_return_code_t::LWMQTT_CONNECTION_ACCEPTED)
		Serial.print("Connection Accepted");
	else if (reason == lwmqtt_return_code_t::LWMQTT_UNACCEPTABLE_PROTOCOL)
		Serial.print("Unacceptable Protocol");
	else if (reason == lwmqtt_return_code_t::LWMQTT_IDENTIFIER_REJECTED)
		Serial.print("Identifier Rejected");
	else if (reason == lwmqtt_return_code_t::LWMQTT_SERVER_UNAVAILABLE)
		Serial.print("Server Unavailable");
	else if (reason == lwmqtt_return_code_t::LWMQTT_BAD_USERNAME_OR_PASSWORD)
		Serial.print("Bad UserName/Password");
	else if (reason == lwmqtt_return_code_t::LWMQTT_NOT_AUTHORIZED)
		Serial.print("Not Authorized");
	else if (reason == lwmqtt_return_code_t::LWMQTT_UNKNOWN_RETURN_CODE)
		Serial.print("Unknown Return Code");
}

// IoT thing message receved handler
void messageReceived(String &topic, String &payload)
{
	Serial.println("Recieved [" + topic + "]: " + payload);
}

// Connect to AWS MQTT
void connectToMqtt(bool nonBlocking = false)
{
	Serial.print("MQTT connecting ");
	while (!client.connected())
	{
		// MDNS host name is used as MQTT client name(?)
		if (client.connect(config.MDNSHost))
		{
			Serial.println("connected!");
			if (!client.subscribe(MQTT_SUB_TOPIC))
				lwMQTTErr(client.lastError());
		}
		else
		{
			Serial.print("failed, reason -> ");
			lwMQTTErrConnection(client.returnCode());
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
			StaticJsonDocument<2048> jsonResponce;
			// Serial.print("GET responce: ");
			// Serial.println(httpRequest.getString().c_str());
			deserializeJson(jsonResponce, httpRequest.getString());

			float power = jsonResponce["P"]["sum"];
			// Serial.print("getPowerConsumption: ");
			// Serial.println(power);
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
			"\"hall_floor_1\": " + String(getTemperature(config.heatingChannel[0].sensorAddress), 2) + ", " +
			"\"hall_floor_2\": " + String(getTemperature(config.heatingChannel[1].sensorAddress), 2) +
			"}}}}";
			
		if (!client.publish(MQTT_PUB_TOPIC, temperaturePayload, false, 0))
			lwMQTTErr(client.lastError());		
	}
}

// Go to sensor by address and get current temperature
float getTemperature(DeviceAddress sensorAddress)
{
	// Warning: uses global data.
	ControllerData *gd = &GD;

	return gd->temperatureSensors->getTemperature(sensorAddress);
}

// Update temperatureSensor internal data
void temperatureUpdate()
{
	for (uint8_t sensorIndex = 0; sensorIndex < 2; sensorIndex++)
	{
		float temp = getTemperature(config.heatingChannel[sensorIndex].sensorAddress);

		// Here goes workaround for %f which wasnt working right.
		Serial.printf("Temperature channel %d: %d.%02d\n", sensorIndex, (int)temp, (int)(temp*100)%100);
	}
}

// Heating control.
void controlHeating()
{
	float currentPower = getPowerConsumption();
	Serial.printf("currentPower = %f\n", currentPower);

	float tempDelta[2];
	tempDelta[0] = config.targetTemp - getTemperature(config.heatingChannel[0].sensorAddress);  
	tempDelta[1] = config.targetTemp - getTemperature(config.heatingChannel[1].sensorAddress);

	uint8_t neededState[2];
	neededState[0] = config.active && tempDelta[0] > 0;
	neededState[1] = config.active && tempDelta[1] > 0;

	uint8_t currentState[2];
	currentState[0] = digitalRead(AC_CONTROL_PIN_1);
	currentState[1] = digitalRead(AC_CONTROL_PIN_2);

	// Start from most cold segment
	uint8_t channelTraverseVector[2];
	if (tempDelta[0] > tempDelta[1])
	{
		channelTraverseVector[0] = 0;
		channelTraverseVector[1] = 1;
	}
	else
	{
		channelTraverseVector[0] = 1;
		channelTraverseVector[1] = 0;
	}

	// Traverse segments
	for (uint8_t i = 0; i < 2; i++)
	{
		uint8_t segemetIndex = channelTraverseVector[i];

		float powerDelta = 0;
		if (neededState[segemetIndex] > currentState[segemetIndex]) 
			powerDelta = config.heatingChannel[segemetIndex].heatingPower;
		if (neededState[segemetIndex] < currentState[segemetIndex]) 
			powerDelta = -config.heatingChannel[segemetIndex].heatingPower;

		uint8_t canChangePower = 
			!neededState[segemetIndex] || 
			(neededState[segemetIndex] && currentPower + powerDelta < MAX_ALLOWED_POWER);
		
		if (canChangePower)
		{
			uint8_t controlPins[2];
			controlPins[0] = AC_CONTROL_PIN_1;
			controlPins[1] = AC_CONTROL_PIN_2;

			digitalWrite(controlPins[segemetIndex], neededState[segemetIndex]);
			Serial.printf("Heating channel %d state: %d\n", segemetIndex, neededState[segemetIndex]);
			currentPower += powerDelta;
		}
		else
		{
			Serial.printf("Requred power (%fW) for channel %d is over limit, can't on.\n", currentPower + powerDelta, segemetIndex);
		}	
	}
}

// HTTP GET /status
void HandleHTTPGetStatus()
{
	// Warning: uses global data
	ControllerData *gd = &GD;

	String json =
	String("{ ") +
		"\"CurrentTemperature_ch0\" : " + String(getTemperature(config.heatingChannel[0].sensorAddress), 2) + ", " +
		"\"CurrentTemperature_ch1\" : " + String(getTemperature(config.heatingChannel[1].sensorAddress), 2) + ", " +
		"\"TargetTemperature\" : " + String(config.targetTemp, 2) + ", " +
		"\"Active\" : " + String(config.active) + ", " +
		"\"Heating_ch0\" : " + String(digitalRead(AC_CONTROL_PIN_1)) + ", " +
		"\"Heating_ch1\" : " + String(digitalRead(AC_CONTROL_PIN_2)) + ", " +
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
	if (key == "DS1820IDS") 
	{
		DeviceAddress sensor1Address, sensor2Address;

		memset(sensor1Address, 0, sizeof(DeviceAddress));
		memset(sensor2Address, 0, sizeof(DeviceAddress));

		gd->temperatureSensors->getAddress(0, sensor1Address);
		gd->temperatureSensors->getAddress(1, sensor2Address);

		DeviceAddressChar sensor1CharAddress, sensor2CharAddress;

		gd->temperatureSensors->deviceAddresToString(sensor1Address, sensor1CharAddress);
		gd->temperatureSensors->deviceAddresToString(sensor2Address, sensor2CharAddress);

		return 
			String(sensor1CharAddress) + ", heating is " + (digitalRead(AC_CONTROL_PIN_1) ? "on" : "off") + "\r\n" + 
			String(sensor2CharAddress) + ", heating is " + (digitalRead(AC_CONTROL_PIN_2) ? "on" : "off");
	} else
	if (key == "VERSION") return (String(getFWCurrentVersion())); else
	if (key == "T_TEMP") return String(config.targetTemp); else
	if (key == "DS1820_CH1_ADDR") 
	{
		DeviceAddressChar addressString;
		gd->temperatureSensors->deviceAddresToString(config.heatingChannel[0].sensorAddress, addressString);
		return String(addressString);
	} else
	if (key == "DS1820_CH2_ADDR") 
	{
		DeviceAddressChar addressString;
		gd->temperatureSensors->deviceAddresToString(config.heatingChannel[1].sensorAddress, addressString);
		return String(addressString);
	} else
	if (key == "CH1_POWER") return String(config.heatingChannel[0].heatingPower); else
	if (key == "CH2_POWER") return String(config.heatingChannel[1].heatingPower); else
	if (key == "CHECKED") return config.active ? "checked" : ""; else
	if (key == "OTA_URL") return String(config.OTA_URL); else
	return "Mapping value undefined.";
}

// // Debug request arguments printout.
// void dbgPostPrintout()
// {
// 	// Warning: uses global data
// 	ControllerData *gd = &GD;
//
// 	for (int i=0; i < gd->thermostatServer->args(); i++)
// 	{
// 		Serial.print(gd->thermostatServer->argName(i));
// 		Serial.print(": ");
// 		Serial.print(gd->thermostatServer->arg(i));
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

		// Channel 1
		DeviceAddressChar sensorAddress;
		gd->thermostatServer->arg("DS1820_CH1_ADDR").toCharArray(sensorAddress, ONE_WIRE_ADDR_LEN + 1);
		gd->temperatureSensors->stringToDeviceAddress(sensorAddress, config.heatingChannel[0].sensorAddress);

		param = gd->thermostatServer->arg("CH1_POWER");
		int power = param.toInt();
		if (power > 0 && power < 3000)
			config.heatingChannel[0].heatingPower = power;

		// Channel 2
		gd->thermostatServer->arg("DS1820_CH2_ADDR").toCharArray(sensorAddress, ONE_WIRE_ADDR_LEN + 1);
		gd->temperatureSensors->stringToDeviceAddress(sensorAddress, config.heatingChannel[1].sensorAddress);

		param = gd->thermostatServer->arg("CH2_POWER");
		power = param.toInt();
		if (power > 0 && power < 3000)
			config.heatingChannel[1].heatingPower = power;

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

// void refresh1WireSensorAddresses()
// {
// 	// Warning: uses global data
// 	ControllerData *gd = &GD;

// 	for (uint8_t sensorIndex = 0; sensorIndex < 2; sensorIndex++)
// 	{
// 		gd->temperatureSensors->getAddress(sensorIndex, gd->sensorAddress[sensorIndex]);
// 		gd->sensorAddress[sensorIndex][ONE_WIRE_ADDR_LEN] = '\0';
// 		Serial.printf("Sensor %d at %s\n\r", sensorIndex, gd->sensorAddress[sensorIndex]);
// 	}
// }

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
	gd->temperatureSensors = new TemperatureSensor(ONE_WIRE_PIN);
	pinMode(ONE_WIRE_PIN, INPUT_PULLUP);
	delay(500);

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

	// (?) client.setKeepAlive(30000);
	client.begin(MQTT_HOST, MQTT_PORT, net);
	client.onMessage(messageReceived);
	connectToMqtt();

	// Set up regulars
	gd->timer->every(UPDATE_TEMP_EVERY, temperatureUpdate);
	gd->timer->every(CHECK_HEATING_EVERY, controlHeating);
	gd->timer->every(CHECK_SW_UPDATES_EVERY, checkSoftwareUpdates);
	gd->timer->every(POST_TEMPERATURE_EVERY, postTemperature);

	pinMode(AC_CONTROL_PIN_1, OUTPUT);
	pinMode(AC_CONTROL_PIN_2, OUTPUT);
	digitalWrite(AC_CONTROL_PIN_1, LOW);
	digitalWrite(AC_CONTROL_PIN_2, LOW);
}

void loop()
{
	ControllerData *gd = &GD;

	gd->thermostatServer->handleClient();
	gd->timer->update();
	WiFiManager::update();

	now = time(nullptr);
	client.loop();
	if (!client.connected())
		connectToMqtt(true);
}
