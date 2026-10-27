#include "Arduino.h"
#include "core/options.h"
#include "core/config.h"
#include "pluginsManager/pluginsManager.h"
#include "core/telnet.h"
#include "core/player.h"
#include "core/display.h"
#include "core/network.h"
#include "core/netserver.h"
#include "core/controls.h"
//#include "core/mqtt.h"
#include "core/optionschecker.h"
#include "core/timekeeper.h"
#ifdef USE_NEXTION
#include "displays/nextion.h"
#endif
#include "core/audiohandlers.h"


#if USE_OTA
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
#include <NetworkUdp.h>
#else
#include <WiFiUdp.h>
#endif
#include <ArduinoOTA.h>
#endif

#if DSP_HSPI || TS_HSPI || VS_HSPI
SPIClass  SPI2(HSPI);
#endif

extern __attribute__((weak)) void yoradio_on_setup();

#if USE_OTA
void setupOTA(){
  if(strlen(config.store.mdnsname)>0)
    ArduinoOTA.setHostname(config.store.mdnsname);
#ifdef OTA_PASS
  ArduinoOTA.setPassword(OTA_PASS);
#endif
  ArduinoOTA
    .onStart([]() {
      player.sendCommand({PR_STOP, 0});
      display.putRequest(NEWMODE, UPDATING);
      telnet.printf("Start OTA updating %s\n", ArduinoOTA.getCommand() == U_FLASH?"firmware":"filesystem");
    })
    .onEnd([]() {
      telnet.printf("\nEnd OTA update, Rebooting...\n");
      ESP.restart();
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      telnet.printf("Progress OTA: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      telnet.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        telnet.printf("Auth Failed\n");
      } else if (error == OTA_BEGIN_ERROR) {
        telnet.printf("Begin Failed\n");
      } else if (error == OTA_CONNECT_ERROR) {
        telnet.printf("Connect Failed\n");
      } else if (error == OTA_RECEIVE_ERROR) {
        telnet.printf("Receive Failed\n");
      } else if (error == OTA_END_ERROR) {
        telnet.printf("End Failed\n");
      }
    });
  ArduinoOTA.begin();
}
#endif

void setup() {
  Serial.begin(115200);
  if(REAL_LEDBUILTIN!=255) pinMode(REAL_LEDBUILTIN, OUTPUT);
  if (yoradio_on_setup) yoradio_on_setup();
  pm.on_setup();
  config.init();
  display.init();
  player.init();
  network.begin();
  if (network.status != CONNECTED && network.status!=SDREADY) {
    netserver.begin();
    initControls();
    display.putRequest(DSP_START);
    while(!display.ready()) delay(10);
    return;
  }
  if(SDC_CS!=255) {
    display.putRequest(WAITFORSD, 0);
    Serial.print("##[BOOT]#\tSD search\t");
  }
  config.initPlaylistMode();
  netserver.begin();
  telnet.begin();
  initControls();
  display.putRequest(DSP_START);
  while(!display.ready()) delay(10);
  #if USE_OTA
    setupOTA();
  #endif
  if (config.getMode()==PM_SDCARD) player.initHeaders(config.station.url);
  player.lockOutput=false;
  if (config.store.smartstart == 1) {
    player.sendCommand({PR_PLAY, config.lastStation()});
  }
  Audio::audio_info_callback =  audio_info;   // ������� �audio_change� �������������� � ����� audiohandlers.h.
  pm.on_end_setup();
}

void loop() {
  timekeeper.loop1();
  telnet.loop();
  if (network.status == CONNECTED || network.status==SDREADY) {
    player.loop();
#if USE_OTA
    ArduinoOTA.handle();
#endif
  }
  if (!controlsTaskStarted()) loopControls();
  #ifdef NETSERVER_LOOP1
  netserver.loop();
  #endif
}

/**************************************************************************
*   Plugin BacklightDown.
*   Ver.1.1 (Maleksm) for yoRadio 20.04.2026
*   ��������: fader ����������� � screensaver, ��� STOPPED,
*   � ����� ��� PLAYING � ���� ������� ������� "fader while playing".
*   ����: faderReset() � ����� ������� fader ��� ���������� ������������.
*   ����: ������� ������� ������� (isBrightening) ������ �����������.
***************************************************************************/
#include <Ticker.h>

