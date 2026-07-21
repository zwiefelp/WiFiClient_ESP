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
#include <math.h>
#include "secrets.h"

const char* ssid     = SECRET_WIFI_SSID;
const char* password = SECRET_WIFI_PASSWORD;
const char* mqtt_server = SECRET_MQTT_SERVER;

#define TFT_DC 2
#define TFT_CS -1
#define sleepmillis 10000
#define WIFI_TIMEOUT_MS 20000   // WLAN-Verbindung: Timeout, dann Neustart
#define HC595 
#define DEBUG
// TFT Rotation: 0=Pinheader on bottom, 2=Pinheader on top
#define TFTROT 2

// ---- Farbpalette (RGB565) fuer das Card-Design in paintScreen() ----
// Kraeftige, gesaettigte Farben - das Panel hat starken Blaustich/Gamma,
// Pastelltoene wirken darauf ausgewaschen. Karten neutralgrau statt Navy.
#define COL_BG       ILI9341_BLACK
#define COL_CARD     0x0841   // neutrales, fast schwarzes Grau
#define COL_CARD_HI  0x4208   // Icon-Kachel (mittleres Grau)
#define COL_STROKE   0x52AA   // Kartenrand (deutlich sichtbar)
#define COL_TEXT     ILI9341_WHITE
#define COL_MUTED    0xC618   // Untertitel/gedaempft (helles Grau)
#define COL_ACCENT   0x07FF   // Cyan (voll gesaettigt)
#define COL_ON       0x07E0   // Gruen  (Ein/An/Start)
#define COL_OFF      0xF800   // Rot    (Aus/Stop)
#define COL_NEUTRAL  0xC618   // Hellgrau (neutrale Aktionen, z.B. Rollo Auf/Ab)
#define COL_TRACK    0x2945   // Grau   (inaktive Flaeche)
#define COL_AMBER    0xFD20   // Orange  (Sensor CO2)
#define COL_VIOLET   0xF81F   // Magenta (Sensor Laerm)

// ---- Kartenmasse + Button-Raster ----
// 5 Baender a 64px ueber die volle Hoehe: Band 0 = Navigation (oberstes
// Taster-Paar), Band 1-4 = Member 1-4. So liegen die Karten auf Hoehe der
// zugehoerigen physischen Taster.
#define CARD_X    8
#define CARD_W    224
#define CARD_H    56
#define BAND_H    64

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

// ============================================================
//  Gemeinsame Zeichen-Helfer (Boot-/Sleep-Screen)
// ============================================================

// Kreisbogen (Grad; 0=rechts, im Uhrzeigersinn da y nach unten; -90=oben).
void drawArc(int cx, int cy, int r, int a0, int a1, int thickness, uint16_t color) {
  for (int a = a0; a <= a1; a++) {
    float rad = a * 0.0174533f;
    float c = cosf(rad), s = sinf(rad);
    for (int t = 0; t < thickness; t++) {
      tft.drawPixel(cx + (int)((r + t) * c), cy + (int)((r + t) * s), color);
    }
  }
}

// WiFi-Symbol: drei nach oben offene Boegen ueber einem Punkt.
void drawWifiIcon(int cx, int cy, int size, uint16_t color) {
  drawArc(cx, cy, (int)(size * 0.30f), 210, 330, 3, color);
  drawArc(cx, cy, (int)(size * 0.58f), 210, 330, 3, color);
  drawArc(cx, cy, (int)(size * 0.86f), 210, 330, 3, color);
  tft.fillCircle(cx, cy, 3, color);
}

// Ring-Gauge: voller Track + Wertbogen ab 12 Uhr im Uhrzeigersinn.
void drawRing(int cx, int cy, int r, float frac, uint16_t color) {
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  drawArc(cx, cy, r, 0, 359, 3, COL_TRACK);
  if (frac > 0) drawArc(cx, cy, r, -90, -90 + (int)(360 * frac), 3, color);
}

// Statuszeile des Splash-Screens (Band bei y~190) neu setzen.
void drawSplashMsg(const char* msg, uint16_t col) {
  int16_t x1, y1;
  uint16_t tw, th;
  tft.fillRect(0, 186, 240, 22, COL_BG);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(col);
  tft.getTextBounds(msg, 0, 0, &x1, &y1, &tw, &th);
  tft.setCursor((240 - tw) / 2 - x1, 202);
  tft.print(msg);
}

