#ifndef OTA_H
#define OTA_H

void updateAll(int firmwareVersion, int spiffsVersion, const char* repositoryBaseUrl);
void updateFirmware(int currentVersion, const char* repositoryBaseUrl);
void updateSpiffs(int currentVersion, const char* repositoryBaseUrl);

#endif
