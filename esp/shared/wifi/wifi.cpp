#include "wifi.h"

// Either WiFi is connected or restart.
void makeSureWiFiConnected(ConnectedESPConfiguration* config, MDNSResponder* mdns)
{
	if (WL_CONNECTED != WiFi.status())
	{
		Serial.println("Disconnected.");
		WiFi.begin(config->ssid, config->secret);
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
		Serial.printf("Connected to: %s\n", config->ssid);
		Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());

		if (mdns->begin(config->MDNSHost, WiFi.localIP()))
		{
			Serial.println("MDNS responder started.");
		}
	}
}