// Fortschrittsbalken (0..100).
void drawProgress(int pct) {
  const int bx = 40, bw = 160, by = 228, bh = 8;
  tft.fillRoundRect(bx, by, bw, bh, 4, COL_TRACK);
  if (pct > 0) tft.fillRoundRect(bx, by, bw * pct / 100, bh, 4, COL_ACCENT);
}

// Kompletter Splash-Screen: WiFi-Icon im Glow-Kreis, Titel, Status, Balken.
void drawSplash(const char* title, const char* msg, uint16_t msgCol) {
  int16_t x1, y1;
  uint16_t tw, th;
  tft.setRotation(TFTROT);
  tft.fillScreen(COL_BG);
  tft.fillCircle(120, 92, 46, COL_CARD);
  tft.fillCircle(120, 92, 34, COL_CARD_HI);
  drawWifiIcon(120, 104, 30, COL_ACCENT);
  tft.setFont(&FreeSans18pt7b);
  tft.setTextColor(COL_TEXT);
  tft.getTextBounds(title, 0, 0, &x1, &y1, &tw, &th);
  tft.setCursor((240 - tw) / 2 - x1, 168);
  tft.print(title);
  drawSplashMsg(msg, msgCol);
  drawProgress(0);
}

// Temperatur-Karte fuer den Dashboard/Sleep-Screen.
void drawTempCard(int x, int y, int w, int h, const char* label,
                  const char* temp, const char* hum, uint16_t col) {
  char buf[16];
  tft.fillRoundRect(x, y, w, h, 10, COL_CARD);
  tft.drawRoundRect(x, y, w, h, 10, COL_STROKE);

  tft.setFont();
  tft.setTextColor(COL_MUTED);
  tft.setCursor(x + 12, y + 12);
  tft.print(label);

  tft.setFont(&FreeSans18pt7b);
  tft.setTextColor(col);
  tft.setCursor(x + 10, y + 50);
  snprintf(buf, 16, "%.1f", atof(temp));   // auf 1 Nachkommastelle begrenzen
  tft.print(buf);
  tft.setFont(&FreeSans9pt7b);
  tft.print(" C");

  snprintf(buf, 16, "%d%%", atoi(hum));
  tft.drawBitmap(x + 10, y + h - 22, humicon, 11, 16, COL_ACCENT);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(COL_TEXT);
  tft.setCursor(x + 26, y + h - 9);
  tft.print(buf);
}

