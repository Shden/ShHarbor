/*
How it works:

WiFiManager is a module to support WiFi connectivity for ESP chip. Firts thing,
it shoud be initialised by calling init(cfg) function that receives configuration
structure with wifi credentials and other info. Init creates internal timer and
tries to connect to the wifi by calling handleWiFiConnectivity(). After init,
handleWiFiConnectivity() is called by timer with RECONNECTION_CYCLE interval.

Each reconnection cycle starts with checking connection status. If wifi is in
WL_CONNECTED, we are done. If not, we use current credentials to do
MAX_CONNECTION_ATTEMPTS to connect. If we managed to connect, we are done. If
not, a software access point (AP) is initialised for configuration and credentials
update. AP name as follows: [mDNS name of the module]_[ChipId].

Module should be in the loop() cycle via update() entry point.
*/
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <Timer.h>
#include <DNSServer.h>

#define MAX_CONNECTION_ATTEMPTS		200
#define RECONNECTION_CYCLE		4 * 60 * 1000	// each 4 minutes
#define BLUE_LED_PIN			2		// HIGH = off, LOW = on.
#define DNS_PORT			53		// DNS server

namespace WiFiManager
{
	ConnectedESPConfiguration* config;
	MDNSResponder* mDNS = new MDNSResponder();;
	Timer* connectionPulse = new Timer();

	// This is only for AP mode to redirect ALL domain request to configuration page
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
		// 	// short blinks each 4 seconds
		// 	digitalWrite(BLUE_LED_PIN, (millis() % 5000) < 4500);
	}

	// Ensure wifi connectivity
	void handleWiFiConnectivity()
	{
		if (WL_CONNECTED != WiFi.status())
		{
			Serial.println("Disconnected.");
			WiFi.mode(WIFI_STA);
			WiFi.begin(config->ssid, config->secret);

			// // --- brod debug
			// IPAddress my_ip(192,168,1,100);
			// IPAddress gw(192,168,1,1);
			// IPAddress subnet(255,255,255,0);
			//
			// WiFi.config(my_ip, gw, subnet);
			// // ---

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

				// // Blue led is ON as we are now connected
				digitalWrite(BLUE_LED_PIN, LOW);

				if (dnsServer) {
					delete dnsServer;
					dnsServer = NULL;
				}
			}
			else
			{
				Serial.println("Fallback to AP configuration.");

				String chipID = String(ESP.getChipId(), HEX);
				chipID.toUpperCase();
				String APName = String(config->MDNSHost) + String("_") + chipID;
				Serial.printf("Configuation access point: %s\n", APName.c_str());
				WiFi.mode(WIFI_AP);
				WiFi.softAP(APName.c_str());

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
	}
}
