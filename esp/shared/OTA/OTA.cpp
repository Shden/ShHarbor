/*
	ESP OTA update.
	based on https://www.bakke.online/index.php/2017/06/02/self-updating-ota-firmware-for-esp8266/
*/

#include <Arduino.h>
#include <OTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

// Checks updates repository for the version available
int getAvailableVersion(int currentVersion, const char* repositoryBaseUrl)
{
	int newVersion = currentVersion;

	String versionInfoURL = String(repositoryBaseUrl);
	versionInfoURL.concat("version.info");

	Serial.print("version.info URL: ");
	Serial.println(versionInfoURL);

	HTTPClient httpClient;
	httpClient.begin(versionInfoURL);
	int httpCode = httpClient.GET();
	if (200 == httpCode)
	{
		String newFWVersion = httpClient.getString();

		Serial.print("Current version: ");
		Serial.println(currentVersion);
		Serial.print("Available version: ");
		Serial.println(newFWVersion);

		newVersion = newFWVersion.toInt();

		if (newVersion > currentVersion)
			Serial.println("New version available.");
		else
			Serial.println("Already on latest version.");
	}
	else
	{
		Serial.print("Version check failed, got HTTP response code: ");
		Serial.println( httpCode );
	}
	httpClient.end();
	return newVersion;
}

void handeUpdateResult(t_httpUpdate_return updateReturnCode)
{
	switch(updateReturnCode)
	{
		case HTTP_UPDATE_FAILED:
			Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s",
				ESPhttpUpdate.getLastError(),
				ESPhttpUpdate.getLastErrorString().c_str());
			break;

		case HTTP_UPDATE_NO_UPDATES:
			Serial.println("HTTP_UPDATE_NO_UPDATES");
			break;
	}
}

void updateFirmware(int currentVersion, const char* repositoryBaseUrl)
{
	Serial.println("Firmware update check.");
	if (currentVersion < getAvailableVersion(currentVersion, repositoryBaseUrl))
	{
		// New firmware URL
		String newFirmwareURL = repositoryBaseUrl;
		newFirmwareURL.concat("FW.bin");
		Serial.print("Updating SPIFFS using image URL: ");
		Serial.println(newFirmwareURL);

		handeUpdateResult(ESPhttpUpdate.update(newFirmwareURL));
		ESP.restart();
	}
}

void updateSpiffs(int currentVersion, const char* repositoryBaseUrl)
{
	Serial.println("SPIFFS update check.");
	if (currentVersion < getAvailableVersion(currentVersion, repositoryBaseUrl))
	{
		// New SPIFFS URL
		String newSpiffsURL = repositoryBaseUrl;
		newSpiffsURL.concat("SPIFFS.bin");
		Serial.print("Updating SPIFFS using image URL: ");
		Serial.println(newSpiffsURL);

		handeUpdateResult(ESPhttpUpdate.updateSpiffs(newSpiffsURL));
		ESP.restart();
	}
}

// Go check if there is a new firmware or SPIFFS got available.
//
// Note: ESP doesnt seem to handle one stop update of FW + SPIFFS, at least
// it didn't work for me. I assume the update process includes upload of the
// new image to a memory buffer then reboot when it replaces either FW or
// SPIFFS. This effectively means that updating of both FW and SPIFFS would
// take 2 cycles including rebooting. Thus 2 independent versions should be
// supported, one for FW and one for SPIFFS.
void updateAll(int firmwareVersion, int spiffsVersion, const char* repositoryBaseUrl)
{
	updateSpiffs(spiffsVersion, repositoryBaseUrl);
	updateFirmware(firmwareVersion, repositoryBaseUrl);
}
