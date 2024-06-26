
//#define DEBUG      // uncomment when debug
#define FEATURE_WiFi

//#include "M5StickCPlus/src/M5StickCPlus.h"
#include <M5StickCPlus.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <LSM303.h>

#ifdef FEATURE_WiFi
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
WebServer server(80);  // 80番ポートを利用
#endif // FEATURE_WiFi

#define APPLICATION  "StickCompass"
#define MAIN_VERSION  1
#define SUB_VERSION  0
#define SERVER_NAME "StickCompass"
#define COMPASS_MIN_ARRAY {32767, 32767, 32767}
#define COMPASS_MAX_ARRAY {-32768, -32768, -32768}
#define COMPASS_VECTOR {0, 1, 0}
#define DECLINATION -8.4    // 正数：東偏　　負数:西偏　
char IP_ADDRESS[] = "192.168.100.175";
#define IP_GATEWAY "192.168.10.1"
#define SUBNET_MASK "255.255.255.0"
#define ESSID "aterm-96a0z4-a"
#define PASSWORD "2e554b53b4104e"

#define LED_PIN 10
#define LoopDelay 100
#define ComPortSpeed 115200

Ticker blinker;
float blinker_Pace = 0.5; // seconds
Ticker beeper;
float beeper_Pace = 1.0;  // seconds

int Menu_Rows_count = 5;  // メニューの最大表示行数

enum mMenu_Level {mMainMenu, mSubMenu};   //　メユーはMainとSubから構成される
uint16_t Menu_Level = mMainMenu;          //  現在のメユーレベル

enum mMainMenu {mMeasure, mCalibration, mSetup};
char *MainMenu_Items[] = {"Measure", "Calibration", "Setup"};
uint16_t MainMenu_No = mMeasure;
int MainMenu_Rows_Count = 3;
int MainMenu_Rows_Index = 0;

enum mSubMenu {mServer, mDeclination, mComPort, mLED, mBeep, mExit};
char *SubMenu_Items[] = {"Server", "Declination", "Com port", "LED", "Beep", "Exit"};
uint16_t SubMenu_No = 0;
int SubMenu_Rows_Count = 6;
int SubMenu_Rows_Index = 0;
int SubMenu_Rows_Top_Index = 0;   //  1行目に表示するメニューのIndex

LSM303 Compass;
LSM303::vector<int16_t> running_min = {32767, 32767, 32767}, running_max = { -32768, -32768, -32768};
LSM303::vector<int16_t> Compass_Vector;
float Azimuth;
float Elevation;
char report[80];

bool SetupValueChanged = false;
struct SetupValue_t {
  char Application [16];
  byte Main_Version;
  byte Sub_Version;
  char Server_Name[16];
  char IP_Address [22];
  LSM303::vector<int16_t> Compass_Min;
  LSM303::vector<int16_t> Compass_Max;
  float Declination;
  bool WiFi;
  bool LED;
  bool Beep;
} SetupValue;


//-----------------------------------------------------------------------------------
void setup() {
  M5.begin();
  InitializeSerial();
#ifdef DEBUG
  Serial.println("----------setup START");
#endif
  InitializeServer();
  InitializeCompass();

  pinMode(LED_PIN, OUTPUT);
  blinker.attach(blinker_Pace, blink);

  Menu_Level = mMainMenu;
  MainMenu_No = mMeasure;
  SubMenu_No = mServer;

  //  Measurment();     // 最初から測定に入る
#ifdef DEBUG
  Serial.println("----------setup END");
#endif
}

void loop() {
#ifdef DEBUG
  //  Serial.println("----------loop START");
#endif
  if (SetupValueChanged) {
    WriteEEPROM;
    SetupValueChanged = false;
  }
  switch (Menu_Level) {
    case (mMainMenu):
      MainMenu();
      break;
    case (mSubMenu) :
      SubMenu();
      break;
  }
  server.handleClient();
  delay(LoopDelay);
}

