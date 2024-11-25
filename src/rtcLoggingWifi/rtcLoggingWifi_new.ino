#include "arduino_secrets.h"


/* rtcLoggingWifi.ino

   This runs a webserver that serves the files from the SD card while logging 
   load cell measurements for a set amount of time before and after an RFID
   tag is read

   2024 August 27
*/

#include <SPI.h>  // SD card
#include <SdFat.h>  // Adafruit fork of SdFat for SD card
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include <Adafruit_GFX.h>
#include <Adafruit_AHTX0.h>    // temp/humidity
#include <Adafruit_NAU7802.h>  // load cell ADC
#include "RTClib.h"  // Real-time clock (PCF8523)
#include "circle_buf.h"  // circular buffer (Boulder Flight version) 

#include <PCA9536D.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

const char* default_ssid = "EagleNet";
const char* default_password = "";
const char* default_apssid = "birdfeeder";
const char* default_appassword = "password1";
char ssid[32];
char password[32];
char apssid[32];
char appassword[32];
int wifiMaxCheckTimes = 10;
int timeZone = 0;  // UTC-0

#define SD_CS             10  // SD card chip select
#define I2C_PWR_PIN        7   // pin to turn on/off I2C power, set high for on
SdFat SD;  // required with SdFat.h, not SD.h
SdFile myFile;  // not sure why SdFile and not just File
File32 myFileLoadCell;
File32 myFileHousekeeping;
String fname = "/settings.txt";  // forward slash critical for ESP32 function
String fname2;
String baseName = "/data";
String ext = ".csv";
int lastFileNum = 0;
Adafruit_NAU7802 nau;  // load cell 
Adafruit_AHTX0 aht;    // Temperature / humidity
RTC_PCF8523 rtc;
unsigned long afterReadRecordTime = 8000;  // number of milliseconds after last tag read to record load cell data
const uint32_t bufLen = 100;

Adafruit_INA219 solar1(0x40);
Adafruit_INA219 solar2(0x44);
PCA9536 buzzLED;
#define RED 0
#define GRN 1
#define BLU 2
#define BUZZ 3

struct loadPoint {
  DateTime t;
  int32_t load;
  unsigned long millis;
};

WebServer *server0, *server1, *server2;

#ifdef LED_BUILTIN
const int led = LED_BUILTIN;
#else
const int led = 13;
#endif

void handleRoot(WebServer *server, const String &content) {
  digitalWrite(led, 1);
  server->send(200, "text/html", content);
  digitalWrite(led, 0);
}

void handleDownload0() {
  // check that an argument of type filename has been submitted
  if(server0->args() < 1) {
    server0->send(200, "text/plain", "Need argument of type ?filename=somename.csv");
    return;
  }
  // get the size of the file
  // create the response with the file contents
  String fname = "junk.txt";
  for(int ii = 0;ii < server0->args();ii++) {
    Serial.println("Arg " + String(ii) + " -> ");
    Serial.println(server0->argName(ii) + ": ");
    Serial.println(server0->arg(ii) + "\n");
    if(server0->argName(ii) == "filename") {
      fname = "/";  // very important for ESP32 based filenames
      fname += server0->arg(ii);
      break;
    }
  }
  Serial.println("Downloading file: " + fname);
  File32 fptr; // = SD.open(fname.c_str());

  if(!fptr.open(fname.c_str(), O_RDONLY)) {
    // file doesn't exist
    Serial.println("File " + fname + " doesn't exist");
    server0->send(200, "text/plain", "File " + fname + " doesn't exist");
    return;
  }
  Serial.println("Filesize " + String(fptr.fileSize()));
  server0->streamFile(fptr, "text/plain");
}

/* handleFileExample0

   This shows how to process arguments and create
   a file that the client will be asked to download.
   It expects a URL like: 
   http://birds.local:8080/file?asdf=5&something=else
*/
void handleFileExample0() {
  String message = "";
  message += "# args: ";
  message += server0->args();
  message += "\n";
  for(int ii = 0;ii < server0->args();ii++) {
    message += "Arg " + String(ii) + " -> ";
    message += server0->argName(ii) + ": ";
    message += server0->arg(ii) + "\n";
  }
  server0->sendContent("HTTP/1.1 200 OK\n");
  server0->sendContent("Content-Type: text/plain\n");
  server0->sendContent("Content-Disposition: attachment; filename=junk.txt\n");
  server0->sendContent("Content-Length: " + String(message.length()) + "\n");
  server0->sendContent("Connection: close\n");
  server0->sendContent("\n");  // to indicate headers have ended
  // server0->sendHeader("Content-Disposition", "attachment; filename=junk.txt");
  server0->sendContent(message);
  // server0->send(200,"text/plain", message.c_str());
}

