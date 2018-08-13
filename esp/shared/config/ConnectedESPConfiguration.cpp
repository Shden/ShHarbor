#include <ConnectedESPConfiguration.h>
#include <Arduino.h>
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
	String defaultMDNSHostName = "SH_ESP_" + ESP.getChipId();
	strncpy(configuration->MDNSHost, defaultMDNSHostName.c_str(), MDNS_HOST_LEN);

	// Initisalised flag
	configuration->initialised = EEPROM_INIT_CODE;

	// And put it back to EEPROM for the next time
	saveConfiguration(configuration, sizeof(ConnectedESPConfiguration));

	Serial.println("Restarting...");
	ESP.restart();
}

void storeStruct(void *data_source, size_t size)
{
	EEPROM.begin(size);
	for(size_t i = 0; i < size; i++)
	{
		char b = ((char *)data_source)[i];
		// Serial.print(b);
		EEPROM.write(i, b);
	}
	EEPROM.commit();
}

void loadStruct(void *data_dest, size_t size)
{
	EEPROM.begin(size);
	for(size_t i = 0; i < size; i++)
	{
		char b = EEPROM.read(i);
		// Serial.print(b);
		((char *)data_dest)[i] = b;
	}
}

// Getting configuration either from EEPROM or from console.
void loadConfiguration(ConnectedESPConfiguration* configuration, size_t configSize)
{
	// Read what's in EEPROM
	loadStruct(configuration, configSize);

	// // Debug:
	// Serial.printf("SSID configured: %s\n", configuration->ssid);
	// Serial.printf("Secret conigured: %s\n", configuration->secret);

	// Check if it has a proper signature
	Serial.println("Press any key to start configuration...");
	if (EEPROM_INIT_CODE != configuration->initialised || Serial.readString() != "")
	{
		getWiFiConfigurationTTY(configuration);
	}
	Serial.printf("SSID to connect: %s\n", configuration->ssid);

	// guard conditions
	if (strlen(configuration->ssid) > SSID_LEN)
		configuration->ssid[0] = '\0';

	if (strlen(configuration->secret) > SECRET_LEN)
		configuration->secret[0] = '\0';

	if (strlen(configuration->MDNSHost) > MDNS_HOST_LEN)
		configuration->MDNSHost[0] = '\0';
}

// Save controller configuration to EEPROM
void saveConfiguration(ConnectedESPConfiguration* configuration, size_t configSize)
{
	storeStruct(configuration, configSize);
}
