#define CIRCULAR_BUFFER_XS
#include <CircularBuffer.h>
#include <NewPingESP8266.h>
#include "RGBdriver.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFiMulti.h>
#include <NTPClient.h>
#include <time.h>
const char* ssid = "WIFI-SSID";
const char* password = "WIFI-Password"
const char* webserverip = "192.168.0.250";
const char* www_username = "admin";
const char* www_password = "admin";
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "192.168.0.1", 3600, 60000);
CircularBuffer<int,50> parkstatetimestamp;
CircularBuffer<int,50> parking;
CircularBuffer<int,50> sensordoor;
CircularBuffer<int,50> sensordrive;
CircularBuffer<int,25> httpcodetimestamp;
CircularBuffer<int,25> httpcodes;
CircularBuffer<int,25> measuredvolts;
HTTPClient http;
ESP8266WebServer srv(80);
unsigned long previousMillis = millis();
int httpfailed = 1;
bool caraction = 0;
int distthresh = 230;
int carrangelow = 161; // used for ultrasonic sensor near door. needs to be adapted to your car.
int carrangehigh = carrangelow + 7; // in this range the car parking position is considered as OK
int red = 0;
int green = 0;
int blue = 0; // used to show wifi communication issues
bool OTAupdate = 0;
String randuri;
// distance sensors in front of the door and at the drive itself
#define echoPin1 D0
#define trigPin1 D1
#define echoPin2 D2
#define trigPin2 D3
NewPingESP8266 sensdoor(trigPin1, echoPin1);
NewPingESP8266 sensdrive(trigPin2, echoPin2);
// voltage sensor used to detect if the door is open or closed
#define analogInput A0
// REED Relay - used to open the door
#define openpin D5
// LED controller
#define LED D4 //GPIO02 Interne Led of NodeMCU Board */
#define CLK D6
#define DIO D7
RGBdriver RGBLEDs(CLK,DIO);

const char INDEX_HTML_START[] =
"<!DOCTYPE HTML><html><head><title>NDMCU-Garage Sensordata</title>"
"<style>table, tr, td { border: 1px solid black; border-collapse: collapse; text-align: center; }"
" table.frame, tr.frame, td.frame { border: 0px solid white; vertical-align: top; text-align: center; }</style>"
"</head><body><table class=\"frame\" width=\"95%\"><tr class=\"frame\"><td width=\"50%\" class=\"frame\">";

String generateparktable() {
  if (parking.isEmpty()) return "";
  String table = "<table align=\"center\"><tr><td><b>Date / Time</b></td><td><b>Parkstate</b></td><td><b>Drive</b></td><td><b>Door</b></td></tr>";
  for (int i = parking.size()-1; i >= 0; i--) {
    table += "<tr><td>"; table += timeconv(parkstatetimestamp[i]);
    table += "</td><td>";
    switch (parking[i]) {
      case 1:
        table += "no car here - all off";  break;
      case 2:
        table += "finished parking - light green"; break;
      case 3:
        table += "parked too far - light red"; break;
      case 4:
        table += "detected car - light red"; break;
      case 5:
        table += "car nearly parked - light red"; break;
      case 6:
        table += "What shall I do? - all off"; break;
      default:
        table += String(parking[i]); break;
    }
    table += "</td><td>"; table += String(sensordrive[i]);
    table += "</td><td>"; table += String(sensordoor[i]);
    table += "</td></tr>";
  }
  table += "</table>";
  return table;
}

String generatedoorstatetable() {
  if (httpcodes.isEmpty()) return "";
  String table = "<table align=\"center\"><tr><td><b>Date / Time</b></td><td><b>Code</b></td><td><b>Volt</b></td></tr>";
  for (int i = httpcodes.size()-1; i >= 0; i--) {
    table += "<tr><td>"; table += timeconv(httpcodetimestamp[i]);
    table += "</td><td>"; table += String(httpcodes[i]);
    table += "</td><td>"; table += String(float(measuredvolts[i])/100);
    table += "</td></tr>";
  }
  table += "</table>";
  return table;
}

String timeconv(int ts) {
  time_t t = ts;
  struct tm lt;
  (void) localtime_r(&t, &lt);
  char res[20];
  strftime(res, sizeof(res), "%Y-%m-%d %H:%M:%S", &lt);
  return res;
}

void handleRoot() {
  if (!srv.authenticate(www_username, www_password)) return srv.requestAuthentication();
  String parktable = generateparktable();
  String doorstatetable = generatedoorstatetable();
  srv.send(200, "text/html", INDEX_HTML_START + parktable + "</td><td width=\"50%\" class=\"frame\">" + doorstatetable + "</td></tr></table></body></html>");
  Serial.print("200 OK: "); Serial.println(srv.uri());
}