//-----------------------------------------------------------------------------------
void MainMenu () {
  static bool Init = true;

#ifdef DEBUG
  Serial.println("----------MainMenu START");
#endif
  if (Init) {
    SetupDisplay();     // Displayの初期設定
    DisplayMainMenu();
    Init = false;
  }

  M5.update();
  if (M5.BtnA.wasReleased()) {      // MainMenuの選択決定
    MainMenu_No = MainMenu_Rows_Index;
    ExecMainMenu();
    Init = true;
  }
  else if (M5.BtnB.wasReleased()) {
    MainMenu_Rows_Index--;
    if (MainMenu_Rows_Index < 0) {
      MainMenu_Rows_Index = MainMenu_Rows_Count - 1;
    }
    DisplayMainMenu();
  }
  else if (M5.Axp.GetBtnPress() == 2) {   //  BtnC:2秒未満のクリック
    MainMenu_Rows_Index++;
    if (MainMenu_Rows_Index >= MainMenu_Rows_Count) {
      MainMenu_Rows_Index = 0;
    }
    DisplayMainMenu();
  }
#ifdef DEBUG
  Serial.println("----------MainMenu END");
#endif
}

void ExecMainMenu() {
#ifdef DEBUG
  Serial.println("----------ExecMainMenu START");
#endif
  switch (MainMenu_No) {
    case mMeasure:
      Measurment();
      break;
    case mCalibration:
      Calibration() ;
      break;
    case mSetup:
      Menu_Level = mSubMenu;
      break;
  }
}

void DisplayMainMenu() {
  // 選択されたメニュの色を設定する
#ifdef DEBUG
  Serial.println("----------DisplayMainMenu START");
#endif
  //  uint16_t col;

  M5.Lcd.fillScreen(BLACK);
  for (int i = 0; i < MainMenu_Rows_Count; i++) {
    if (i == MainMenu_Rows_Index) {
      PrintALine(1, i, MainMenu_Items[i], YELLOW);
    }
    else {
      PrintALine(1, i, MainMenu_Items[i], GREEN);
    }
  }
}

//-----------------------------------------------------------------------------------
void SubMenu () {
  //#ifdef DEBUG
  //  Serial.println("----------SubMenu START");
  //#endif
  static bool Init = true;      //  繰り返し呼ばれる間も値が保存される

  if (Init) {
    SetupDisplay();
    DisplaySubMenu();
    Init = false;
  }
  M5.update();
  if (M5.BtnA.wasReleased()) {
    SubMenu_No = (uint16_t)SubMenu_Rows_Index;
    execSubMenu();                          // 各SubMenuを実行する
    Init = true;
  }
  else if (M5.BtnB.wasReleased()) {
    SubMenu_Rows_Index--;
    if (SubMenu_Rows_Index < 0) {
      SubMenu_Rows_Index = SubMenu_Rows_Count - 1;
    }
    DisplaySubMenu();
  }
  else if (M5.Axp.GetBtnPress() == 2) {       //  BtnC:2秒未満のクリック
    SubMenu_Rows_Index++;
    if (SubMenu_Rows_Index >= SubMenu_Rows_Count) {
      SubMenu_Rows_Index = 0;
    }
    DisplaySubMenu();
  }
  //#ifdef DEBUG
  //  Serial.println("----------SubMenu END");
  //#endif
}

void execSubMenu () {
#ifdef DEBUG
  Serial.println("----------execSubMenu START");
#endif
  if (SubMenu_No == mServer) {
    SubMenu_Server();
  }
  else if (SubMenu_No == mDeclination) {
    SubMenu_Declination();
  }
  else if (SubMenu_No == mComPort) {
    SubMenu_ComPort();
  }
  else if (SubMenu_No == mLED) {
    SetupValue.LED = not SetupValue.LED;
    SetupValueChanged = true;
    DisplaySubMenu();
  }
  else if (SubMenu_No == mBeep) {
    SetupValue.Beep = not SetupValue.Beep;
    SetupValueChanged = true;
    DisplaySubMenu();
  }
  else if (SubMenu_No == mExit) {
    Menu_Level = mMainMenu;
  }
}

void DisplaySubMenu() {
#ifdef DEBUG
  Serial.println("----------DisplaySunMenu START");
#endif
  uint16_t color;
  char s[16];
  int j;

  if (SubMenu_Rows_Index < SubMenu_Rows_Count - Menu_Rows_count) {
    SubMenu_Rows_Top_Index = 0;
  }
  else {
    SubMenu_Rows_Top_Index = SubMenu_Rows_Count - Menu_Rows_count;
  }

  M5.Lcd.fillScreen(BLACK);
  for (int i = 0; i <= Menu_Rows_count - 1; i++) {
    color = GREEN;            // 選択されていないMenuをグリーンに設定
    if (i + SubMenu_Rows_Top_Index == SubMenu_Rows_Index) {
      color = YELLOW;         // 選択されたMenuをイエローに設定
    }
    strcpy(s, SubMenu_Items[i + SubMenu_Rows_Top_Index]);
    PrintALine(1, i , s, color);
  }
}