#if (BRIGHTNESS_PIN!=255)
  Ticker backlightTicker;

  static uint8_t current_bl = map(config.store.brightness, 0, 100, 0, 255);
  static bool isFading = false;
  static bool isBrightening = false;
  static uint32_t lastFaderMillis = 0;

  // ���������� ���� �fader ������ �������� � ��� faderReset() �� controls.cpp
  bool faderShouldBeActive = false;

  // Forward declaration: backlightDown() ������������ � tickFader() ����
  void backlightDown();

  void tickFader() {
    if (!isFading && !isBrightening) return;
    if (millis() - lastFaderMillis < 30) return;
    lastFaderMillis = millis();

    uint8_t fullBr = map(config.store.brightness, 0, 100, 0, 255);

    // --- ������� ������� ---
    if (isFading) {
      uint8_t target = config.store.fadermin;
      if (current_bl == target) {
        isFading = false;
      } else if (current_bl > target) {
        if (current_bl - target < 2) current_bl = target;
        else current_bl -= 2;
      } else {
        if (target - current_bl < 2) current_bl = target;
        else current_bl += 2;
      }
      analogWrite(BRIGHTNESS_PIN, current_bl);
    }

    // --- ������� ������� ---
    if (isBrightening) {
      if (current_bl == fullBr) {
        isBrightening = false;
        // ������� �������� � ��������� ������ ������� ������ ���� fader �������
        if (faderShouldBeActive) {
          backlightTicker.attach(config.store.fadertimeout, backlightDown);
        }
      } else if (current_bl < fullBr) {
        if (fullBr - current_bl < 2) current_bl = fullBr;
        else current_bl += 2;
        analogWrite(BRIGHTNESS_PIN, current_bl);
      } else {
        if (current_bl - fullBr < 2) current_bl = fullBr;
        else current_bl -= 2;
        analogWrite(BRIGHTNESS_PIN, current_bl);
      }
    }
  }

  void backlightDown() {
    if (!config.store.faderon || network.status == SOFT_AP) return;
    if (isBrightening) return;   // �� ������, ���� ���� ������� �������
    isFading = true;
  }


  void brightnessOn() {
    if (!config.store.faderon) return;
    backlightTicker.detach();
    isFading = false;
    // ������� ������� (attach ������� � � tickFader() ����� ����������)
    isBrightening = true;
  }

  void updateFaderState() {
    backlightTicker.detach();
    isFading = false;
    if (config.store.faderon) {
      brightnessOn();
    } else {
      current_bl = map(config.store.brightness, 0, 100, 0, 255);
      analogWrite(BRIGHTNESS_PIN, current_bl);
    }
  }

  void yoradio_on_setup() {
    current_bl = map(config.store.brightness, 0, 100, 0, 255);
    analogWrite(BRIGHTNESS_PIN, current_bl);
  }

  // === FADER: ����� ������� ��� ���������� ������������ (���������� �� controls.cpp) ===
  void faderReset() {
    if (config.store.faderon && faderShouldBeActive) {
      brightnessOn();
    }
  }
  // === /FADER ===

  void player_on_track_change() {
    //if (config.isScreensaver ||
     //   (player.status() == PLAYING && config.store.screensaverPlayingBlank))
      //brightnessOn();
	    if (config.isScreensaver) brightnessOn();   // ������ � screensaver

  }
  void player_on_start_play() {
    if (config.isScreensaver || config.store.screensaverPlayingBlank)
      brightnessOn();
  }
  void player_on_stop_play() {
    if (config.isScreensaver) brightnessOn();
  }

void ctrls_on_loop() {
  tickFader();

  static bool prevFaderActive = false;
  static bool prevScrpb = false;
  static bool prevScrpe = false;
  static bool prevScre  = false;

  bool scrpb = config.store.screensaverPlayingBlank;
  bool scrpe = config.store.screensaverPlayingEnabled;
  bool scre  = config.store.screensaverEnabled;

  if (scrpb != prevScrpb || scrpe != prevScrpe || scre != prevScre) {
    prevScrpb = scrpb;
    prevScrpe = scrpe;
    prevScre  = scre;
    brightnessOn();
    prevFaderActive = true;
  }

  faderShouldBeActive =
      config.isScreensaver ||
      (player.status() == STOPPED) ||
      (player.status() == PLAYING && scrpb);

  if (faderShouldBeActive && !prevFaderActive) {
    brightnessOn();
  }
  if (!faderShouldBeActive && prevFaderActive) {
    brightnessOn();
    backlightTicker.detach();
  }

  // ? ��������: ���� fader �������� � �������������� �������� ������
  if (!faderShouldBeActive) {
    backlightTicker.detach();
  }

  prevFaderActive = faderShouldBeActive;
}
#endif  /*  #if (BRIGHTNESS_PIN!=255) */