void handleRoot0() {
  String outStr = "<html><head><title>Bird Files</title></head><body><h1>Files</h1><br>\n";
  outStr += "<strong>Local Access Point:&nbsp;</strong>";
  outStr += apssid;
  outStr += "<br>\n<strong>Local Access Point Password:&nbsp;</strong>";
  outStr += appassword;
  outStr += "<br><br>\n\n<strong>WiFi AccessPoint: </strong>";
  outStr += ssid;
  outStr += "<br>\n<strong>WiFi Access Point Password: </strong>";
  outStr += password;
  outStr += "<br><br>\n\n";
  
  File32 rootDir = SD.open("/");
  while(true) {
    File32 entry = rootDir.openNextFile();
    if(!entry) {
      outStr += "No more files<br><br>\n";
      outStr += "<strong>Edit Real-Time-Clock</strong><br><br>"
        "<form action=\"/rtcForm\" method=\"get\">"
          "<label for=\"rtcValue\">MM/DD/YYYY HH:MM:SS:</label>"
          "<input type=\"text\" id=\"rtcValue\" name=\"rtcValue\">"
          "<input type=\"submit\" value=\"Submit\">"
        "</form>"
        "<br>"
        "<form action=\"/timeZoneForm\" method=\"get\">"
          "<label for=\"timeZoneValue\">Timezone UTC(#):</label>"
          "<input type=\"text\" id=\"timeZoneValue\" name=\"timeZoneValue\">"
          "<input type=\"submit\" value=\"Submit\">"
        "</form>"
        "<br>"
        "<strong>Edit settings.txt</strong> (Restart ESP32-S3 to apply changes)"  // Too lazy to implement a better fix
        "<br><br>"
        "<form action=\"/wifiSSIDForm\" method=\"get\">"
          "<label for=\"wifiSSIDValue\">WiFi Access Point SSID:</label>"
          "<input type=\"text\" id=\"wifiSSIDValue\" name=\"wifiSSIDValue\">"
          "<input type=\"submit\" value=\"Submit\">"
        "</form>"
        "<br>"
        "<form action=\"/wifiPasswordForm\" method=\"get\">"
          "<label for=\"wifiPasswordValue\">WiFi Access Point Password:</label>"
          "<input type=\"text\" id=\"wifiPasswordValue\" name=\"wifiPasswordValue\">"
          "<input type=\"submit\" value=\"Submit\">"
        "</form>"
        "<br>"
        "<form action=\"/apSSIDForm\" method=\"get\">"
          "<label for=\"apSSIDValue\">Local Access Point SSID:</label>"
          "<input type=\"text\" id=\"apSSIDValue\" name=\"apSSIDValue\">"
          "<input type=\"submit\" value=\"Submit\">"
        "</form>"
        "<br>"
        "<form action=\"/apPasswordForm\" method=\"get\">"
          "<label for=\"apPasswordValue\">Local Access Point Password:</label>"
          "<input type=\"text\" id=\"apPasswordValue\" name=\"apPasswordValue\">"
          "<input type=\"submit\" value=\"Submit\">"
        "</form>"
        "<br>"
            "<form action=\"/restart\" method=\"post\">"  // Teehee (Late Night Delerium Hittin HARD)
              "<input type=\"submit\" value=\"Restart ESP32\">"
            "</form>";
      outStr += "</body></html>";
      break;
    }
    char nameField[16];  //  Allows files with names less than 16 characters. Perhaps should be upped to 32?
    int nameLen = entry.getName(nameField, 16);  
    if(nameLen > 0 && nameField[0] != '.') {
      uint16_t mDate = 0;
      uint16_t mTime = 0;
      
      //entry.getModifyDateTime(&mDate, &mTime);
      //entry.printFatDate(&Serial,&mDate);
      //entry.printModifyDateTime(&Serial);
      outStr += "<a href=\"download?filename=";
      outStr += nameField;
      outStr += "\">";
      outStr += nameField;
      outStr += "</a> ";
      outStr += (float)entry.fileSize()/1024.0/1024.0;
      outStr += " MB";
      if (String(nameField) == fname.substring(1)) {
        // outStr += "<strong> << Last Update: </strong> ";
        // outStr += curTimeStr(rtc.now());
        outStr += "<strong> << RTC Time:</strong> <span id=\"serverTime\"></span> (UTC";
        outStr += String(timeZone);
        outStr += ") ";
        outStr += "<strong> << Current Time:</strong> <span id=\"clock\"></span> ";
        outStr += "<strong> << Time Difference:</strong> <span id=\"timeDifference\"></span><br><br>\n";
        outStr += "<script>"
                  "function updateRTCTime() {"
                  "  fetch('/rtcTime').then(response => response.text()).then(time => {"
                  "    document.getElementById('serverTime').textContent = time;"
                  "  }).catch(error => console.error('Error fetching server time:', error));"
                  "}"
                  "function updateClock() {"
                  "  var now = new Date();"
                  "  var day = now.getDate().toString().padStart(2, '0');"
                  "  var month = (now.getMonth() + 1).toString().padStart(2, '0');"
                  "  var year = now.getFullYear();"
                  "  var hours = now.getHours().toString().padStart(2, '0');"
                  "  var minutes = now.getMinutes().toString().padStart(2, '0');"
                  "  var seconds = now.getSeconds().toString().padStart(2, '0');"
                  "  var dateString = month + '/' + day + '/' + year;"
                  "  var timeString = hours + ':' + minutes + ':' + seconds;"
                  "  var dateTimeString = dateString + ' ' + timeString;"
                  "  document.getElementById('clock').textContent = dateTimeString;"
                  "  updateTimeDifference();"
                  "}"
                  "function updateTimeDifference() {"
                  "  var serverTimeStr = document.getElementById('serverTime').textContent;"
                  "  var clientTimeStr = document.getElementById('clock').textContent;"
                  "  if (serverTimeStr && clientTimeStr) {"
                  "    var serverTime = new Date(serverTimeStr);"
                  "    var clientTime = new Date(clientTimeStr);"
                  "    var difference = Math.abs(clientTime - serverTime);"
                  "    var seconds = Math.floor(difference / 1000);"
                  "    var minutes = Math.floor(seconds / 60);"
                  "    seconds = seconds % 60;"
                  "    var hours = Math.floor(minutes / 60);"
                  "    minutes = minutes % 60;"
                  "    var timeDifferenceStr = hours + 'h ' + minutes + 'm ' + seconds + 's';"
                  "    document.getElementById('timeDifference').textContent = timeDifferenceStr;"
                  "  }"
                  "}"
                  "setInterval(updateRTCTime, 1000);"
                  "setInterval(updateClock, 1000);"
                  "updateRTCTime();"
                  "updateClock();"
                  "</script>";
      }
      outStr += "<br>\n";
    }
  }
  handleRoot(server0, outStr);
}

