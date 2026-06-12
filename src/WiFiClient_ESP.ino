#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_I2CDevice.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Adafruit_ILI9341esp.h>
#include <PubSubClient.h>
#include <netatmo_icons.h>
#include "secrets.h"

const char* ssid     = SECRET_WIFI_SSID;
const char* password = SECRET_WIFI_PASSWORD;
const char* mqtt_server = SECRET_MQTT_SERVER;

#define TFT_DC 2
#define TFT_CS -1
#define sleepmillis 10000
#define HC595 
#define DEBUG
// TFT Rotation: 0=Pinheader on bottom, 2=Pinheader on top
#define TFTROT 2 

#ifndef HC595
#define button1 15
#define button2 0
#define button3 4
#define button4 5
#define button5 16
#define multi 10
#endif

#ifdef HC595
#define DS 15
#define SRCLK 4
#define RCLK 5
#define SENSE 16
#endif

#ifndef HC595
bool b1d, b2d, b3d, b4d, b5d, b6d, b7d, b8d, b9d, b10d = false;
#endif

bool sleep = false;

#ifdef HC595
bool bd[11] = {false,false,false,false,false,false,false,false,false,false,false};
  #if TFTROT == 0
  // Pinheader on bottom
  int btnmap[16] ={0,1,3,5,7,9,0,0,0,2,4,6,8,10,0,0};
  #else
  // Pinheader on top
  int btnmap[16] ={0,10,8,6,4,2,0,0,0,9,7,5,3,1,0,0};
  #endif
#endif

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
WiFiClient espClient;
PubSubClient client(espClient);

char msg[200];
char espID[20];
int num = 0;
bool configured = false;
long delaym = 0;
long smillis = 0;
char tempin[20] = "0.0";
char humin[20] = "100";
char pressin[20] = "1000.0";
char noisein[20] = "00";
char coin[20] = "000";
char tempout[20] = "0.0";
char humout[20] = "100";
char daytime[20] = "00:00";
char daydate[20] = "Mon,01.01.1900";
char humavo[20] = "100";

struct Member {
  char name1[20];
  char name2[20];
  char type[10];
  char topic[50];
  char txtl[10];
  char txtr[10];
  char cmdl[10];
  char cmdr[10];
  char statetopic[50];
  char state[10];
} member;

struct Member Screen[10][5];
char scrname[10][20];
int id = ESP.getChipId();

unsigned int nScreens = 0;
unsigned int confstage;

