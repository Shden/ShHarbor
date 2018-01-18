#ifndef OTA_H
#define OTA_H

void updateFirmware(int currentVersion, const char* repositoryBaseUrl);
void updateSpiffs(int currentVersion, const char* repositoryBaseUrl);

#endif