//------------------------------------------------------------------------------
bool Measurment() {
  char txt[10];
  char txt1[10];

#ifdef DEBUG
  Serial.println("----------Measurment");
#endif
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setRotation( 3 );
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextFont(2);

  Compass.m_min = SetupValue.Compass_Min;
  Compass.m_max = SetupValue.Compass_Max;

#ifdef DEBUG
  snprintf(report, sizeof(report), "Measurment min:{%+4d,%+4d,%+4d}  max:{%+4d,%+4d,%+4d}",
           Compass.m_min.x, Compass.m_min.y, Compass.m_min.z,
           Compass.m_max.x, Compass.m_max.y, Compass.m_max.z);
  Serial.println(report);
#endif

  bool e = true;
  while (e) {     // どれかボタンが押される迄測定を続ける
    M5.update();
#ifdef DEBUG
    //    Serial.println("----------Measurment　loop");
#endif
    //    if ((M5.BtnA.wasReleased()) || (M5.BtnB.wasReleased()) || (M5.Axp.GetBtnPress() == 2) {
    //      MainMenu_No = mMeasure;
    //      e = false;
    //      break;
    //    };
    if (M5.BtnA.wasReleased()) {
      MainMenu_No = mMeasure;
      e = false;
      break;
    };

    if  (M5.BtnB.wasReleased()) {     // なぜかこのボタンを押すと表示Measureのままで止まる。ボタンではメニュに戻る　　？
      MainMenu_No = mMeasure;
      e = false;
      break;
    };

    if (M5.Axp.GetBtnPress() == 2) {
      MainMenu_No = mMeasure;
      e = false;
      break;
    }

    Compass.read();
    float heading = Compass.heading(Compass_Vector);

    Azimuth = heading ;  // pololu library returns float value of actual heading.
    Azimuth = Azimuth + SetupValue.Declination;
    while (Azimuth < 0) {
      Azimuth += 360;
    };
    while (Azimuth > 360) {
      Azimuth -= 360;
    };
    M5.Lcd.setTextSize(4);
    dtostrf(Azimuth, 3, 0, txt1);
    sprintf(txt, "Az:%4s  ", txt1);
    M5.Lcd.drawString(txt, 10, 18, 2);     // M5.Lcd.drawCentreString(text, x, y, font);

    Elevation = (atan2(Compass.a.y, Compass.a.z) * 180) / M_PI;     //lsm.accelData.y
    while (Elevation < -180) {
      Elevation += 360;
    };
    while (Elevation > 180) {
      Elevation -= 360;
    };
    dtostrf(Elevation, 3, 0, txt1);
    sprintf(txt, "El:%4s  ", txt1);
    M5.Lcd.drawString(txt, 23, 75, 2);

    M5.Lcd.setTextSize(1);
    sprintf(txt, "Ver. %d.%d", SetupValue.Main_Version, SetupValue.Sub_Version);
    M5.Lcd.drawString(txt, 10, 5, 2);

    // バッテリー残量表示 (簡易的に、線形で4.1Vで100%、3.0Vで0%とする)
    // AXP192のデータシートによると1ステップは1.1mV
    float BatV = M5.Axp.GetVbatData() * 1.1 / 1000;
    int8_t BatRatio = int8_t((BatV - 3.0) / 1.1 * 100);
    if (BatRatio > 100) {
      BatRatio = 100;
    } else if (BatRatio < 0) {
      BatRatio = 0;
    }
    M5.Lcd.setTextSize(1);
    sprintf(txt, "BAT:%3d%%", BatRatio);
    M5.Lcd.drawString(txt, 160, 5, 2);

#ifdef    FEATURE_WiFi
    server.handleClient();
#endif

    delay(LoopDelay);
  }   // while (e)
}