void returnFail(String msg) {
  srv.sendHeader("Connection", "close");
  srv.sendHeader("Access-Control-Allow-Origin", "*");
  srv.send(500, "text/plain", msg + "\r\n");
  Serial.print("500 Fail: "); Serial.println(srv.uri());  
}

void returnOK() {
  srv.sendHeader("Connection", "close");
  srv.sendHeader("Access-Control-Allow-Origin", "*");
  srv.send(200, "text/plain", "OK\r\n");
  Serial.print("200 OK: "); Serial.println(srv.uri());
}

void simulatedoorswitch() {
  digitalWrite(openpin, HIGH);
  delay(500);
  digitalWrite(openpin, LOW);
  Serial.println("pushed door button");
  returnOK();
  generatedoorswitchuri();
}

void handleReset() {
  Serial.println("Reset requested!");
  returnOK();
  delay(500);
  ESP.restart();
}

void handleNotFound() {
  if (srv.uri() == randuri) {
    Serial.println("found randuri request - processing");
    simulatedoorswitch();
    return;
  } else {
    Serial.print("current randuri: "); Serial.println(randuri);
  }
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += srv.uri();
  message += "\nMethod: ";
  message += (srv.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += srv.args();
  message += "\n";
  for (uint8_t i=0; i<srv.args(); i++){
    message += " " + srv.argName(i) + ": " + srv.arg(i) + "\n";
  }
  srv.send(404, "text/plain", message);
  Serial.print("404 Not found: "); Serial.println(srv.uri());
}

void wifi_connect() {
  WiFi.begin(ssid, password);

  Serial.print("WiFi connecting ");
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (blue == 0) blue = 32;
    else blue = 0;
    controlleds(red, green, blue);
    delay(500);
    Serial.print(".");
    i++;
    if (i >= 20 ) {
      Serial.print("failed: ");
      blue = 1;
      controlleds(red, green, blue);
      return;
    }
  }
  blue = 0;
  controlleds(red, green, blue);
  Serial.println("success!");

  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI): ");
  Serial.print(rssi);
  Serial.println("dBm");
}

void doorstatus(int action, unsigned long currentMillis) {
  String url;
  if (action > 1000) url = "/doorstate.php?state=1"; // door open
  else if (action < 500 && action >= 100) url = "/doorstate.php?state=2"; // door closed
  else if (action < 100) url = "/doorstate.php?state=3"; // door sensor error
  else url = "/doorstate.php?state=4&action=" + action; // door state unknown
  // url = "/doorstate.php?state=5"; // door test request
  Serial.print("Pushing doorstate to http://"); Serial.print(webserverip); Serial.print(url);
  http.begin(webserverip,80,url);
  int httpCode = http.GET();
  if (httpCode) {
    if (httpCode == 200) {
      Serial.println(" Success!");
      //String payload = http.getString(); Serial.println(payload);*/
      httpfailed = 0;
      previousMillis = currentMillis;
      blue = 0;
    } else {
      httpfailed++;
      Serial.print(" Failed! #"); Serial.println(httpfailed);
      if (httpfailed >= 600) handleReset();
    }
    httpcodes.unshift(httpCode);
    measuredvolts.unshift(action);
    httpcodetimestamp.unshift(timeClient.getEpochTime());
  }
  //Serial.println("closing connection");
  http.end();
}

void generatedoorswitchuri() {
  HTTPClient http;
  String url;
  String randuritmp;
  url = "/doorcontrol.php?switchpp3=1";
  Serial.print("Getting random from http://"); Serial.print(webserverip); Serial.println(url);
  http.begin(webserverip,80,url);
  int httpCode = http.GET();
  if (httpCode == 200) { 
    randuritmp = http.getString();
    Serial.println(randuritmp);
  } else {
    randuritmp = "doorpp";
  }
  http.end();
  randuri = "/" + randuritmp;
}

int measurevoltage() {
  float vin = 0.0;
  float R1 = 30000.0; 
  float R2 = 7500.0;
  int value = 0;
  value = analogRead(analogInput);
  vin = ((value * 3.0) / 1024.0) / (R2/(R1+R2));
  //Serial.print("Measured Voltage: "); Serial.println(vin);
  return vin*100;
}

