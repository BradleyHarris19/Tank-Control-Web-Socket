/*
 * ESP8266 Web server with Web Socket to control an LED.
 *
 * The web server keeps all clients' LED status up to date and any client may
 * turn the LED on or off.
 *
 * For example, clientA connects and turns the LED on. This changes the word
 * "LED" on the web page to the color red. When clientB connects, the word
 * "LED" will be red since the server knows the LED is on.  When clientB turns
 * the LED off, the word LED changes color to black on clientA and clientB web
 * pages.
 *
 * References:
 *
 * https://github.com/Links2004/arduinoWebSockets
 *
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Servo.h>

#include "WifiCredentials.h"
#include "webpage.h" //Our HTML webpage contents

MDNSResponder mdns;

static void writeLED(bool);
static void writewifiLED(bool);

static Servo servoX, servoY;
static const int servoPinX = GPIO_NUM_13;
static const int servoPinY = GPIO_NUM_14;

WiFiMulti wifi;
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Internal LED pin for LOLIN32
const int LEDPIN = GPIO_NUM_25;
// Internal LED pin for connection to wifi
const int WIFILEDPIN = GPIO_NUM_21;
// Current LED status
bool LEDStatus;

// Commands sent through Web Socket
const char LEDON[] = "ledon";
const char LEDOFF[] = "ledoff";

String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

bool positionUpdateHandler(const char *data)
// Received data of format x,y e.g. -100,80
{
  {
    String field;

    field = getValue(data, ',', 0);
    if (field == "") return false;
    int xpos = field.toInt();

    field = getValue(data, ',', 1);
    if (field == "") return false;
    int ypos = field.toInt();

    int X = 90 - (90*xpos)/100;
    int Y = 90 - (90*ypos)/100;

    servoX.write(X);
    servoY.write(Y);
    return true;
  }

  return false;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  Serial.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\r\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        // Send the current LED status
        if (LEDStatus) {
          webSocket.sendTXT(num, LEDON, strlen(LEDON));
        }
        else {
          webSocket.sendTXT(num, LEDOFF, strlen(LEDOFF));
        }
      }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\r\n", num, payload);

      if (strcmp(LEDON, (const char *)payload) == 0) {
        writeLED(true);
      }
      else if (strcmp(LEDOFF, (const char *)payload) == 0) {
        writeLED(false);
      }
      else if (positionUpdateHandler((const char *)payload)) {
        // Nothing else to do
      }
      else {
        Serial.println("Unknown command");
      }
      // send data to all connected clients
      webSocket.broadcastTXT(payload, length);
      break;
    case WStype_BIN:
      Serial.printf("[%u] get binary length: %u\r\n", num, length);
      //hexdump(payload, length);

      // echo data back to browser
      webSocket.sendBIN(num, payload, length);
      break;
    default:
      Serial.printf("Invalid WStype [%d]\r\n", type);
      break;
  }
}

void handleRoot()
{
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

static void writewifiLED(bool LEDon)
{
  // Note inverted logic for Adafruit HUZZAH board
  if (LEDon) {
    digitalWrite(WIFILEDPIN, 1);
  }
  else {
    digitalWrite(WIFILEDPIN, 0);
  }
}

static void writeLED(bool LEDon)
{
  LEDStatus = LEDon;
  // Note inverted logic for Adafruit HUZZAH board
  if (LEDon) {
    digitalWrite(LEDPIN, 0);
  }
  else {
    digitalWrite(LEDPIN, 1);
  }
}

void setup()
{
  pinMode(LEDPIN, OUTPUT);
  writeLED(false);

  pinMode(WIFILEDPIN, OUTPUT);
  writewifiLED(false);

  servoX.attach(servoPinX);
  servoY.attach(servoPinY);
  servoX.write(90);
  servoY.write(90);

  Serial.begin(921600);
  Serial.println();
  Serial.println();
  for(uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] BOOT WAIT %d...\r\n", t);
    Serial.flush();
    delay(1000);
  };

  wifi.addAP(wifiSSID, wifiPassword);
  while(wifi.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(wifiSSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

   writewifiLED(true);

  if (mdns.begin("espWebSock")) {
    Serial.println("MDNS responder started");
    mdns.addService("http", "tcp", 80);
    mdns.addService("ws", "tcp", 81);
  }
  else {
    Serial.println("MDNS.begin failed");
  }
  Serial.print("Connect to http://espWebSock.local or http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop()
{
  webSocket.loop();
  server.handleClient();
}
