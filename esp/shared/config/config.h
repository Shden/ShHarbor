#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define SSID_LEN		80
#define SECRET_LEN		80
#define MDNS_HOST_LEN		24
#define EEPROM_INIT_CODE	28465

// Basic configuration layout for connected ESP
struct ConnectedESPConfiguration
{
	int16_t		initialised;
	char		ssid[SSID_LEN + 1];
	char		secret[SECRET_LEN + 1];
	char		MDNSHost[MDNS_HOST_LEN + 1];
};

void loadConfiguration(ConnectedESPConfiguration*, size_t);
void saveConfiguration(ConnectedESPConfiguration*, size_t);

#endif