/*******************************************************************
*   Plugin tm_1637_dsp. For yoRadio v0.9.xxx
*   Author: bill-gilbert
*   Created on: Mar 10.2023     Updated on: Jun 21.2025 (Maleksm)

��� 7 ����������� ������� �� TM1637
�������������� �������� � ���������� � � ����������� ������� 00:00 � 0.0.0.0

- ����������� CLK � DIO �  �� ����� ��������� ���� , � �������� ���������.
  GPIO12 �� ������������ ����� �������� � �������� ��-�������������� ESP32
  � ��� ��� CLK � DIO ��������� ����������� � ����� �������.

- ������������ �����, ����� ������� � ��������� ��� ���������
- ������� ������� WiFi
-- ������������� �������������� ������������ ������ PLAY, STOP
-- ��� ������ ������������ ����� � � ����� �������

������� ��������� ���������� �� ����, ������ ���������������

������ �������� ��� �������  #define DSP_DUMMY,
� ��� �� � ������ ���������, ��������� � ILI9341, ST7789, NOKIA5110, ST7735, NEXTION
*******************************************************************/

#if (defined(TM_CLK) && defined(TM_DIO))
#ifndef TM_BRIGHT
  #define TM_BRIGHT   4 		// ������� ���������� (0-7)
#endif

#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ����� ������� � ���������� ���������
uint8_t bufseg[] = {0x40, 0x3d,  0x3f, 0x40 }; // ��������� ��������� "-GO-"
uint8_t mode_inf;
uint8_t vol_old;
uint16_t num_old;
uint16_t num_next;
uint8_t errors = 0;

// ��������� ����� FreeRTOS
#define STACK_SIZE  1024*2 		/* ������ ����� ������� */
TaskHandle_t TmTask;

// ��������� �������
#define NUMSTAT config.store.lastStation
#define NUMNEXT display.currentPlItem
#define VOLUME config.store.volume

// ������� TM1637
#define BITDELAY delayMicroseconds(100)
#define tm_COMM1    0x40
#define tm_COMM2    0xC0
#define tm_COMM3    0x80

// ���� �������� ��� 7-����������� ����������
const uint8_t digitHEX[] = {
  0x3f, 0x06, 0x5b, 0x4f, 			// 0-3
  0x66, 0x6d, 0x7d, 0x07, 			// 4-7
  0x7f, 0x6f, 0x00, 0x40  			// 8-9, ������, -
};

// ��������� �������
void tm_init();
void tm_start();
void tm_stop();
bool tm_writeByte(uint8_t b);
void tm_bright(uint8_t b);
void tm_setSegments(const uint8_t segments[], uint8_t length, uint8_t pos);
void tm_displayByte(uint8_t bit0, uint8_t bit1, uint8_t bit2, uint8_t bit3);
void clock_seg();
void print_seg(float value, uint8_t sym);
void info_play();
void info_updating();
void createTmTask();
void loopTmTask(void * pvParameters);
void tm1637();

// ================= TM1637 ������� =================
void tm_init(){
  pinMode(TM_CLK, INPUT);
  pinMode(TM_DIO, INPUT);
  digitalWrite(TM_CLK, LOW);
  digitalWrite(TM_DIO, LOW);
  uint8_t dispArray[] = {0x6d, 0x78, 0x3f, 0x73};
  tm_setSegments(dispArray, 4, 0); // ������� �������
}

void tm_start(){
  pinMode(TM_DIO, OUTPUT);
  BITDELAY;
}

void tm_stop(){
  pinMode(TM_DIO, OUTPUT);
  BITDELAY;
  pinMode(TM_CLK, INPUT);
  BITDELAY;
  pinMode(TM_DIO, INPUT);
  BITDELAY;
}

bool tm_writeByte(uint8_t b){
  uint8_t data = b;

  // 8 Data Bits
  for (uint8_t i = 0; i < 8; i++) {
    // CLK low
    pinMode(TM_CLK, OUTPUT);
    BITDELAY;

    // Set data bit
    if (data & 0x01)
      pinMode(TM_DIO, INPUT);
    else
      pinMode(TM_DIO, OUTPUT);
    BITDELAY;

    // CLK high
    pinMode(TM_CLK, INPUT);
    BITDELAY;
    data = data >> 1;
  }

  // Wait for acknowledge
  // CLK to zero
  pinMode(TM_CLK, OUTPUT);
  pinMode(TM_DIO, INPUT);
  BITDELAY;

  // CLK to high
  pinMode(TM_CLK, INPUT);
  BITDELAY;
  
  uint8_t ack = digitalRead(TM_DIO);
  if (ack == 0)
  
  pinMode(TM_DIO, OUTPUT);
  BITDELAY;
  pinMode(TM_CLK, OUTPUT);
  BITDELAY;

  return ack;
}

