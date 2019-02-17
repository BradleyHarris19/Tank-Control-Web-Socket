



#include <WiFi.h>
#include <WiFiMulti.h>

#include "WifiCredentials.h"

WiFiMulti wifi;

void setupWifi()
{
  wifi.addAP(wifiSSID, wifiPassword);
  while(wifi.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(wifiSSID);
  Serial.print("Connect to http://espWebSock.local or http://");
  Serial.println(WiFi.localIP());
}