void controlleds(int r, int g, int b) {
  //Serial.print("Set new color r/g/b: "); Serial.print(r); Serial.print("/"); Serial.print(g); Serial.print("/"); Serial.println(b);
  RGBLEDs.begin();
  RGBLEDs.SetColor(r, g, b);
  RGBLEDs.end();
  if (b == 0) digitalWrite(LED, HIGH);
  else digitalWrite(LED, LOW);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nSetup started!");
  pinMode(trigPin1, OUTPUT);
  pinMode(echoPin1, INPUT);
  pinMode(trigPin2, OUTPUT);
  pinMode(echoPin2, INPUT);
  pinMode(CLK, OUTPUT);
  pinMode(DIO, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(analogInput, INPUT);
  pinMode(openpin, OUTPUT);
  digitalWrite(openpin, LOW);
  WiFi.mode(WIFI_STA);
  wifi_connect();
  ArduinoOTA.setHostname("OTA-NDMCU-Garage");
  ArduinoOTA.onStart([]() {
    OTAupdate = 1;
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  generatedoorswitchuri();

  timeClient.begin();
  timeClient.update();

  srv.on("/", handleRoot);
  srv.on("/doorpp", generatedoorswitchuri);
  srv.on("/reset", handleReset);
  srv.onNotFound(handleNotFound);
  srv.begin();
  Serial.println("Server started on port 80.");
  
  Serial.println("\nSetup finished!");
}

void loop() {
  
  ArduinoOTA.handle();
  if (OTAupdate == 1) return;
  unsigned long currentMillis = millis();
  bool skipcoloring = 0;
  int parkingstate = 0;
  
// WiFi connect and reconnect, periodically doorstatus push
  if (WiFi.status() == WL_CONNECTED || WiFi.status() == WL_IDLE_STATUS) {
    srv.handleClient();
    if ((httpfailed != 0 && caraction == 0 && currentMillis - previousMillis > 10000) || (currentMillis - previousMillis > 60000 && httpfailed == 0)) {
      doorstatus(measurevoltage(), currentMillis);
      if (randuri == "doorpp") generatedoorswitchuri();
      timeClient.update();
    }
  } else if (currentMillis - previousMillis > 30000 && caraction == 0) {
    if (httpfailed >= 30) handleReset();
    httpfailed++;
    previousMillis = currentMillis;
    wifi_connect();
  }

// Distance measurement and LED control
  int distancedoor = sensdoor.ping_median(5) / US_ROUNDTRIP_CM;
  srv.handleClient();
  int distancedrive = sensdrive.ping_median(5) / US_ROUNDTRIP_CM;
  srv.handleClient();
  if (distancedoor == 0 || distancedrive == 0) return;
  if (((distancedoor >= distthresh) && (distancedrive >= distthresh)) || (((distancedoor >= 120) && (distancedoor <= 125)) && (distancedrive >= distthresh))) {
    //Serial.print("no car here - all off          ");
    if (red == 0 && green == 0) skipcoloring = 1;
    else {
      red = 0; green = 0;
    }
    parkingstate = 1; caraction = 0;
  } else if (((distancedoor >= carrangelow) && (distancedoor <= carrangehigh) || ((distancedoor > 115) && (distancedoor < 130)) || (distancedoor < 87)) && (distancedrive <= distthresh)) {
    //Serial.print("finished parking - light green ");
    if (red == 0 && green == 255) skipcoloring = 1;
    else {
      red = 0; green = 255;
    }
    parkingstate = 2; caraction = 0; 
  } else if ((distancedoor > carrangehigh) && (distancedoor <= distthresh) && (distancedrive <= distthresh)) {
    //Serial.print("parked too far - light red     ");
    if (red == 255 && green == 0) skipcoloring = 1;
    else {
      red = 255; green = 0;
    }
    parkingstate = 3; caraction = 1;
  } else if ((distancedoor <= distthresh) && (distancedrive >= distthresh)) {
    //Serial.print("detected car - light red   ");
    parkingstate = 4; caraction = 1; 
    if (red == 255 && green == 0) skipcoloring = 1;
    else {
      red = 255; green = 0;
    }
  } else if ((distancedoor <= distthresh) && (distancedrive <= distthresh)) {
    //Serial.print("car nearly parked - light red");
    parkingstate = 5; caraction = 1;
    if (red == 255 && green == 0) skipcoloring = 1;
    else {
      red = 255; green = 0;
    }
  } else {
    //Serial.print("What shall I do? - red/gr off");
    parkingstate = 6; 
    if (red == 0 && green == 0) skipcoloring = 1;
    else {
     red = 0; green = 0; 
    }
  }
  //Serial.print(" - distancedoor: "); Serial.print(distancedoor); Serial.print(" distancedrive: "); Serial.println(distancedrive);
  
  if (skipcoloring == 0) controlleds(red, green, blue);
  srv.handleClient();
  if (parking.first() != parkingstate || parking.isEmpty()) {
    parking.unshift(parkingstate);
    sensordoor.unshift(distancedoor);
    sensordrive.unshift(distancedrive);
    parkstatetimestamp.unshift(timeClient.getEpochTime());
  }
}