void tm_bright(uint8_t b){
  // Write COMM3 + brightness
  b = b | 0x08;
  tm_start();
  tm_writeByte(tm_COMM3 + (b & 0x0f));
  tm_stop();
}

void tm_setSegments(const uint8_t segments[], uint8_t length, uint8_t pos) {
  // Write COMM1
  tm_start();
  tm_writeByte(tm_COMM1);
  tm_stop();

  // Write COMM2 + first digit address
  tm_start();
  tm_writeByte(tm_COMM2 + (pos & 0x03));

  // Write the data bytes
  for (uint8_t k = 0; k < length; k++)
    tm_writeByte(segments[k]);

  tm_stop();
}

void tm_displayByte(uint8_t bit0, uint8_t bit1, uint8_t bit2, uint8_t bit3){
  uint8_t dispArray[] = {bit0, bit1, bit2, bit3};
  tm_setSegments(dispArray, 4, 0);
}

// ================= �������� ������� ������� =================
void clock_seg(){
  uint8_t hrs  = network.timeinfo.tm_hour;
  uint8_t mins = network.timeinfo.tm_min;
  static bool p_clk = 0;
  //  if (hrs > 99 || mins > 99) return;
  if ((hrs / 10) == 0) bufseg[0] = 0; 		//bufseg[0] = digitHEX[0];
  else bufseg[0] = digitHEX[(hrs / 10)];
  bufseg[1] = digitHEX[hrs % 10];
  bufseg[2] = digitHEX[mins / 10];
  bufseg[3] = digitHEX[mins % 10];
  p_clk = !p_clk;
  if (p_clk) bufseg[1] |= (0x80); 			// ����-���  �����
  tm_setSegments(bufseg, 4, 0);
}

void print_seg(float value, uint8_t sym){
  if (value > 999 || value < 0 ) return;

  uint8_t digits[4];
  value = abs(value);
  digits[0] = (int)value / 1000; 			// ��������� ����� � �����
  uint16_t b = (int)digits[0] * 1000; 			// ��������������� ����������
  digits[1] = ((int)value - b) / 100; 			// ������� ���������� �����
  b += digits[1] * 100; 					// ��������� ����� � ������
  digits[2] = (int)(value - b) / 10; 			// ������� �������
  b += digits[2] * 10; 					// ����� �����, ����� � ��������
  digits[3] = value - b; 					// �������� ���������� ������

  for (uint8_t i = 0; i < 3; i++){
    if (digits[i] == 0) digits[i] = 10;
    else break;
  }
  bufseg[0] = sym;
  bufseg[1] = digitHEX[digits[1]];
  bufseg[2] = digitHEX[digits[2]];
  bufseg[3] = digitHEX[digits[3]];
//  tm_setSegments(bufseg);
  tm_setSegments(bufseg, 4, 0);
}
/*
void print_rssi(){
  int8_t value = WiFi.RSSI();  
  if (value < -99 ) value = -99;
  uint8_t digits[4];
  value = abs(value);
  digits[0] = (int)value / 1000;        // ��������� ����� � �����
  uint16_t b = (int)digits[0] * 1000;   // ��������������� ����������
  digits[1] = ((int)value - b) / 100;   // ������� ���������� �����
  b += digits[1] * 100;                 // ��������� ����� � ������
  digits[2] = (int)(value - b) / 10;    // ������� �������
  b += digits[2] * 10;                  // ����� �����, ����� � ��������
  digits[3] = value - b;                // �������� ���������� ������

  for (uint8_t i = 0; i < 3; i++) {
    if (digits[i] == 0) digits[i] = 10;
    else break;
  }
  bufseg[0] = 0x40;//   -
  bufseg[1] = digitHEX[digits[2]];
  bufseg[2] = digitHEX[digits[3]]; 
  bufseg[3] = 0x5e;//   d
  tm_setSegments(bufseg, 4, 0);
}
*/

// ================= ���������� �������� FreeRTOS =================
void createTmTask(){
  xTaskCreatePinnedToCore(
    loopTmTask, 			// ������� ������
    "TmTask", 			// ��� ������
    STACK_SIZE, 			// ������ �����
    NULL, 				// ���������
    4, 					// ���������
    &TmTask, 			// ���������� ������
    !xPortGetCoreID() 		// ���� (0 ��� 1)
  );
}

void loopTmTask(void * pvParameters){
   while(true){
      tm1637();
      vTaskDelay(5);
   }
   vTaskDelete( NULL );
   TmTask=NULL;
}