//------------------------------------------------------------------------------
void Calibration() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setRotation( 3 );
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextFont(2);
  M5.Lcd.setTextSize(2);

  snprintf(report, sizeof(report), "処理前 min:{%+4d,%+4d,%+4d} max:{%+4d,%+4d,%+4d}",
           Compass.m_min.x, Compass.m_min.y, Compass.m_min.z,
           Compass.m_max.x, Compass.m_max.y, Compass.m_max.z);
  Serial.println(report);

  running_min = COMPASS_MIN_ARRAY;
  running_max = COMPASS_MIN_ARRAY;
  Compass.init();
  Compass.enableDefault();

  bool e = true;
  while (e) {
    M5.update();
    if ((M5.BtnA.wasReleased()) || (M5.BtnB.wasReleased()) || (M5.Axp.GetBtnPress() == 2)) {
      e = false;
    }
    else {
      Compass.read();
      running_min.x = min(running_min.x, Compass.m.x);
      running_min.y = min(running_min.y, Compass.m.y);
      running_min.z = min(running_min.z, Compass.m.z);
      running_max.x = max(running_max.x, Compass.m.x);
      running_max.y = max(running_max.y, Compass.m.y);
      running_max.z = max(running_max.z, Compass.m.z);
      snprintf(report, sizeof(report), " min:\n\r {%+4d,%+4d,%+4d}\n\r max:\n\r {%+4d,%+4d,%+4d}",
               running_min.x, running_min.y, running_min.z,
               running_max.x, running_max.y, running_max.z);
      M5.Lcd.setCursor(0, 6, 2);
      M5.Lcd.println(report);    // 自動でScrollupしない

      delay(LoopDelay);
    }

    //    Compass.init();
    //    Compass.enableDefault();
    Compass.m_min = (LSM303::vector<int16_t>) running_min;
    Compass.m_max = (LSM303::vector<int16_t>) running_max;
    SetupValue.Compass_Min = (LSM303::vector<int16_t>) running_min;
    SetupValue.Compass_Max = (LSM303::vector<int16_t>) running_max;
    WriteEEPROM();
    SetupValueChanged = true;
  }   // while (e)

  snprintf(report, sizeof(report), "処理後 min:{%+4d,%+4d,%+4d}  max:{%+4d,%+4d,%+4d}",
           Compass.m_min.x, Compass.m_min.y, Compass.m_min.z,
           Compass.m_max.x, Compass.m_max.y, Compass.m_max.z);
  Serial.println(report);
}

void SubMenu_Server() {
  M5.Lcd.fillScreen(BLACK);
  PrintALine(1, 0 , "Server", YELLOW);
  PrintALine(2, 1 , SERVER_NAME, GREEN);
  PrintALine(2, 2 , IP_ADDRESS, GREEN);
  bool e = true;
  while (e) {
    M5.update();
    if ((M5.BtnA.wasReleased()) || (M5.BtnB.wasReleased()) || (M5.Axp.GetBtnPress() == 2)) {
      e = false;
    }
    else {
      delay(LoopDelay);
    }
  }
}

void SubMenu_Declination() {
  char txt[10];
   
  M5.Lcd.fillScreen(BLACK);
  PrintALine(1, 0 , "Declination", YELLOW);
  dtostrf(DECLINATION, 3, 1, txt);
  PrintALine(2, 1 , txt, GREEN);
  bool e = true;
  while (e) {
    M5.update();
    if ((M5.BtnA.wasReleased()) || (M5.BtnB.wasReleased()) || (M5.Axp.GetBtnPress() == 2)) {
      e = false;
    }
    else {
      delay(LoopDelay);
    }
  }
}

void SubMenu_ComPort() {

}

void SubMenu_exit() {
  Menu_Level = mMainMenu;
}

//------------------------------------------------------------------------------
void InitializeSerial() {
  Serial.begin(ComPortSpeed);
  delay(500);     // 時間を空けないと print() 等が機能しない
}


//------------------------------------------------------------------------------
void InitializeCompass() {
#ifdef DEBUG
  Serial.println("----------InitializeCompass START");
#endif
  Wire.begin(0, 26);
  if (!Compass.init()) {
    Serial.println(F("setup: LSM303 error"));
  }
  Compass.enableDefault();
  Compass.m_min = (LSM303::vector<int16_t>) SetupValue.Compass_Min;
  Compass.m_max = (LSM303::vector<int16_t>) SetupValue.Compass_Max;
  Compass_Vector = (LSM303::vector<int16_t>) COMPASS_VECTOR;
}

