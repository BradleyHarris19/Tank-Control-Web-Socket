/* creates a hotspot to join then runs the HTML
 * so far the hotspot works
 *
 *
 *
 */

#include <WiFi.h>


const char *ssid = "Tank control";
const char *password = "";

void setupHotspot() {

  Serial.println();
  Serial.print("Configuring access point...");

  WiFi.softAP(ssid, password);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

}
