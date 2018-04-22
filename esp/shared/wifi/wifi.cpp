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

			// Blink blue led
			digitalWrite(BLUE_LED_PIN, !digitalRead(BLUE_LED_PIN));

			if (++connectionAttempts > 40)
			{
				Serial.println("Unable to connect.");
				ESP.restart();
			}
		}

		Serial.println();
		Serial.printf("Connected to: %s\n", config->ssid);
		Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());

		digitalWrite(BLUE_LED_PIN, LOW);

		if (mdns->begin(config->MDNSHost, WiFi.localIP()))
		{
			Serial.println("MDNS responder started.");
		}
	}
	// short blinks each 4 seconds
	digitalWrite(BLUE_LED_PIN, (millis() % 5000) < 4500);
}
