#include "config.h"
#include <EEPROM.h>

// Get character sting from terminal.
int readString(char* buff, size_t buffSize)
{
	memset(buff, '\0', buffSize);

	size_t count = 0;
	while (count < buffSize)
	{
		if (Serial.available())
		{
			char c = Serial.read();
			if (isprint(c))
			{
				buff[count++] = c;
				Serial.print(c);
			}
			else if (count > 0 && (c == '\r' || c == '\n'))
			{
				Serial.println();
				return 1;
			}
		}
		yield();
	}
	return 0; // reached end of buffer
}

// Get WiFi configuration data interactively via TTY and save to EEPROM.
void getWiFiConfigurationTTY(ConnectedESPConfiguration* configuration)
{
	// Ask user for config values
	Serial.print("SSID: ");
	readString(configuration->ssid, SSID_LEN);
	Serial.print("Secret: ");
	readString(configuration->secret, SECRET_LEN);

	// Defaults
	strncpy(configuration->MDNSHost, "ESP" + ESP.getChipId(), MDNS_HOST_LEN);

	// Initisalised flag
	configuration->initialised = EEPROM_INIT_CODE;

	// And put it back to EEPROM for the next time
	saveConfiguration(configuration);

	Serial.println("Restarting...");
	ESP.restart();
}

// Getting configuration either from EEPROM or from console.
void loadConfiguration(ConnectedESPConfiguration* configuration)
{
	// Read what's in EEPROM
	EEPROM.begin(sizeof(*configuration));
	EEPROM.get(0, *configuration);
	EEPROM.end();

	// // Debug:
	// Serial.printf("SSID configured: %s\n", config.ssid);
	// Serial.printf("Secret conigured: %s\n", config.secret);

	// Check if it has a proper signature
	Serial.println("Press any key to start configuration...");
	if (EEPROM_INIT_CODE != configuration->initialised || Serial.readString() != "")
	{
		getWiFiConfigurationTTY(configuration);
	}
	Serial.printf("SSID to connect: %s\n", configuration->ssid);
}

// Save controller configuration to EEPROM
void saveConfiguration(ConnectedESPConfiguration* configuration)
{
	EEPROM.begin(sizeof(*configuration));
	EEPROM.put(0, *configuration);
	EEPROM.commit();
	EEPROM.end();
}