// Sensor-Kachel mit Ring-Gauge, Icon, Wert und Einheit.
void drawSensorTile(int x, int y, int w, int h, const uint8_t* icon, int iw,
                    uint16_t col, const char* value, const char* unit, float frac) {
  tft.fillRoundRect(x, y, w, h, 10, COL_CARD);
  tft.drawRoundRect(x, y, w, h, 10, COL_STROKE);
  int cx = x + 26, cy = y + h / 2;
  drawRing(cx, cy, 15, frac, col);
  tft.drawBitmap(cx - iw / 2, cy - 8, icon, iw, 16, col);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(COL_TEXT);
  tft.setCursor(x + 48, cy - 1);
  tft.print(value);
  tft.setFont();
  tft.setTextColor(COL_MUTED);
  tft.setCursor(x + 48, cy + 5);
  tft.print(unit);
}

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
  tft.setRotation(TFTROT);
  drawSplash("Verbinde", ssid, COL_MUTED);

  // We start by connecting to a WiFi network
   #ifdef DEBUG
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
  #endif

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long wifiStart = millis();
  int pct = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
      #ifdef DEBUG
        Serial.print(".");
      #endif
    pct += 8;
    if (pct > 100) pct = 8;
    drawProgress(pct);

    // Timeout: Hinweis anzeigen und Neustart (sauberer Stack, erneuter Versuch)
    if (millis() - wifiStart > WIFI_TIMEOUT_MS) {
      #ifdef DEBUG
        Serial.println();
        Serial.println("WiFi timeout - restarting");
      #endif
      drawSplash("Kein WLAN", ssid, COL_OFF);
      drawSplashMsg("Neuversuch in 3s...", COL_MUTED);
      delay(3000);
      ESP.restart();
    }
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname (mDNS-Name = <Hostname>.local), sonst esp8266-[ChipID]
  #ifdef SECRET_OTA_HOSTNAME
    ArduinoOTA.setHostname(SECRET_OTA_HOSTNAME);
  #endif

  // OTA-Passwort - dringend empfohlen, sonst kann jeder im WLAN flashen
  #ifdef SECRET_OTA_PASSWORD
    ArduinoOTA.setPassword(SECRET_OTA_PASSWORD);
  #else
    #warning "Kein SECRET_OTA_PASSWORD gesetzt - OTA laeuft ohne Authentifizierung!"
  #endif

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

  drawSplash("Verbunden", WiFi.localIP().toString().c_str(), COL_ON);
  drawProgress(100);
  delay(800);

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
    for (unsigned int i = 0; i < length; i++) {
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
    for (int i=0; i <= (int)nScreens; i++) {
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
                paintCard(j);   // nur die geaenderte Karte neu zeichnen
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
  drawSplash("MQTT", "verbinde...", COL_MUTED);
  while (!client.connected()) {
    #ifdef DEBUG
      Serial.print("Attempting MQTT connection...");
    #endif
    // Attempt to connect
    if (client.connect(espID)) {
      drawSplashMsg("verbunden", COL_ON);
      drawProgress(100);
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
        for (int i=0; i <= (int)nScreens; i++) {
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
      char errbuf[40];
      snprintf(errbuf, 40, "Fehler rc=%d, erneut...", client.state());
      #ifdef DEBUG
        Serial.print("failed, rc=");
        Serial.println(client.state());
      #endif
      drawSplashMsg(errbuf, COL_OFF);
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
        if (num > (int)nScreens) { num = 0; }
        paintScreen();
      }
    }
    delaym = 0;
    delay(200);
}

void getConfiguration(const char* cmd) {
  char delimiter[] = ":";
  char *ptr;

  if ( strcmp(cmd,"initialize") == 0 ) {
    drawSplash("Konfiguration", "wird geladen...", COL_MUTED);
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
    drawSplash("Konfiguration", "neu laden...", COL_MUTED);
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

// Semantische Farbe fuer ein Tasten-Label.
uint16_t keyColor(const char* t) {
  if (strcmp(t, "Ein") == 0 || strcmp(t, "An") == 0 ||
      strcmp(t, "Start") == 0) return COL_ON;
  if (strcmp(t, "Aus") == 0 || strcmp(t, "Stop") == 0) return COL_OFF;
  return COL_NEUTRAL;   // neutral, z.B. Rollo Auf/Ab/Zu
}

// Breite einer Taste inkl. Innenabstand.
int keyWidth(const char* label) {
  int16_t x1, y1;
  uint16_t tw, th;
  tft.setFont(&FreeSans9pt7b);
  tft.getTextBounds(label, 0, 0, &x1, &y1, &tw, &th);
  return tw + 16;
}

// Zeichnet eine Taste am Kartenrand (mappt auf den physischen Taster daneben).
// active -> gefuellt mit Farbe + schwarzer Text (hoher Kontrast)
// inaktiv -> nur farbiger Rahmen + farbiger Text
void drawKey(int x, int y, const char* label, uint16_t color, bool active) {
  int16_t x1, y1;
  uint16_t tw, th;
  const int h = 26;
  tft.setFont(&FreeSans9pt7b);
  tft.getTextBounds(label, 0, 0, &x1, &y1, &tw, &th);
  int w = tw + 16;

  if (active) {
    tft.fillRoundRect(x, y, w, h, 5, color);
    tft.setTextColor(COL_BG);
  } else {
    tft.drawRoundRect(x, y, w, h, 5, color);
    tft.setTextColor(color);
  }
  tft.setCursor(x + 8 - x1, y + h / 2 - y1 - th / 2);
  tft.print(label);
}

// Zeichnet genau eine Geraetekarte (i = 1..4) des aktuellen Screens neu.
// Links/rechts je eine Taste (passend zu den Tastern neben dem Display),
// Name mittig. Wird beim Vollaufbau und bei Status-Updates genutzt.
void paintCard(unsigned int i) {
  int16_t x1, y1;
  uint16_t tw, th;

  if (strcmp(Screen[num][i].name1, "") == 0) return;

  // Member i in Band i (Band 0 = Navigation) -> Ausrichtung an den Tastern
  int cy = i * BAND_H + (BAND_H - CARD_H) / 2;

  // Karte
  tft.fillRoundRect(CARD_X, cy, CARD_W, CARD_H, 8, COL_CARD);
  tft.drawRoundRect(CARD_X, cy, CARD_W, CARD_H, 8, COL_STROKE);

  bool leftOn  = (strcmp(Screen[num][i].state, Screen[num][i].cmdl) == 0);
  bool rightOn = (strcmp(Screen[num][i].state, Screen[num][i].cmdr) == 0);
  int ky = cy + (CARD_H - 26) / 2;

  // Linke Taste (linker Taster -> cmdl) und rechte Taste (rechter Taster -> cmdr)
  int lw = keyWidth(Screen[num][i].txtl);
  int rw = keyWidth(Screen[num][i].txtr);
  drawKey(CARD_X + 6, ky, Screen[num][i].txtl, keyColor(Screen[num][i].txtl), leftOn);
  drawKey(CARD_X + CARD_W - 6 - rw, ky, Screen[num][i].txtr, keyColor(Screen[num][i].txtr), rightOn);

  // Name mittig zwischen den beiden Tasten
  int cx = ((CARD_X + 6 + lw) + (CARD_X + CARD_W - 6 - rw)) / 2;

  // name1 (Typ) dezent oben, name2 (Name) betont darunter
  tft.setFont();  // GLCD klein/duenn fuer den Typ
  tft.setTextColor(COL_MUTED);
  tft.setCursor(cx - (int)strlen(Screen[num][i].name1) * 3, cy + 18);
  tft.print(Screen[num][i].name1);

  tft.setFont(&FreeSans9pt7b);  // fett/hell fuer den Namen
  tft.setTextColor(COL_TEXT);
  tft.getTextBounds(Screen[num][i].name2, 0, 0, &x1, &y1, &tw, &th);
  tft.setCursor(cx - tw / 2 - x1, cy + 42);
  tft.print(Screen[num][i].name2);
}

void paintScreen() {
  int16_t x1, y1;
  uint16_t tw, th;

  tft.setRotation(TFTROT);
  tft.fillScreen(COL_BG);

  // ---- Titelzeile ----
  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(COL_TEXT);
  tft.getTextBounds(scrname[num], 0, 0, &x1, &y1, &tw, &th);
  tft.setCursor((240 - tw) / 2, 30);
  tft.print(scrname[num]);

  // Navigations-Chevrons: Button 1 = zurueck (links), Button 2 = weiter (rechts)
  tft.fillTriangle(10, 18, 10, 30, 4, 24, COL_MUTED);
  tft.fillTriangle(230, 18, 230, 30, 236, 24, COL_MUTED);

  // Seiten-Punkte (aktuelle Screen-Position)
  int dots = nScreens + 1;
  int dx = (240 - dots * 10) / 2;
  for (int k = 0; k < dots; k++) {
    tft.fillCircle(dx + k * 10 + 3, 46, 2, (k == (int)num) ? COL_ACCENT : COL_STROKE);
  }

  // ---- Geraetekarten ----
  for (unsigned int i = 1; i <= 4; i++) {
    paintCard(i);
  }
}

void paintSleep() {
  int16_t x1, y1;
  uint16_t tw, th;
  char buf[16];
  int tmp;

  tft.setRotation(TFTROT);
  tft.fillScreen(COL_BG);

  // ---- Uhr + Datum ----
  tft.setFont(&FreeSans18pt7b);
  tft.setTextColor(COL_TEXT);
  tft.getTextBounds(daytime, 0, 0, &x1, &y1, &tw, &th);
  tft.setCursor((240 - tw) / 2 - x1, 46);
  tft.print(daytime);

  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(COL_MUTED);
  tft.getTextBounds(daydate, 0, 0, &x1, &y1, &tw, &th);
  tft.setCursor((240 - tw) / 2 - x1, 74);
  tft.print(daydate);

  // ---- Temperatur-Karten (Innen/Aussen) ----
  drawTempCard(8, 88, 110, 86, "INNEN", tempin, humin, COL_ON);
  drawTempCard(122, 88, 110, 86, "AUSSEN", tempout, humout, COL_OFF);

  // ---- Sensor-Kacheln mit Ring-Gauges (fuellen bis zum unteren Rand) ----
  tmp = atoi(coin);
  snprintf(buf, 16, "%d", tmp);
  drawSensorTile(8, 180, 110, 66, co2icon, 16, COL_AMBER, buf, "ppm CO2",
                 (tmp - 400) / 1600.0f);

  tmp = atoi(noisein);
  snprintf(buf, 16, "%d", tmp);
  drawSensorTile(122, 180, 110, 66, noiseicon, 15, COL_VIOLET, buf, "dB Laerm",
                 (tmp - 30) / 60.0f);

  tmp = atoi(pressin);
  snprintf(buf, 16, "%d", tmp);
  drawSensorTile(8, 250, 110, 66, pressicon, 14, COL_ACCENT, buf, "mbar",
                 (tmp - 960) / 80.0f);

  tmp = atoi(humavo);
  snprintf(buf, 16, "%d", tmp);
  drawSensorTile(122, 250, 110, 66, humicon, 11, COL_ON, buf, "% Avocado",
                 tmp / 100.0f);
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