//------------------------------------------------------------------------------
void InitializeServer () {
#ifdef DEBUG
  Serial.println("----------InitializeServer START");
#endif

#ifdef FEATURE_WiFi

  //  WiFi.config(ip, gateway, subnet, DNS);   // Set fixed IP address
  //  delay(10);

  WiFi.begin(ESSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  if (!MDNS.begin(SERVER_NAME)) {
    Serial.println("mDNS failed");    //  初期化失敗
  }
  MDNS.addService("http", "tcp", 80);
  String s = ipToString(WiFi.localIP());
  s.toCharArray(IP_ADDRESS, 15);
  Serial.println("WiFi connected.");
  Serial.print("IP = ");
  Serial.println(ipToString(WiFi.localIP()));

  server.on("/", PageIndex);
  server.on("/Start", PageStart);
  server.onNotFound(PageNotFound);
  server.begin();
#endif    // FEATURE_WiFi

#ifdef DEBUG
  Serial.println("--------InitializeWiFi END");
#endif
}

void PageIndex() {
  String html;
  char str[10];

  // HTMLを組み立てる
  html = "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  html += "<meta charset=\"utf-8\"><title>StickCompass</title>";
  html += "<META http-equiv=\"refresh\" content=\"1\">";    //  1秒後に自動的にページを読み込む
  html += "</head>";
  html += "<body>";
  html += "<FONT size=\"7\">";

  html += "<P>AZ＝";
  dtostrf(Azimuth, -1, 0, str);
  html += str;
  html += "<BR>EL＝";
  dtostrf(Elevation, -1, 0, str);
  html += str;
  html += "</P>";

  html += "</FONT></body>";
  html += "</html>";
  // HTMLを出力する
  server.send(200, "text/html", html);
}

void PageStart() {
};

void PageNotFound() {
  server.send(404, "text/plain", "Not found");
};

//------------------------------------------------------------------------------
void InitializeEEPROM() {
  if (!EEPROM.begin(100)) {
    Serial.println("Failed to initialise EEPROM");
    Serial.println("Restarting...");
    delay(100);
    ESP.restart();
  }
}

void InitializeEEPROM_defaults() {
#ifdef DEBUG
  Serial.println("----------InitializeEEPROM START");
#endif
  strcpy(SetupValue.Application, APPLICATION);
  SetupValue.Main_Version = MAIN_VERSION;
  SetupValue.Sub_Version = SUB_VERSION;
  strcpy(SetupValue.Server_Name, SERVER_NAME);
  strcpy(SetupValue.IP_Address, IP_ADDRESS);
  SetupValue.Compass_Min = (LSM303::vector<int16_t>)COMPASS_MIN_ARRAY;
  SetupValue.Compass_Max = (LSM303::vector<int16_t>)COMPASS_MAX_ARRAY;
  SetupValue.Declination = DECLINATION;
  SetupValue.LED = true;
  SetupValue.Beep = true;
  WriteEEPROM();
}

void ReadEEPROM() {
  byte *p = (byte *)(void *)&SetupValue;
  unsigned int i;

  for (i = 0; i < sizeof(SetupValue); i++) {
    *p++ = EEPROM.read(i);
  }
}

void WriteEEPROM() {
  byte *p = (byte *)(void *)&SetupValue;
  unsigned int i;

  for (i = 0; i < sizeof(SetupValue); i++) {
    EEPROM.write(i, *p++);
  }
  EEPROM.commit();
}

//------------------------------------------------------------------------------
void SetupDisplay() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setRotation(3);
  M5.Lcd.setTextFont(2);
  M5.Lcd.setTextSize(2);
}

void blink() {
  if (SetupValue.LED) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  else {
    digitalWrite(LED_PIN, HIGH);
  }
}

void beep () {
  M5.begin();
  //  M5.Power.begin();

  //  M5.Speaker.begin();       // 呼ぶとノイズ(ポップ音)が出る
  //  M5.Speaker.setVolume(1);  // 0は無音、1が最小、8が初期値(結構大きい)
  //  if (SetupValue.Beep) {
  //
  //  M5.Speaker.beep();        // ビープ開始
  //  delay(100);               // 100ms鳴らす(beep()のデフォルト)
  //  M5.Speaker.mute();        //　ビープ停止

}

void PrintALine(int col , int row, char * txt, uint16_t color) {
  int x = col * 16;
  int y = row * 25;
  M5.Lcd.setTextColor(color, BLACK);
  M5.Lcd.drawString(txt, x, y);
  //  Serial.println(txt);
}

String ipToString(uint32_t ip) {
  String result = "";
  result += String((ip & 0xFF), 10);
  result += ".";
  result += String((ip & 0xFF00) >> 8, 10);
  result += ".";
  result += String((ip & 0xFF0000) >> 16, 10);
  result += ".";
  result += String((ip & 0xFF000000) >> 24, 10);
  return result;
}