void setup() {
  configured = false;
  confstage = 0;
  snprintf(espID,20,"esp%i", id);

  #ifdef DEBUG
    Serial.begin(115200);
    Serial.println();
    Serial.print("Begin...");
    Serial.println(espID);
  #endif
  
  delay(10);
  
  #ifndef HC595
  pinMode(button1, INPUT);
  pinMode(button2, INPUT_PULLUP); //GPIO0 ist LOW Aktiv!
  pinMode(button3, INPUT);
  pinMode(button4, INPUT);
  pinMode(button5, INPUT);
  pinMode(multi, INPUT);
  digitalWrite(button2, LOW);
  #endif

  #ifdef HC595
  pinMode(DS, OUTPUT);
  pinMode(SRCLK, OUTPUT);
  pinMode(RCLK, OUTPUT);
  pinMode(SENSE, INPUT_PULLDOWN_16);
  
  #endif

  tft.begin();
  SPI.setFrequency(ESP_SPI_FREQ);
  tft.fillScreen(ILI9341_BLACK);  
  tft.setTextColor(ILI9341_WHITE);
  //tft.setTextSize(1);
  tft.setRotation(TFTROT); 
  tft.setFont();
  //tft.setFont(&FreeSans9pt7b);

  // We start by connecting to a WiFi network
   #ifdef DEBUG
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
  #endif

  tft.print("Connecting to ");
  tft.print(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
      #ifdef DEBUG
        Serial.print(".");
      #endif
    tft.print(".");
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
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
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  
  ArduinoOTA.begin();
  
  #ifdef DEBUG
    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  #endif

  //tft.println("");
  tft.println("WiFi connected");  
  tft.print("IP address: ");
  tft.println(WiFi.localIP());
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void callback(char* topic, byte* payload, unsigned int length) {
  char spayload[length + 1];
  memcpy(spayload, payload, length);
  spayload[length] = '\0';
  char topicfilter[50] = "";
  
   #ifdef DEBUG
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
  #endif
  
  if (strcmp(topic,"/openhab/out/Netatmo_Temp_Indoor/state") == 0) {
    strcpy(tempin, spayload);
  }

  if (strcmp(topic,"/openhab/out/Netatmo_Hum_Indoor/state") == 0) {
    strcpy(humin, spayload);
  }

  if (strcmp(topic,"/openhab/out/Netatmo_Press_Indoor/state") == 0) {
    strcpy(pressin, spayload);
  }

  if (strcmp(topic,"/openhab/out/Netatmo_CO2_Indoor/state") == 0) {
    strcpy(coin, spayload);
  }

  if (strcmp(topic,"/openhab/out/Netatmo_Noise_Indoor/state") == 0) {
    strcpy(noisein, spayload);
  }

  //if (strcmp(topic,"/openhab/out/Netatmo_Temp_Outdoor/state") == 0) {
  if (strcmp(topic,"/openhab/out/731e000008ea_temp/state") == 0) {
    strcpy(tempout, spayload);
  }

  //if (strcmp(topic,"/openhab/out/Netatmo_Hum_Outdoor/state") == 0) {
  if (strcmp(topic,"/openhab/out/731e000008ea_hum/state") == 0) {
    strcpy(humout, spayload);
  }

  if (strcmp(topic,"/openhab/out/AvocadoWZ_Perc/state") == 0 ) {
    strcpy(humavo, spayload);
  }

  if (strcmp(topic,"/openhab/DayDate") == 0) {
    strcpy(daydate, spayload);
  }

  if (strcmp(topic,"/openhab/Daytime") == 0) {
    strcpy(daytime, spayload);
    if (sleep) { paintSleep(); }
  }

  if (confstage == 4) {
    for (int i=0; i <= nScreens; i++) {
      for (int j=1; j <= 4; j++) {
        //Serial.println(Screen[i][j].statetopic);
        if (strcmp(topic, Screen[i][j].statetopic) == 0) {
          if (strcmp(Screen[i][j].state, spayload) != 0) {
            #ifdef DEBUG
            Serial.println("Found Update on StateTopic");
            #endif   
            strcpy(Screen[i][j].state, spayload);
            if ( i == num) {
              if (!sleep) {
                paintScreen();
              } else {
                delaym = 0;
              }
            } 
          }
        }
      }
    }
  }

  snprintf(topicfilter,50,"/openhab/configuration/%i",id);
    if (strcmp(topic,topicfilter) == 0) {
    getConfiguration(spayload);
  }
  
}

void reconnect() {
  // Loop until we're reconnected
  tft.fillScreen(ILI9341_BLACK);  
  tft.setTextColor(ILI9341_WHITE);
  while (!client.connected()) {
    tft.print("Attempting MQTT connection...");
    #ifdef DEBUG
      Serial.print("Attempting MQTT connection...");
    #endif
    // Attempt to connect
    if (client.connect(espID)) {
      tft.println("connected");
      #ifdef DEBUG
        Serial.println("connected");
      #endif
      // Once connected, publish an announcement...
      snprintf(msg,50,"Startup %i", id);
      client.publish("/openhab/esp8266", msg);
      #ifdef DEBUG
        Serial.print("Publish Announcement: ");
        Serial.println(msg);
      #endif

      // ... and resubscribe
      #ifdef DEBUG
        Serial.println("Resubscribe...");
      #endif
      client.subscribe("/openhab/configuration");
      client.subscribe("/openhab/configuration/#");
      client.subscribe("/openhab/out/Netatmo_Temp_Indoor/state");
      client.subscribe("/openhab/out/Netatmo_Hum_Indoor/state");
      client.subscribe("/openhab/out/Netatmo_Press_Indoor/state");
      client.subscribe("/openhab/out/Netatmo_CO2_Indoor/state");
      client.subscribe("/openhab/out/Netatmo_Noise_Indoor/state");
      client.subscribe("/openhab/out/Netatmo_Temp_Outdoor/state");
      client.subscribe("/openhab/out/731e000008ea_temp/state");
      client.subscribe("/openhab/out/Netatmo_Hum_Outdoor/state");
      client.subscribe("/openhab/out/731e000008ea_hum/state");
      client.subscribe("/openhab/out/AvocadoWZ_Perc/state");
      client.subscribe("/openhab/DayDate");
      client.subscribe("/openhab/Daytime");

      if ( configured && nScreens > 0 ) {
        for (int i=0; i <= nScreens; i++) {
          for (int j=1; j <= 4; j++) {
            if (strlen(Screen[i][j].statetopic) > 0) {
              client.subscribe(Screen[i][j].statetopic);
              #ifdef DEBUG
                Serial.print("Subscribe: ");
                Serial.println(Screen[i][j].statetopic);
              #endif
            }
          }
        }
      }
      
    } else {
      tft.print("failed, rc=");
      tft.print(client.state());
      #ifdef DEBUG
        Serial.print("failed, rc=");
        Serial.println(client.state());
      #endif

      tft.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  delaym = 0;
}

#ifdef HC595
void btnLoop() {
  int bnum = 0;
  int scan = 0;
  int loopd = 1; // Set to 1 or 2 for button sweep, 5 for LED sweep

  // Scan all the IO-Pins on the 74HC595
  for(scan = 0; scan <= 15; scan++) {
    
    digitalWrite(SRCLK, LOW);
    
    //Serial.print("Button set ");
    //Serial.print(i);
    //Serial.println();
    
    if (scan == 0) {
      digitalWrite(DS, HIGH);
    } else {
      digitalWrite(DS, LOW);
    }
    delay(loopd);

    //Serial.println("Latch SRCLK");
    digitalWrite(SRCLK, HIGH);
    delay(loopd);
    digitalWrite(SRCLK, LOW);
    delay(loopd);

    //Serial.println("Latch RCLK");
    digitalWrite(RCLK, HIGH);
    delay(loopd);
    digitalWrite(RCLK, LOW);
    delay(loopd);

    // Get button nr connected to the scanned IO-Pin
    bnum = btnmap[scan];

    if (bnum > 0) { // Only check btndwn if a button is connected to the IO-Pin

      // Check Button pressed and debounce
      if (digitalRead(SENSE) == HIGH && bd[bnum] == false) {
        Serial.print("Button pressed: ");
        Serial.print(bnum);
        Serial.println();
        btnDownCallback(bnum);
        bd[bnum] = true;
      }

      if (digitalRead(SENSE) == LOW && bd[bnum] == true ) {
        bd[bnum] = false;
      }
    }
    
    delay(loopd);

  }
}
#endif

#ifndef HC595
void btnLoop() {
  if (digitalRead(button1) == HIGH && digitalRead(multi) == LOW && b2d == false) {
    btnDownCallback(2);
    b2d == true; 
  }

  if (digitalRead(button1) == LOW && b2d == true) {
    b2d = false;
  }

  if (digitalRead(button1) == HIGH && digitalRead(multi) == HIGH && b1d == false) {
    btnDownCallback(1);
    b1d == true; 
  }

  if (digitalRead(button1) == LOW && digitalRead(multi) == LOW && b1d == true) {
    b1d = false;
  }

  // GPIO0 ist LOW Aktiv!
  if (digitalRead(button2) == LOW && digitalRead(multi) == LOW && b4d == false) {
    btnDownCallback(4);
    b4d == true; 
  }

  if (digitalRead(button2) == HIGH && b4d == true) {
    b4d = false;
  }

  if (digitalRead(button2) == LOW && digitalRead(multi) == HIGH && b3d == false) {
    btnDownCallback(3);
    b3d == true; 
  }

  if (digitalRead(button2) == HIGH && digitalRead(multi) == LOW && b2d == true) {
    b3d = false;
  } 

  if (digitalRead(button3) == HIGH && digitalRead(multi) == LOW && b6d == false) {
    btnDownCallback(6);
    b6d == true; 
  }

  if (digitalRead(button3) == LOW && b6d == true) {
    b6d = false;
  }

  if (digitalRead(button3) == HIGH && digitalRead(multi) == HIGH && b5d == false) {
    btnDownCallback(5);
    b5d == true; 
  }

  if (digitalRead(button3) == LOW && digitalRead(multi) == LOW && b5d == true) {
    b5d = false;
  }
  
  if (digitalRead(button4) == HIGH && digitalRead(multi) == LOW && b8d == false) {
    btnDownCallback(8);
    b8d == true; 
  }

  if (digitalRead(button4) == LOW && b8d == true) {
    b8d = false;
  }

  if (digitalRead(button4) == HIGH && digitalRead(multi) == HIGH && b7d == false) {
    btnDownCallback(7);
    b7d == true; 
  }

  if (digitalRead(button4) == LOW && digitalRead(multi) == LOW && b7d == true) {
    b7d = false;
  }

  if (digitalRead(button5) == HIGH && digitalRead(multi) == LOW && b10d == false) {
    btnDownCallback(10);
    b9d == true; 
  }

  if (digitalRead(button5) == LOW && b10d == true) {
    b10d = false;
  }

  if (digitalRead(button5) == HIGH && digitalRead(multi) == HIGH && b9d == false) {
    btnDownCallback(9);
    b9d == true; 
  }

  if (digitalRead(button5) == LOW && digitalRead(multi) == LOW && b9d == true) {
    b9d = false;
  }
}
#endif

void btnDownCallback(unsigned int btn) {
    char cmd[10]  = "";
    char topic[50] = "";
    int i  = 0;
    i = int((btn - .5) / 2);
    
    if (!sleep) {
      if ( i > 0 ) {
        #ifdef DEBUG
          Serial.print("Executing Command for Button ");
          Serial.print(btn);
          Serial.print(" Member=");
          Serial.println(i);
        #endif
       
        if (btn == 1 || btn == 3 || btn == 5 || btn == 7 || btn == 9) {
          strcpy(cmd, Screen[num][i].cmdl);
        } else {
          strcpy(cmd, Screen[num][i].cmdr);
        }
        
        strcpy(topic, Screen[num][i].topic);
            
        #ifdef DEBUG
          snprintf (msg, 75, "%s %s", topic, cmd);
          Serial.print("Publish message: ");
          Serial.println(msg);
        #endif
        client.publish(topic, cmd, true);
      } else {
        if (btn == 2) { num++; }
        if (btn == 1) { num--; }
        if (num < 0) { num = nScreens; }
        if (num > nScreens) { num = 0; }
        paintScreen();
      }
    }
    delaym = 0;
    delay(200);
}

void getConfiguration(char* cmd) {
  char delimiter[] = ":";
  char *ptr;
  char temp[100] = "";

  tft.println(cmd);
  
  if ( strcmp(cmd,"initialize") == 0 ) {
    tft.println("Getting Configuration...");
    snprintf(msg,50,"getconfig:%i", id);
    client.publish("/openhab/configuration",msg, true);
    confstage = 1;
    goto finish;
  }

  if ( strcmp(cmd,"restart") == 0 ) {
    ESP.restart();
  }

  if ( strcmp(cmd,"reconfigure") == 0 ) {
    configured = false;
    confstage = 0;    
    tft.fillScreen(ILI9341_BLACK);  
    tft.setTextColor(ILI9341_WHITE);
    tft.setFont();
    tft.setCursor(0,0);
    goto finish;
  }

  if ( confstage == 1 ) {
    strcpy(msg,cmd);
    ptr = strtok(msg, delimiter);
    if (ptr != NULL) {
      if (strcmp(ptr,"Screens") == 0) {
        ptr = strtok(NULL, delimiter);
        confstage = 2;
        nScreens = atoi(ptr) - 1;
        num = 0;
        goto finish;
      }
    }
  }

  if ( confstage == 2 ) {
    strcpy(msg,cmd);
    ptr = strtok(msg, delimiter);
    if (ptr != NULL) {
      if (strcmp(ptr,"StartScreen") == 0) {
        ptr = strtok(NULL, delimiter);
        strcpy(scrname[num],ptr);
        confstage = 3;
        goto finish;
      }
      if (strcmp(ptr,"EndConfig") == 0) {
        confstage = 4;
        goto finish;  
      }
    }
  }

  if ( confstage == 3 ) {
    strcpy(msg,cmd);
    ptr = strtok(msg, delimiter);
    if (ptr != NULL) {
      if (strcmp(ptr,"EndScreen") == 0) {
        num++;
        confstage = 2;
        goto finish;
      }
      if (strcmp(ptr,"Member") == 0) {
        ptr = strtok(NULL, delimiter);
        int member = atoi(ptr);      
        ptr = strtok(NULL, delimiter);
        strcpy(Screen[num][member].name1, ptr);
        ptr = strtok(NULL, delimiter);
        strcpy(Screen[num][member].name2, ptr);
        ptr = strtok(NULL, delimiter);
        strcpy(Screen[num][member].type, ptr);
        ptr = strtok(NULL, delimiter);
        strcpy(Screen[num][member].topic, ptr);
        ptr = strtok(NULL, delimiter);
        strcpy(Screen[num][member].txtl, ptr);
        ptr = strtok(NULL, delimiter);
        strcpy(Screen[num][member].txtr, ptr);
        ptr = strtok(NULL, delimiter);
        strcpy(Screen[num][member].cmdl, ptr);
        ptr = strtok(NULL, delimiter);
        strcpy(Screen[num][member].cmdr, ptr); 
        ptr = strtok(NULL, delimiter);
        strcpy(Screen[num][member].statetopic, ptr);
        client.subscribe(Screen[num][member].statetopic);
         #ifdef DEBUG
          Serial.print("Subscribe to StateTopic: ");
          Serial.println(Screen[num][member].statetopic);
        #endif         
      }  
    }
  }
  finish:;
}

void paintScreen() {
  int xpos;
  char* text;
  int ypos;
  int cw = 9;
  char scr[20];
  int16_t x1, y1; 
  uint16_t w, h;

  
  tft.fillScreen(ILI9341_BLACK);  
  tft.setTextColor(ILI9341_WHITE);
  //tft.setTextSize(1);
  tft.setRotation(TFTROT);
  tft.setFont();
  cw = 6;
  uint16_t color;

  tft.setTextColor(ILI9341_WHITE);
  snprintf(scr,20,"%s (%u/%u)", scrname[num], num + 1, nScreens + 1);
  xpos = ((240 - strlen(scr) * cw) / 2);
  tft.setCursor(xpos, 6);
  tft.print(scr);

  tft.fillTriangle(231, 14, 231, 4, 238, 9, ILI9341_WHITE);
  tft.fillTriangle(9, 14, 9, 4, 2, 9, ILI9341_WHITE);

  tft.setFont(&FreeSans9pt7b);
  cw = 9;
  
  for (unsigned int i = 1; i<=4; i++ ) {
      
    if ( strcmp(Screen[num][i].name1,"") != 0) {
      ypos = (i-1) * 83 + 15 + 35;
      text = Screen[num][i].txtl;
      color = getColor(text);
      tft.getTextBounds(text, 6, ypos, &x1, &y1, &w, &h);

      if (strcmp(Screen[num][i].state,Screen[num][i].cmdl) == 0) {
        tft.fillRoundRect(x1 - 4, y1 - 4 , w + 8, h + 8, 2, color);
        tft.setTextColor(ILI9341_WHITE);
      } else {
        tft.drawRoundRect(x1 - 4, y1 - 4 , w + 8, h + 8, 2, color);
        tft.setTextColor(color);
      }

      tft.setCursor(6, ypos);
      tft.print(text);

      tft.setTextColor(ILI9341_WHITE);
      text = Screen[num][i].name1;
      tft.getTextBounds(text, 10,10, &x1, &y1, &w, &h);
      xpos = ((240 - w) / 2);
      tft.setCursor(xpos, ypos - 9);
      tft.print(text);
   
      text = Screen[num][i].name2;
      tft.getTextBounds(text, 10,10, &x1, &y1, &w, &h);
      xpos = ((240 - w) / 2);
      tft.setCursor(xpos, ypos + 9);
      tft.print(text);
  
      text = Screen[num][i].txtr;
      color = getColor(text);
      tft.getTextBounds(text, 10,10, &x1, &y1, &w, &h);
      xpos = (240 - w - 6);

      if (strcmp(Screen[num][i].state,Screen[num][i].cmdr) == 0) {
        tft.fillRoundRect(xpos - 4, ypos - 16, w + 8, h + 8, 2, color);
        tft.setTextColor(ILI9341_WHITE);
      } else {
        tft.drawRoundRect(xpos - 4, ypos - 16, w + 8, h + 8, 2, color);
        tft.setTextColor(color);
      }
      
      tft.setCursor(xpos, ypos);
      tft.print(text);
    }
  }
}

uint16_t getColor(char* text) {
  uint16_t color;
  color = ILI9341_YELLOW;
  
  if ( strcmp(text,"Ein") == 0 ) {
    color = ILI9341_GREEN;
  }

  if ( strcmp(text,"Aus") == 0 ) {
    color = ILI9341_RED;
  }
  return color;
}

void paintSleep() {
  int xpos;
  int cw = 18;
  char scr[20] ="00:00";
  char* buf;
  int tmp;
  
  tft.fillScreen(ILI9341_BLACK);  
  tft.setTextColor(ILI9341_WHITE);
  //tft.setTextSize(1);
  tft.setRotation(TFTROT);
  tft.setFont(&FreeSans18pt7b);

  tft.setTextColor(ILI9341_WHITE);
  strcpy(scr, daytime);
  xpos = ((240 - strlen(daytime) * cw) / 2);
  tft.setCursor(xpos, 40);
  tft.print(daytime);

  tft.setFont(&FreeSans9pt7b);
  cw = 9;
  tft.setTextColor(ILI9341_WHITE);
  strcpy(scr, daydate);
  xpos = ((240 - strlen(scr) * cw) / 2);
  tft.setCursor(xpos, 62);
  tft.print(scr);

  tft.setFont();
  cw = 6;
  tft.setCursor(10, 110);
  tft.print("Netatmo Innen");
  //tft.drawRoundRect(2, 105, 236, 100, 2, ILI9341_WHITE);

  tft.setCursor(10, 200);
  tft.print("Avocado WZ");
  //tft.drawRoundRect(2, 230, 236, 80, 2, ILI9341_WHITE);

  tft.setCursor(10, 235);
  tft.print("Netatmo Aussen");
 
  tft.setFont(&FreeSans18pt7b);
  cw = 18;
  tft.setTextColor(ILI9341_GREEN);
  snprintf(scr,20,"%s C", tempin);
  xpos = (230 - strlen(scr) * cw);
  tft.setCursor(xpos, 140);
  tft.print(scr);

  tft.setFont(&FreeSans9pt7b);
  cw = 9;
  
  tmp = atoi(humin);
  //snprintf(scr,20,"%s %%", humin);
  snprintf(scr,20,"%d %%", tmp);
  xpos = (100 - strlen(scr) * cw);
  tft.setCursor(xpos, 170);
  tft.print(scr);
  tft.drawBitmap(10, 156, humicon, 11, 16, ILI9341_WHITE);

  tmp = atoi(pressin);
  //snprintf(scr,20,"%s mb", pressin);
  snprintf(scr,20,"%d mb", tmp);
  xpos = (230 - strlen(scr) * cw);
  tft.setCursor(xpos, 170);
  tft.print(scr);
  tft.drawBitmap(130, 156, pressicon, 14, 16, ILI9341_WHITE);
  
  tmp = atoi(coin);
  //snprintf(scr,20,"%s ppm", coin);
  snprintf(scr,20,"%d ppm", tmp);
  xpos = (100 - strlen(scr) * cw);
  tft.setCursor(xpos, 190);
  tft.print(scr);
  tft.drawBitmap(8, 176, co2icon, 16, 16, ILI9341_WHITE);

  tmp = atoi(noisein);
  //snprintf(scr,20,"%s db", noisein);
  snprintf(scr,20,"%d db", tmp);
  xpos = (230 - strlen(scr) * cw);
  tft.setCursor(xpos, 190);
  tft.print(scr);
  tft.drawBitmap(130, 176, noiseicon, 15, 16, ILI9341_WHITE);

  tmp = atoi(humavo);
  snprintf(scr,20,"%d %%", tmp);
  xpos = (230 - strlen(scr) * cw);
  tft.setCursor(xpos, 210);
  tft.print(scr);
  tft.drawBitmap(130, 196, humicon, 11, 16, ILI9341_WHITE);

  tft.setTextColor(ILI9341_RED);
  tft.setFont(&FreeSans18pt7b);
  cw = 18;
  snprintf(scr,20,"%s C", tempout);
  xpos = (230 - strlen(scr) * cw);
  tft.setCursor(xpos, 265);
  tft.print(scr);

  tft.setFont(&FreeSans9pt7b);
  cw = 9;
  
  tmp = atoi(humout);
  //snprintf(scr,20,"%s %%", humout);
  snprintf(scr,20,"%d %%", tmp);
  xpos = (100 - strlen(scr) * cw);
  tft.setCursor(xpos, 295);
  tft.print(scr);
  tft.drawBitmap(10, 281, humicon, 11, 16, ILI9341_WHITE);

  tmp = atoi(pressin);
  //snprintf(scr,20,"%s mb", pressin);
  snprintf(scr,20,"%d mb", tmp);
  xpos = (230 - strlen(scr) * cw);
  tft.setCursor(xpos, 295);
  tft.print(scr);
  tft.drawBitmap(130, 281, pressicon, 14, 16, ILI9341_WHITE);
}

void loop() {
  ArduinoOTA.handle();

  smillis = millis();
  if (!client.connected()) {
    reconnect();
  }
  if (!configured) {
    if ( confstage == 0 ) {
      getConfiguration("initialize");
    }
    if ( confstage == 4 ) {
      num = 0;
      configured = true;
      paintScreen();
    }
  }
  
  client.loop();
  
  if (configured) {
    btnLoop();
    delaym = delaym + (millis() - smillis);
    
    if ( delaym > sleepmillis && !sleep ) {
      paintSleep();
      sleep = true;
    }
    if ( delaym < sleepmillis && sleep ) {
      paintScreen();
      sleep = false;
    }
  }
}

