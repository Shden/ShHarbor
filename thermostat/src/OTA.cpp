/*
	ESP OTA update.
	based on https://www.bakke.online/index.php/2017/06/02/self-updating-ota-firmware-for-esp8266/
*/

#include <Arduino.h>
#include <OTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

void checkForUpdates(int currentVersion, const char* fwUrlBase)
{
	String fwVersionURL = String(fwUrlBase);
	fwVersionURL.concat("version.info");

	Serial.println("Checking for firmware updates.");
	Serial.print("Firmware version URL: ");
	Serial.println(fwVersionURL);

	HTTPClient httpClient;
	httpClient.begin(fwVersionURL);
	int httpCode = httpClient.GET();
	if (200 == httpCode)
	{
		String newFWVersion = httpClient.getString();

		Serial.print("Current firmware version: ");
		Serial.println(currentVersion);
		Serial.print("Available firmware version: ");
		Serial.println(newFWVersion);

		int newVersion = newFWVersion.toInt();

		if (newVersion > currentVersion)
		{
			Serial.println("Preparing to update");

			String fwImageURL = fwUrlBase;
			fwImageURL.concat("SHH-TS-");
			fwImageURL.concat(String(newVersion));
			fwImageURL.concat(".bin");
			Serial.print("Firmware image URL: ");
			Serial.println(fwImageURL);

			t_httpUpdate_return ret = ESPhttpUpdate.update(fwImageURL);

			switch(ret)
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
		else
		{
			Serial.println("Already on latest version");
		}
	}
	else
	{
		Serial.print("Firmware version check failed, got HTTP response code: ");
		Serial.println( httpCode );
	}
	httpClient.end();
}
