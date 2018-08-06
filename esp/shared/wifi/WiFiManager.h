#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <ConnectedESPConfiguration.h>

namespace WiFiManager
{
	void init(ConnectedESPConfiguration* cfg);
	void update();
	void handleWiFiConnectivity();
}
// class WiFiManager
// {
// private:
// 	static ConnectedESPConfiguration* config;
// 	static MDNSResponder* mDNS;
// 	static Timer* connectionPulse;
// public:
// 	WiFiManager(ConnectedESPConfiguration* cfg)
// 	{
// 		config = cfg;
// 		mDNS = new MDNSResponder();
// 		connectionPulse = new Timer();
// 	};
// 	void handleWiFiConnectivity();
// };

#endif