void handleRoot1() {
  handleRoot(server1, "Hello from server1 who listens only on WLAN");
}

void handleRoot2() {
  handleRoot(server2, "Hello from server2 who listens only on own Soft AP");
}

void handleNotFound(WebServer *server) {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server->uri();
  message += "\nMethod: ";
  message += (server->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";
  for (uint8_t i = 0; i < server->args(); i++) {
    message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
  }
  server->send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void handleNotFound0() {
  handleNotFound(server0);
}

void handleNotFound1() {
  handleNotFound(server1);
}

void handleNotFound2() {
  handleNotFound(server2);
}

void loadSettings(String settingFname) {
  File32 fptr;

  // initialize the variables with default values
  strcpy(ssid, default_ssid);
  strcpy(password, default_password);
  strcpy(apssid, default_apssid);
  strcpy(appassword, default_appassword);

  if(!fptr.open(settingFname.c_str(), O_RDONLY)) {
    // file doesn't exist
    Serial.println("Please create a " + settingFname + " file. Using defaults.");
    return;
  }
  const uint8_t size = 80; // max line length
  char str[size];

  int rtn = parseSetting(&fptr, str, size, ':');
  while(rtn > 0) {
    if(rtn == '#') {
      Serial.println("Comment parsed");
    } else {
      // Serial.print("rtn = "); Serial.println(rtn);
      // Serial.print("str = "); Serial.println(str);
      // accesspoint ssid name
      // 58 is :, 10 is \n
      if((rtn == 58) && (strcmp(str, "apssid") == 0)) {
        Serial.print("Access point ssid is ");
        rtn = parseSetting(&fptr, str, size, ':');
        if(rtn == 10) {
          Serial.println(str);
        }
        // copy to the variable
        strcpy(apssid, str);
      }
      if((rtn == 58) && (strcmp(str, "appass") == 0)) {
        Serial.print("Access point password is ");
        rtn = parseSetting(&fptr, str, size, ':');
        if(rtn == 10) {
          Serial.println(str);
        }
        // copy to the variable
        strcpy(appassword, str);
      }
      if((rtn == 58) && (strcmp(str, "wifissid") == 0)) {
        Serial.print("WiFi SSID is ");
        rtn = parseSetting(&fptr, str, size, ':');
        if(rtn == 10) {
          Serial.println(str);
        }
        // copy to the variable
        strcpy(ssid, str);
      }
      if((rtn == 58) && (strcmp(str, "wifipass") == 0)) {
        Serial.print("WiFi password is ");
        rtn = parseSetting(&fptr, str, size, ':');
        if(rtn == 10) {
          Serial.println(str);
        }
        // copy to the variable
        strcpy(password, str);
      }
      if((rtn == 58) && (strcmp(str, "timezone") == 0)) {
        Serial.print("Access point time zone is ");
        rtn = parseSetting(&fptr, str, size, ':');
        if(rtn == 10) {
          Serial.println(str);
        }
        // copy to the variable
        timeZone = atoi(str);
      }
    }
    rtn = parseSetting(&fptr, str, size, ':');
  }
}

int parseSetting(File32* fptr, char* str, int size, char delim) {
  char ch;
  int rtn;
  size_t n = 0;
  bool comment = false;
  bool leadingWhitespace = false;
  while (true) {
    // check for EOF
    if (!fptr->available()) {
      rtn = 0;
      break;
    }
    // read next character
    if (fptr->read(&ch, 1) != 1) {
      // read error
      rtn = -1;
      break;
    }
    // Delete CR.
    if (ch == '\r') {
      continue;
    }
    // ignore comment lines
    if (n == 0 && ch == '#') {
      comment = true;
      continue;
    }
    // end of comment line
    if(comment == true && ch == '\n') {
      rtn = '#';
      break;
    }
    // keep ignoring to the end of the line for comments
    if(comment == true) {
      continue; 
    }
    // ignore leading spaces
    if(n == 0 && (ch == ' ' || ch == '\t')) {
      leadingWhitespace = true;
      continue;
    }
    // clip continued white space
    if(leadingWhitespace && (ch == ' ' || ch == '\t')) {
      continue;
    }
    // turn off clipping white space
    if(ch != ' ' && ch != '\t') {
      leadingWhitespace = false;
    }
    // end of parsed word
    if (ch == delim || ch == '\n') {
      rtn = ch;
      break;
    }
    if ((n + 1) >= size) {
      // string too long
      rtn = -2;
      n--;
      break;
    }
    str[n++] = ch;
  }
  str[n] = '\0';
  return(rtn);
}

bool loadCellInit() {
  if (!nau.begin()) {
    Serial.println("Failed to find NAU7802");
    return(false);
  }
  Serial.println("Found NAU7802");

  nau.setLDO(NAU7802_3V0);
  Serial.print("LDO voltage set to ");
  switch (nau.getLDO()) {
    case NAU7802_4V5:  Serial.println("4.5V"); break;
    case NAU7802_4V2:  Serial.println("4.2V"); break;
    case NAU7802_3V9:  Serial.println("3.9V"); break;
    case NAU7802_3V6:  Serial.println("3.6V"); break;
    case NAU7802_3V3:  Serial.println("3.3V"); break;
    case NAU7802_3V0:  Serial.println("3.0V"); break;
    case NAU7802_2V7:  Serial.println("2.7V"); break;
    case NAU7802_2V4:  Serial.println("2.4V"); break;
    case NAU7802_EXTERNAL:  Serial.println("External"); break;
  }

  nau.setGain(NAU7802_GAIN_128);
  Serial.print("Gain set to ");
  switch (nau.getGain()) {
    case NAU7802_GAIN_1:  Serial.println("1x"); break;
    case NAU7802_GAIN_2:  Serial.println("2x"); break;
    case NAU7802_GAIN_4:  Serial.println("4x"); break;
    case NAU7802_GAIN_8:  Serial.println("8x"); break;
    case NAU7802_GAIN_16:  Serial.println("16x"); break;
    case NAU7802_GAIN_32:  Serial.println("32x"); break;
    case NAU7802_GAIN_64:  Serial.println("64x"); break;
    case NAU7802_GAIN_128:  Serial.println("128x"); break;
  }

  nau.setRate(NAU7802_RATE_10SPS);
  Serial.print("Conversion rate set to ");
  switch (nau.getRate()) {
    case NAU7802_RATE_10SPS:  Serial.println("10 SPS"); break;
    case NAU7802_RATE_20SPS:  Serial.println("20 SPS"); break;
    case NAU7802_RATE_40SPS:  Serial.println("40 SPS"); break;
    case NAU7802_RATE_80SPS:  Serial.println("80 SPS"); break;
    case NAU7802_RATE_320SPS:  Serial.println("320 SPS"); break;
  }

  // Take 10 readings to flush out readings
  for (uint8_t i=0; i<10; i++) {
    while (! nau.available()) delay(1);
    nau.read();
  }

  while (! nau.calibrate(NAU7802_CALMOD_INTERNAL)) {
    Serial.println("Failed to calibrate internal offset, retrying!");
    delay(1000);
  }
  Serial.println("Calibrated internal offset");

  while (! nau.calibrate(NAU7802_CALMOD_OFFSET)) {
    Serial.println("Failed to calibrate system offset, retrying!");
    delay(1000);
  }
  Serial.println("Calibrated system offset");
  return(true);
}


bool tempHumidityInit() {
  if (aht.begin()) {
      Serial.println("Found AHT20");
      return(true);
  } else {
      Serial.println("Didn't find AHT20");
      return(false);
  }  
}

String findFname(String name) {
  String fname = name + String(lastFileNum) + ext;
  while(SD.exists(fname)) {
    fname = name + String(++lastFileNum) + ext;
  }
  return(fname);
}

String curTimeStr(DateTime now) {
  //DateTime now = rtc.now();

  String outStr = "";
  outStr += now.month();
  outStr += '/';
  outStr += now.day();
  outStr += '/';
  outStr += now.year();
  outStr += " ";
  outStr += now.hour();
  outStr += ':';
  outStr += now.minute();
  outStr += ':';
  outStr += now.second();
  
  return(outStr);
}

void printCurTime() {
  DateTime now = rtc.now();

  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print('/');
  Serial.print(now.year(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.print(" unix = ");
  Serial.println(now.unixtime());
}

// callback for SD card write times
void dateTime(uint16_t* date, uint16_t* time) {
  DateTime now = rtc.now();  

  *date = FAT_DATE(now.year(), now.month(), now.day());
  *time = FAT_TIME(now.hour(), now.minute(), now.second());
}

void handleForm(WebServer *server, String inputName) {
  String input;
  if (server->hasArg(inputName)) {
    input = server->arg(inputName);
    Serial.println("Received input: " + input);
  }
  // I want to streamline this a bit more. Make it more generic later on
  if (inputName =="rtcValue") {
    rtc.adjust(strToDateTime(input));
    printCurTime();
  } else if (inputName == "wifiSSIDValue") {
    input.toCharArray(ssid, sizeof(ssid));
  } else if (inputName == "wifiPasswordValue") {
    input.toCharArray(password, sizeof(password));
  } else if (inputName == "apSSIDValue") {
    input.toCharArray(apssid, sizeof(apssid));
  } else if (inputName == "apPasswordValue") {
    input.toCharArray(appassword, sizeof(appassword));
  } else if (inputName == "timeZoneValue") {
    timeZone = input.toInt();  // If given invalid input, will default to 0
    syncRTCTime();
  } else {
    Serial.println("Error with inputName");
  }
  updateSettings();
  handleRoot0();  //  Setup to make the page refresh and update with new content when a form is submitted
}

void checkWifi(int retryConnect = 1, int retryInterval = 0) {
  // Checks for WiFi availability and connects to it.
  String outStr = "";
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  if (((currentMillis - previousMillis) >= retryInterval) && (WiFi.status() !=WL_CONNECTED)) {
    previousMillis = currentMillis;
    
    WiFi.begin(ssid, password);
    Serial.print("Attempting to Connect to WiFi access point");
    
    int nWifiCheck = 0;
    
    while ((WiFi.status() != WL_CONNECTED) && (nWifiCheck++ < retryConnect)) {
      delay(500);
      Serial.print(".");
    }
  
    if(WiFi.status() == WL_CONNECTED) {
      // outStr += "\n";
      outStr += "Connected to \"";
      outStr += ssid;
      outStr += "\", IP address: ";
      outStr += WiFi.localIP();
      // outStr += "\n";
      Serial.println(outStr);
      housekeepWrite("$Network",outStr);
      syncRTCTime();
    } else {
      WiFi.disconnect();
      outStr += "Failed to connect to WiFi access point ";
      outStr += ssid;
      Serial.print(outStr);
      housekeepWrite("$Error",outStr);
    }
  }
}

DateTime strToDateTime(String str) {
  if (str.length() != 19) {
    Serial.println("Error: Invalid DateTime String");
    return DateTime(2000, 1, 1, 0, 0, 0);
  }

  // Extract individual components from the string
  int month = str.substring(0, 2).toInt();
  int day = str.substring(3, 5).toInt();
  int year = str.substring(6, 10).toInt();
  int hour = str.substring(11, 13).toInt();
  int minute = str.substring(14, 16).toInt();
  int second = str.substring(17, 19).toInt();

  // Create and return a DateTime object using the extracted components
  return DateTime(year, month, day, hour, minute, second);
}

void updateSettings() {
  File32 settingsFile = SD.open("settings.txt", O_WRITE);
  if (settingsFile) {
    settingsFile.print("# This is a comment that starts with a # sign as first character\napssid: ");
    settingsFile.println(apssid);
    settingsFile.print("appass: ");
    settingsFile.println(appassword);
    settingsFile.print("wifissid: ");
    settingsFile.println(ssid);
    settingsFile.print("wifipass: ");
    settingsFile.println(password);
    settingsFile.print("timezone: ");
    settingsFile.println(timeZone);
    settingsFile.close();
  } else {
    Serial.println("Error: Failed to update settings.txt");
  }
}

void syncRTCTime() {
  String outStr = "Old RTC Time: ";
  if (WiFi.status() == WL_CONNECTED) {
    // Configure NTP
    configTime(timeZone * 3600, 0, "pool.ntp.org", "time.nist.gov");  //Add Variables to configure time zones and daylight savings offset

    // Wait for time to be set
    struct tm time0;
    if (getLocalTime(&time0)) {
      DateTime time1(time0.tm_year + 1900, time0.tm_mon + 1, time0.tm_mday, time0.tm_hour, time0.tm_min, time0.tm_sec);

      outStr += curTimeStr(rtc.now());
      outStr += ", New RTC Time: ";
      outStr += String(time1.month()) + "/" + String(time1.day()) + "/" + String(time1.year()) + " " + String(time1.hour()) + ":" + String(time1.minute()) + ":" + String(time1.second()) + "(UTC)";
      housekeepWrite("$Time",outStr);
      
      rtc.adjust(time1);
      Serial.print("Sync RTC time success: ");
      Serial.println(String(time1.month()) + "/" + String(time1.day()) + "/" + String(time1.year()) + " " + String(time1.hour()) + ":" + String(time1.minute()) + ":" + String(time1.second()) + " (UTC)");
    } else {
      Serial.println("Error: Sync RTC time failed. Unable to obtain time from NTP server.");
      housekeepWrite("$Error","Error: Sync RTC time failed. Unable to obtain time from NTP server.");
    }
  } else {
    Serial.println("Error: Sync RTC time failed. Not Connected to WiFi.");
    housekeepWrite("$Error","Error: Sync RTC time failed. Not Connected to WiFi.");
  }
}

void handleRTCTime() {
  String serverTime = curTimeStr(rtc.now());
  serverTime += ".";
  serverTime += String(millis()%1000);
  server0->send(200, "text/plain", serverTime);
}

void INA219Setup() {
  Serial.println("Starting INA219...");
  
  // Initialize the INA219.
  // By default the initialization will use the largest range (32V, 2A).  However
  // you can call a setCalibration function to change this range (see comments).
  if (!solar1.begin()) {
    Serial.println("Failed to find solar1 INA219 chip");
    while (1) { delay(10); }
  }
  if (!solar2.begin()) {
    Serial.println("Failed to find solar2 INA219 chip");
    while (1) { delay(10); }
  }
  // To use a slightly lower 32V, 1A range (higher precision on amps):
  //ina219.setCalibration_32V_1A();
  // Or to use a lower 16V, 400mA range (higher precision on volts and amps):
  //ina219.setCalibration_16V_400mA();

  Serial.println("Measuring voltage and current with INA219 ...");
}

void PCA9536BuzzSetup()
{
  Serial.println("PCA9536 starting...");

  Wire.begin();

  // Initialize the PCA9536 with a begin functbuzzLEDn
  if (buzzLED.begin() == false)
  {
    Serial.println("PCA9536 not detected. Please check wiring. Freezing...");
    while (1)
      ;
  }

  // set all I/O as outputs
  for (int i = 0; i < 4; i++)
  {
    // pinMode can be used to set an I/O as OUTPUT or INPUT
    buzzLED.pinMode(i, OUTPUT);
  }
}

void sequenceBuzzLED(int iterations, int blinkDuration, int color) {
  for (int i = 0; i < iterations; i++) {
    buzzLED.write(color, LOW);
    buzzLED.write(BUZZ, HIGH);
    delay(blinkDuration);
    buzzLED.write(color, HIGH);
    buzzLED.write(BUZZ, LOW);
    delay(blinkDuration);
  }
}

void readBatteryInfo() {
  float shuntvoltage1 = 0;
  float busvoltage1 = 0;
  float current_mA1 = 0;
  float loadvoltage1 = 0;
  float power_mW1 = 0;

  float shuntvoltage2 = 0;
  float busvoltage2 = 0;
  float current_mA2 = 0;
  float loadvoltage2 = 0;
  float power_mW2 = 0;
  String outStr = "";

  shuntvoltage1 = solar1.getShuntVoltage_mV();
  busvoltage1 = solar1.getBusVoltage_V();
  current_mA1 = solar1.getCurrent_mA();
  power_mW1 = solar1.getPower_mW();
  loadvoltage1 = busvoltage1 + (shuntvoltage1 / 1000);
  
  shuntvoltage2 = solar2.getShuntVoltage_mV();
  busvoltage2 = solar2.getBusVoltage_V();
  current_mA2 = solar2.getCurrent_mA();
  power_mW2 = solar2.getPower_mW();
  loadvoltage2 = busvoltage2 + (shuntvoltage2 / 1000);
  
  // Serial.print("Bus Voltages:   "); Serial.print(busvoltage1); Serial.print(" V, "); Serial.print(busvoltage2); Serial.println(" V");
  // Serial.print("Shunt Voltages: "); Serial.print(shuntvoltage1); Serial.print(" mV, "); Serial.print(shuntvoltage2); Serial.println(" mV");
  // Serial.print("Load Voltages:  "); Serial.print(loadvoltage1); Serial.print(" V, "); Serial.print(loadvoltage2); Serial.println(" V");
  // Serial.print("Currents:       "); Serial.print(current_mA1); Serial.print(" mA, "); Serial.print(current_mA2); Serial.println(" mA");
  // Serial.print("Powers:         "); Serial.print(power_mW1); Serial.print(" mW, "); Serial.print(power_mW2); Serial.println(" mW");
  // Serial.println("");
  // out = "Bus Voltages:   "; out += busvoltage1;out += " V, ";out += busvoltage2; out +=" V\n";
  // out += "Shunt Voltages: "; out += shuntvoltage1;out += " mV, ";out += shuntvoltage2; out +=" mV\n";
  // out += "Load Voltages:  "; out += loadvoltage1;out += " V, ";out += loadvoltage2; out +=" V\n";
  // out += "Currents:       "; out += current_mA1;out += " mA, ";out += current_mA2; out +=" mA\n";
  outStr += "Powers:         "; outStr += power_mW1;outStr += " mW, ";outStr += power_mW2; outStr +=" mW";
  housekeepWrite("$Power",outStr);
}

void loadRFIDWrite() {
  static unsigned long lastCharTime = 0; // millis of last character
  static unsigned long tagReadWaitTime = 300;   // ms to wait before newline for RFID tag read finish
  static unsigned long lastTagReadTime = 0;  // millis of last tag read
  unsigned long curMillis = millis();
  static bool newChars = false;  // true if new chars since last newline
  static String tagNum = "";
  static bfs::CircleBuf<loadPoint, bufLen> loadcellBuf;  // circular buffer for loadcell data

  // RFID reader input on Serial1
  if (Serial1.available()) {       // If anything comes in Serial1 (pins 0 & 1)
    digitalWrite(led, HIGH);
    lastCharTime = millis();
    newChars = true;
    byte inChar = Serial1.read();
    tagNum += String(inChar, HEX);
    //Serial.print(inChar, HEX);  // read it and send it out Serial (USB)
  }

  if(nau.available()) {
    int32_t val = nau.read();
    if((curMillis - lastTagReadTime) < afterReadRecordTime) {
      myFileLoadCell = SD.open(fname, O_WRITE | O_APPEND); // vs FILE_WRITE
      if (myFileLoadCell) {
        Serial.print(val);
        Serial.print(" \t\t ");
        // close the file:
        myFileLoadCell.println(curTimeStr(rtc.now()) + "," + String(curMillis) + "," + String(val) + ",");
        myFileLoadCell.close();
        Serial.print(curTimeStr(rtc.now()));
        Serial.print("\t\t");
        Serial.println(curMillis - lastTagReadTime);
        // Serial.println();
      } else {
        // if the file didn't open, print an error:
        Serial.println("error opening " + fname);
      }
    } else {  // not already saving to the SD card so save in buffer
      loadPoint popVal;  // value removed from buffer if full
      if(loadcellBuf.size() >= bufLen) {  // buffer full, need to remove one
        unsigned int nRead = loadcellBuf.Read(&popVal);
        if(nRead == 0) {
          Serial.println("!!!! Buffer read fail!!!!");
        }
      }
      loadPoint curPoint;
      curPoint.load = val;
      curPoint.millis = curMillis;
      curPoint.t = rtc.now();
      unsigned int nWrite = loadcellBuf.Write(curPoint); // add value to buffer
      if(nWrite < 1) {
        Serial.println("Failed to write data to buffer");
      }
      // Serial.print("BufSize: ");
      // Serial.print(loadcellBuf.size());
      // Serial.println();
    }
  }
  if((curMillis - lastCharTime) > tagReadWaitTime && newChars) {
    newChars = false;
    //Serial.println("");
    myFileLoadCell = SD.open(fname, O_WRITE | O_APPEND); // vs FILE_WRITE
    myFileLoadCell.print("");  //  I really can't tell you why this is needed to work
    // if the file opened okay, write to it:
    if (myFileLoadCell) {
      // write buffer values
      loadPoint bufVal;
      int cnt = 0;
      uint32_t curBufLen = loadcellBuf.size();
      Serial.println("Starting buffer save with size " + String(curBufLen));
      for(size_t ii = 0; ii < curBufLen; ii++) {
        uint32_t readNum = loadcellBuf.Read(&bufVal);
        if(readNum < 1) {
          Serial.println("buffer read fail");
        }
        Serial.println("saving buffer: " + String(cnt++));
        myFileLoadCell.println(curTimeStr(bufVal.t) + "," + String(bufVal.millis) + "," + String(bufVal.load) + ",");
      }
      Serial.print("Writing '" + tagNum + "' to " + fname + "...");
      // close the file:
      myFileLoadCell.println(curTimeStr(rtc.now()) + "," + String(curMillis) + ",," + tagNum);
      myFileLoadCell.close();
      Serial.println("done.");
      lastTagReadTime = curMillis; // record last time a tag was read
      digitalWrite(led, LOW);
  } else {
      // if the file didn't open, print an error:
      Serial.println("error opening " + fname);
    }
    tagNum = ""; // reset tagNum
  }
}

void housekeepWrite(String sentenceID, String data) {
  if(!data.isEmpty()) {
    myFileHousekeeping = SD.open(fname2, O_WRITE | O_APPEND);
    if(myFileHousekeeping) {
      myFileHousekeeping.print(sentenceID);
      myFileHousekeeping.print(",");
      myFileHousekeeping.print(data);
      myFileHousekeeping.print(",");
      myFileHousekeeping.print(curTimeStr(rtc.now()));
      myFileHousekeeping.println("");
      myFileHousekeeping.close();
    } else {
      Serial.println("Failed to open housekeeping file");
    }
  }
}

void setup(void) {
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  pinMode(I2C_PWR_PIN, OUTPUT);
  digitalWrite(I2C_PWR_PIN, HIGH);  // make sure I2C power is on
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  Serial1.begin(9600);  // serial port for RFID data
  delay(4000);  // wait for serial ports to start if they exist

  Serial.println("Bird feeder Wifi starting");
  delay(1000);

  // RTC initialization
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
  } else {
    rtc.start();
  }

  loadCellInit();
  tempHumidityInit();

  SdFile::dateTimeCallback(dateTime);  
  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("initialization failed!");
  } else {
    Serial.println("initialization done.");
  }
  loadSettings("/settings.txt");  // load settings from SD card

  //Print File headings
  fname = findFname(baseName);
  Serial.println("Filename: " + fname);
  myFileLoadCell = SD.open(fname, O_WRITE | O_CREAT);
  if(myFileLoadCell) {
    myFileLoadCell.println("datetime,time(ms),LoadCell,RFIDTag");
    myFileLoadCell.close();
  } else {
    Serial.println("Failed to open file");
  }

  fname2 = findFname("/log");
  Serial.println("Filename: " + fname2);
  myFileHousekeeping = SD.open(fname2, O_WRITE | O_CREAT);
  if(myFileHousekeeping) {
    myFileHousekeeping.println("$sentenceID, Data, Date and Time");
    myFileHousekeeping.close();
  } else {
    Serial.println("Failed to open file");
  }

  WiFi.mode(WIFI_AP_STA);
  checkWifi(wifiMaxCheckTimes);
  
  if (!WiFi.softAP(apssid, appassword)) {
    Serial.println("failed to start softAP");
    for (;;) {
        digitalWrite(led, 1);
        delay(100);
        digitalWrite(led, 0);
        delay(100);
    }
  }
  Serial.print("Soft AP SSID: \"");
  Serial.print(apssid);
  Serial.print("\", IP address: ");
  Serial.println(WiFi.softAPIP());

  if (MDNS.begin(apssid)) {
    Serial.println("MDNS responder started");
  }

  server0 = new WebServer(80);
  server1 = new WebServer(WiFi.localIP(), 8081);
  server2 = new WebServer(WiFi.softAPIP(), 8081);

  server0->on("/", handleRoot0);
  server1->on("/", handleRoot1);
  server2->on("/", handleRoot2);
  
  server0->on("/fileex", handleFileExample0);
  server0->on("/download", handleDownload0);
  server0->on("/rtcTime", HTTP_GET, handleRTCTime);

  // Handle inputs
  server0->on("/rtcForm", HTTP_GET, []() { handleForm(server0, "rtcValue"); });
  server0->on("/timeZoneForm", HTTP_GET, []() { handleForm(server0, "timeZoneValue"); });
  server0->on("/wifiSSIDForm", HTTP_GET, []() { handleForm(server0, "wifiSSIDValue"); });
  server0->on("/wifiPasswordForm", HTTP_GET, []() { handleForm(server0, "wifiPasswordValue"); });
  server0->on("/apSSIDForm", HTTP_GET, []() { handleForm(server0, "apSSIDValue"); });
  server0->on("/apPasswordForm", HTTP_GET, []() { handleForm(server0, "apPasswordValue"); });
  server0->on("/restart", HTTP_POST, []() {
    handleRoot(server0, "Restarting ESP32-S3");
    ESP.restart();
  });

  server0->onNotFound(handleNotFound0);
  server1->onNotFound(handleNotFound1);
  server2->onNotFound(handleNotFound2);

  server0->begin();
  Serial.println("HTTP server0 started");
  server1->begin();
  Serial.println("HTTP server1 started");
  server2->begin();
  Serial.println("HTTP server2 started");

  Serial.printf("SSID: %s\n\thttp://", ssid); Serial.print(WiFi.localIP()); Serial.print(":80\n\thttp://"); Serial.print(WiFi.localIP()); Serial.println(":8081");
  Serial.printf("SSID: %s\n\thttp://", apssid); Serial.print(WiFi.softAPIP()); Serial.print(":80\n\thttp://"); Serial.print(WiFi.softAPIP()); Serial.println(":8081");
  Serial.printf("Any of the above SSIDs\n\thttp://");Serial.print(apssid);Serial.print(".local:80\n\thttp://");Serial.print(apssid);Serial.print(".local:8081\n");
}

void loop(void) {
  // Handle Webserver
  server0->handleClient();
  server1->handleClient();
  server2->handleClient();
  delay(2);//allow the cpu to switch to other tasks

  checkWifi(wifiMaxCheckTimes/2, 10000); //Just cause :3

  sequenceBuzzLED(3,200,RED);
  readBatteryInfo();

  loadRFIDWrite();

}
