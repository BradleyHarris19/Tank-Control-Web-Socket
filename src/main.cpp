/*
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */
#include <Arduino.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <WebServer.h>
#include <Servo.h>
#include <ESPmDNS.h>
#include <iostream>
#include <string>


#include "webpage.h" //Our HTML webpage file

//references to the two different types of connection
extern void setupWifi();
extern void setupHotspot();

//LED setup
static void writeLED(bool);
static void writewifiLED(bool);

//servo set up
static Servo servoX, servoY;
static const int servoPinX = GPIO_NUM_12;
static const int servoPinY = GPIO_NUM_27;

//DNS server
MDNSResponder mdns;

//sets up the websocket
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// sets LED pin
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

// Received data of format x,y [-100,+100]  e.g. -100,80
bool positionUpdateHandler(const char *data)
{
  if (!data)
    return false;

  String field;

  field = getValue(data, ',', 0);
  if (field == "") return false;
  int xpos = field.toInt();
  xpos = max(-100,min(xpos,100));

  field = getValue(data, ',', 1);
  if (field == "") return false;
  int ypos = field.toInt();
  ypos = max(-100,min(ypos,100));

  int X = 90 - (90*xpos)/100;
  int Y = 90 - (90*ypos)/100;

  Serial.printf("Servos=%d,%d\r\n",X,Y);

  servoX.write(X);
  servoY.write(Y);
  return true;
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

// wifi connected LED
static void writewifiLED(bool LEDon)
{
  if (LEDon) {
    digitalWrite(WIFILEDPIN, 1);
  }
  else {
    digitalWrite(WIFILEDPIN, 0);
  }
}

//on/off LED
static void writeLED(bool LEDon)
{
  LEDStatus = LEDon;
  if (LEDon) {
    digitalWrite(LEDPIN, 0);
  }
  else {
    digitalWrite(LEDPIN, 1);
  }
}


void setup()
{
  // Initialise LEDStatus
  pinMode(LEDPIN, OUTPUT);
  writeLED(false);

  pinMode(WIFILEDPIN, OUTPUT);
  writewifiLED(false);

  // Initialise Servos
  const int servoPulseMin(1000), servoPulseMax(2000);
  servoX.attach(servoPinX, servoPulseMin, servoPulseMax);
  servoY.attach(servoPinY, servoPulseMin, servoPulseMax);
  servoX.write(90);
  servoY.write(90);

  // Start serial port
  Serial.begin(921600);
  Serial.println();
  Serial.println();
  for(uint8_t t = 4; t > 0; t--)
  {
    Serial.printf("[SETUP] BOOT WAIT %d...\r\n", t);
    Serial.flush();
    delay(1000);
  }

  // set up type of connection
  int toggle = 22;
  pinMode (toggle, INPUT_PULLUP);
    if (digitalRead(toggle) == HIGH)
      setupWifi();
    else
      setupHotspot();

  /*int i;
    Serial.println("Do you have WiFi? (y/n) ");
    i = Serial.read();
    if (i=='y')
    setupWifi();
    else if (i=='n')
      setupHotspot();
  */
  writewifiLED(true);

  // Initialise server
  if (mdns.begin("espWebSock")) {
    Serial.println("MDNS responder started");
    mdns.addService("http", "tcp", 80);
    mdns.addService("ws", "tcp", 81);
  }
  else {
    Serial.println("MDNS.begin failed");
  }

  // Start server
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