// ================= �������� ���� ������� =================
void tm1637(){
   static uint8_t f_vol = 0;
   static uint8_t f_num = 0;
   static uint8_t f_pos = 0;

   if (network.timeinfo.tm_year<100){ 		// ���� ����������� � ���� � ��������� �������� �������
//   if (network.status != CONNECTED && network.status != SDREADY){ 		// ���� ����������� � ����
    vTaskDelay(200);
    bufseg[0] =  0;   
    bufseg[1] =  0;
    bufseg[2] =  0; 
    bufseg[3] =  0; 
    bufseg[f_pos] = 0x40; 				//_dash;
    tm_setSegments(bufseg, 4, 0);
    f_pos++;
    if (f_pos==4) f_pos = 0;
    return;
    }

  // ����������� ��������� ��� ���������
    if (VOLUME != vol_old){
       vol_old = VOLUME;
       f_vol = 100 * 2; 					// 2 ������� ����� ���������
       print_seg(VOLUME, 0x1c); 			// ������ "v"
    }
    if (f_vol != 0){
        f_vol--;
        if (f_vol == 0) f_vol = 0;
        vTaskDelay(10);
        return;  
    }

  // ����������� ������ ������� ��� ���������
    if (NUMSTAT != num_old){
       num_old = NUMSTAT;
       f_num= 100 * 1; 				// 1 ������� ����� �������
       print_seg(NUMSTAT, 0x58); 		// ������ "c"
    }
    if (f_num != 0){
       f_num--;
       if (f_num ==0) f_num = 0;
       vTaskDelay(10);
       return;  
    }

  // ����� ������ �������
  if (display.mode() == STATIONS){
    if (NUMNEXT != num_next){
       num_next = NUMNEXT;
//-----------------------------------
        print_seg(NUMNEXT, 0x54); 			// ������ "n"
//        print_seg(NUMSTAT, 0x63); 		// ������ "o"
//        print_seg(NUMNEXT, 0x58); 		// ������ "c"
//-----------------------------------
    }
       mode_inf = 0;
       vTaskDelay(10);
       return;
  }

  // ����� ����������
  if (display.mode() == UPDATING) {
    info_updating();
    return;
  }
  
  // �������� ����� ������
  if (display.mode() == PLAYER) {
    info_play();
  }
}

void info_play(){
   static unsigned long lastTime = 0;
   if (millis() - lastTime < 500) return; 		// ����������
   lastTime = millis();

//-----------ERROR-------------------  
   if (player.status() == ERROR) errors = 1; else  errors = 0;
/*   if (errors == 1){
     print_seg(NUMSTAT, 0x79); 			// ������ "E"
     return;
   }	*/

   if (mode_inf>100){
      if (player.status() == STOPPED)
         tm_displayByte(0x6d,0x78,0x3f,0x73); 	// ������� "StOP"
      if (player.status() == PLAYING)
         tm_displayByte(0x73,0x38,0x77,0x6e); 	// ������� "PLAY"
      mode_inf++;
      if (mode_inf==103) mode_inf=0;
      return;
   }

    if (mode_inf<4){
       print_seg(NUMSTAT, (errors == 1)?0x79:0x58); 		// 4/2= 2 ��� - ����� ������� ( "E":"c" )
       mode_inf++;
       return;
    }
/*
    if (mode_inf<12){
    print_rssi(); 						// �������� RSSI
    mode_inf++;
    return;
    }
*/
    clock_seg();
    mode_inf++;
    if (mode_inf==18) mode_inf=101; 			// (18-4)/2= 7 ������ - ����

}

// ================= ����������� ������� ������ =================
void info_updating(){
   tm_displayByte(0x40, 0x3e, 0x73, 0x40); 		// -UP-
   mode_inf = 0;
   }

void player_on_start_play(){
    if (network.timeinfo.tm_year<100) return;
    tm_displayByte(0x73, 0x38, 0x77, 0x6e); 		// PLAY
    mode_inf=101;
  }

void player_on_stop_play(){
     tm_displayByte(0x6d, 0x78, 0x3f, 0x73); 		// StOP
     mode_inf=101;
  }

void player_on_station_change(){
   num_old = 0; 					// ����� ��� ���������� ������ �������
  }

// ================= ������������� =================
void yoradio_on_setup(){
   tm_init();
   tm_bright(TM_BRIGHT); 			// 0...7 �������
   tm_setSegments(bufseg, 4, 0); 		// ���������� "-GO-"
//   Delay(1500);
   vTaskDelay(1500);
   createTmTask(); 				// �������� ������ FreeRTOS
   mode_inf = 5;
   vol_old = VOLUME;
   num_old = NUMSTAT;
}
#endif		// End if (defined(TM_CLK) && defined(TM_DIO))