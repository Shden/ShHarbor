#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <Timer.h>
#include <DNSServer.h>

#define MAX_CONNECTION_ATTEMPTS		32
#define RECONNECTION_CYCLE		4 * 60 * 1000	// each 4 minutes
#define BLUE_LED_PIN			2		// HIGH = off, LOW = on.
#define DNS_PORT			53		// DNS server

namespace WiFiManager
{
	ConnectedESPConfiguration* config;
	MDNSResponder* mDNS = new MDNSResponder();;
	Timer* connectionPulse = new Timer();
	DNSServer* dnsServer = NULL;

	void init(ConnectedESPConfiguration* cfg)
	{
		config = cfg;

		// Use blue led to indicate wifi connection.
		pinMode(BLUE_LED_PIN, OUTPUT);
		digitalWrite(BLUE_LED_PIN, HIGH);

		handleWiFiConnectivity();

		connectionPulse->every(RECONNECTION_CYCLE, handleWiFiConnectivity);
	}

	void update()
	{
		connectionPulse->update();
		if (dnsServer)
			dnsServer->processNextRequest();
	}

	// Ensure wifi connectivity
	void handleWiFiConnectivity()
	{
		if (WL_CONNECTED != WiFi.status())
		{
			Serial.println("Disconnected.");
			WiFi.mode(WIFI_STA);
			WiFi.begin(config->ssid, config->secret);
			Serial.print("Connecting to WiFi: ");

			// Connection loop
			int connectionAttempts = 0;
			while (WiFi.status() != WL_CONNECTED)
			{
				delay(500);
				Serial.print(".");

				// Blink blue led
				digitalWrite(BLUE_LED_PIN, !digitalRead(BLUE_LED_PIN));

				if (++connectionAttempts > MAX_CONNECTION_ATTEMPTS)
				{
					Serial.println(" failed.");
					break;
				}
			}

			if (WL_CONNECTED == WiFi.status())
			{
				// Connected:
				Serial.println();
				Serial.printf("Connected to: %s\n", config->ssid);
				Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());

				if (mDNS->begin(config->MDNSHost, WiFi.localIP()))
				{
					Serial.println("MDNS responder started.");
				}

				// No need in SoftAP, disconnect
				// True will switch the soft-AP mode off
				WiFi.softAPdisconnect(true);

				// Blue led is ON as we are now connected
				digitalWrite(BLUE_LED_PIN, LOW);

				if (dnsServer) {
					delete dnsServer;
					dnsServer = NULL;
				}
			}
			else
			{
				Serial.println("Fallback to AP configuration.");
				Serial.printf("Configuation access point: %s\n", config->MDNSHost);
				WiFi.mode(WIFI_AP);
				WiFi.softAP(config->MDNSHost);
				Serial.printf("Configuation access point IP address: %s\n", WiFi.softAPIP().toString().c_str());

				Serial.printf("Next WiFi connection attempt in %d ms.\n", RECONNECTION_CYCLE);

				// Blue led is OFF as we are disconnected
				digitalWrite(BLUE_LED_PIN, HIGH);

				/* Setup the DNS server redirecting all the domains to the apIP */
				dnsServer = new DNSServer();
				dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
				dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
			}
		}
	// 	// short blinks each 4 seconds
	// 	digitalWrite(BLUE_LED_PIN, (millis() % 5000) < 4500);
	}
}
