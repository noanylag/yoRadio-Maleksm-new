#include "Arduino.h"
#include "options.h"
#include "WiFi.h"
#include "time.h"
#include "config.h"
#include "display.h"
#include "player.h"
#include "network.h"
#include "netserver.h"
#include "timekeeper.h"
#include "../pluginsManager/pluginsManager.h"
#include "../displays/dspcore.h"
#include "../displays/widgets/widgets.h"
#include "../displays/widgets/pages.h"
#include "../displays/tools/l10n.h"
#ifdef CPU_LOAD
   #include "../Perfmon/esp32_perfmon.h"
#endif

Display display;
#ifdef USE_NEXTION
#include "../displays/nextion.h"
Nextion nextion;
#endif

#ifndef CORE_STACK_SIZE
  #define CORE_STACK_SIZE  1024*4
#endif
#ifndef DSP_TASK_PRIORITY
  #define DSP_TASK_PRIORITY  3			// original: "2"
#endif
#ifndef DSP_TASK_CORE_ID
  #define DSP_TASK_CORE_ID  0
#endif
#ifndef DSP_TASK_DELAY
  #define DSP_TASK_DELAY pdMS_TO_TICKS(10) // cap for 50 fps
#endif

#define DSP_QUEUE_TICKS 0

#ifndef DSQ_SEND_DELAY
  //#define DSQ_SEND_DELAY portMAX_DELAY
  #define DSQ_SEND_DELAY  pdMS_TO_TICKS(200)
#endif

QueueHandle_t displayQueue;

#ifndef DSP_LCD	////////////////////////////// BATTERY CONSTANTS /////////////////////////////
  // ��������� ��� �����������
  #define BAT_SAMPLES 100       // ���������� ������� ��� ��������

  float ADC_R1 = R1;		// ������� ��������� �� ���� (+)
  float ADC_R2 = R2;		// ������� ��������� �� ����� (-)
//  float DELTA = config.store.batoffset;	// �������� ��������� ���������� �������

  uint8_t g, e = 1;			// �������� ��� ������� � ����������
  bool Charging = false;		// �������, ��� ���������� �������� ����������
  float Volt = 0.0f; 			// ���������� �� �������
  uint32_t lockTimer = 0;    	// ���������� ���������� ��� ���������� ������ ����������� �������
  uint8_t ChargeLevel;
  static float smoothDisplayLevel = -1.0f;
  static float voltAtStart = 0.0f;
  int startPercent = 0;     		// �������, ������� ��� �� �������

// ========= ������ ���������� �� �������, ��������������� �������� ����������� ������: 
  float vs[22] = {2.80, 3.10, 3.20, 3.26, 3.29, 3.33, 3.37, 3.41, 3.46, 3.51, 3.56, 3.61, 3.65, 3.69, 3.72, 3.75, 3.78, 3.82, 3.88, 3.95, 4.03, 4.25};

  // ��������� ��� �������� ��������� �������
  struct {
    float lastVoltage = 0;
    uint32_t lastCheck = 0;
    uint8_t chargeCount = 0;    // ������� ���������������� ���������� ����������
    uint8_t dischargeCount = 0; // ������� ���������������� ������� ����������
  } chargingState;
#endif			/////////////////////////////////////////////////////////////////////////////////

#ifdef CPU_LOAD
// ��������� ��������� ������ ��� ����������� �������� CPU
TextWidget cpuWidget;
#endif

static void loopDspTask(void * pvParameters){
  while(true){
  #ifndef DUMMYDISPLAY
    if(displayQueue==NULL) break;
    if(timekeeper.loop0()){
      display.loop();
    #ifndef NETSERVER_LOOP1
      netserver.loop();
    #endif
    }
  #else
    timekeeper.loop0();
    #ifndef NETSERVER_LOOP1
      netserver.loop();
    #endif
  #endif
    vTaskDelay(DSP_TASK_DELAY);
  }
  vTaskDelete( NULL );
}

void Display::_createDspTask(){
  xTaskCreatePinnedToCore(loopDspTask, "DspTask", CORE_STACK_SIZE,  NULL,  DSP_TASK_PRIORITY, NULL, DSP_TASK_CORE_ID);
}

#ifndef DUMMYDISPLAY
//============================================================================================================================
DspCore dsp;

Page *pages[] = { new Page(), new Page(), new Page(), new Page(), new Page() };

#if !((DSP_MODEL==DSP_ST7735 && DTYPE==INITR_BLACKTAB) || DSP_MODEL==DSP_ST7789 || DSP_MODEL==DSP_ST7796 || DSP_MODEL==DSP_ILI9488 \
 || DSP_MODEL==DSP_ILI9486 || DSP_MODEL==DSP_ILI9341 || DSP_MODEL==DSP_ILI9225 || DSP_MODEL==DSP_ST7789_170 || DSP_MODEL==DSP_GC9A01A)
  #undef  BITRATE_FULL
  #define BITRATE_FULL     false
#endif


void returnPlayer(){
  display.putRequest(NEWMODE, PLAYER);
}

Display::~Display() {
  delete _pager;
  delete _footer;
  delete _plwidget;
  delete _nums;
  delete _clock;
  delete _meta;
  delete _title1;
  delete _title2;
  delete _plcurrent;
}

void Display::init() {
  Serial.print("##[BOOT]#\tdisplay.init\t");
#ifdef USE_NEXTION
  nextion.begin();
#endif
#if LIGHT_SENSOR!=255
  analogSetAttenuation(ADC_0db);
#endif
  _bootStep = 0;
  dsp.initDisplay();
#ifndef DSP_LCD			//////////////////BATTERY INIT /////////////////////////////
  if (config.store.batteryon) {
    // ������������� ADC ��� �������
    analogSetAttenuation(ADC_11db); // ��������� ���������

    // ����� ������� �������, ����� "��������" ��� � ��������� ������
    uint32_t sum = 0;
    for(int i=0; i<50; i++) sum += analogReadMilliVolts(ADC_PIN);

    float startVolt = ((sum / 50.0f) / 1000.0f) * ((ADC_R1 + ADC_R2) / ADC_R2) + config.store.batoffset;
    if (startVolt < 0.0f) startVolt = 0.0f;

    // ������������� �������� ���������� ������� ���������
    Volt = startVolt;

    // ��������� �������: ��������� ��������� ����� ��� ������������ ������
    chargingState.lastVoltage = startVolt;
    chargingState.chargeCount = 0;
    chargingState.dischargeCount = 0;

    lockTimer = millis();		// ������ ������� ���������� ��������� �� 5 ������
  }
#endif					///////////////////////////////////////////////////////////////////////////
  displayQueue=NULL;
  displayQueue = xQueueCreate( 5, sizeof( requestParams_t ) );
  while(displayQueue==NULL){;}
  _createDspTask();
  while(!_bootStep==0) { delay(10); }
  //_pager.begin();
  //_bootScreen();
  _pager = new Pager();
  _footer = new Page();
  _plwidget = new PlayListWidget();
  _nums = new NumWidget();
  _clock = new ClockWidget();
  _meta = new ScrollWidget();
  _title1 = new ScrollWidget();
  _plcurrent = new ScrollWidget();
  Serial.println("done");
}

uint16_t Display::width(){ return dsp.width(); }
uint16_t Display::height(){ return dsp.height(); }
#if TIME_SIZE>19
  #if DSP_MODEL==DSP_SSD1322
    #define BOOT_PRG_COLOR    WHITE
    #define BOOT_TXT_COLOR    WHITE
    #define PINK              WHITE
  #elif DSP_MODEL==DSP_SSD1327
    #define BOOT_PRG_COLOR    0x07
    #define BOOT_TXT_COLOR    0x3f
    #define PINK              0x02
  #else
    #define BOOT_PRG_COLOR    0xE68B
    #define BOOT_TXT_COLOR    0xFFFF
    #define PINK              0xF97F
  #endif
#endif

void Display::_bootScreen(){
  _boot = new Page();
  _boot->addWidget(new ProgressWidget(bootWdtConf, bootPrgConf, BOOT_PRG_COLOR, 0));
  _bootstring = (TextWidget*) &_boot->addWidget(new TextWidget(bootstrConf, 50, true, BOOT_TXT_COLOR, 0));
  _pager->addPage(_boot);
  _pager->setPage(_boot, true);
  dsp.drawLogo(bootLogoTop);
  _bootStep = 1;
}

void Display::_buildPager(){
  _meta->init("*", metaConf, config.theme.meta, config.theme.metabg);
  _title1->init("*", title1Conf, config.theme.title1, config.theme.background);
  _clock->init(clockConf, 0, 0);
  #if DSP_MODEL==DSP_NOKIA5110
    _plcurrent->init("*", playlistConf, 0, 1);
  #else
    _plcurrent->init("*", playlistConf, config.theme.plcurrent, config.theme.plcurrentbg);
  #endif
  _plwidget->init(_plcurrent);
  #if !defined(DSP_LCD)
    _plcurrent->moveTo({TFT_FRAMEWDT, (uint16_t)(_plwidget->currentTop()), (int16_t)playlistConf.width});
  #endif
  #ifndef HIDE_TITLE2
    _title2 = new ScrollWidget("*", title2Conf, config.theme.title2, config.theme.background);
  #endif
  #if !defined(DSP_LCD) && DSP_MODEL!=DSP_NOKIA5110
    _plbackground = new FillWidget(playlBGConf, config.theme.plcurrentfill);
    #if DSP_INVERT_TITLE || defined(DSP_OLED)
      _metabackground = new FillWidget(metaBGConf, config.theme.metafill);
    #else
      _metabackground = new FillWidget(metaBGConfInv, config.theme.metafill);
    #endif
  #endif
  #if DSP_MODEL==DSP_NOKIA5110
    _plbackground = new FillWidget(playlBGConf, 1);
    //_metabackground = new FillWidget(metaBGConf, 1);
  #endif
  #ifndef HIDE_VU
    _vuwidget = new VuWidget(vuConf, bandsConf, config.theme.vumax, config.theme.vumin, config.theme.background);
  #endif
  #ifndef HIDE_VOLBAR
    _volbar = new SliderWidget(volbarConf, config.theme.volbarin, config.theme.background, 254, config.theme.volbarout);
  #endif
  #ifndef HIDE_HEAPBAR
    _heapbar = new SliderWidget(heapbarConf, config.theme.buffer, config.theme.background, psramInit()?300000:1600 * config.store.abuff);
  #endif
  #ifndef HIDE_VOL
    _voltxt = new TextWidget(voltxtConf, 10, false, config.theme.vol, config.theme.background);
  #endif
  #ifndef HIDE_IP
    _volip = new TextWidget(iptxtConf, 30, false, config.theme.ip, config.theme.background);
  #endif
  #ifndef HIDE_RSSI
    _rssi = new TextWidget(rssiConf, 20, false, config.theme.rssi, config.theme.background);
  #endif
  _nums->init(numConf, 10, false, config.theme.digit, config.theme.background);
  #ifndef HIDE_WEATHER
    _weather = new ScrollWidget("\007", weatherConf, config.theme.weather, config.theme.background);
  #endif
  
#ifdef CPU_LOAD
  // ��������� CPU ������ � �������� ������
  cpuWidget.init(cpuConf, 20, false, config.theme.cpu, config.theme.background);
  perfmon_start(); 			// Start CPU monitoring
//  dsp.cpuWidget.setActive(false);
#endif

  if(_volbar)   _footer->addWidget( _volbar);
  if(_voltxt)   _footer->addWidget( _voltxt);
  if(_volip)    _footer->addWidget( _volip);
  if(_rssi)     _footer->addWidget( _rssi);
  if(_heapbar)  _footer->addWidget( _heapbar);
  
  if(_metabackground) pages[PG_PLAYER]->addWidget( _metabackground);
  pages[PG_PLAYER]->addWidget(_meta);
  pages[PG_PLAYER]->addWidget(_title1);
  if(_title2) pages[PG_PLAYER]->addWidget(_title2);
  if(_weather) pages[PG_PLAYER]->addWidget(_weather);
  #if BITRATE_FULL
    _fullbitrate = new BitrateWidget(fullbitrateConf, config.theme.bitrate, config.theme.background);
    pages[PG_PLAYER]->addWidget( _fullbitrate);
  #else
    _bitrate = new TextWidget(bitrateConf, 30, false, config.theme.bitrate, config.theme.background);
    pages[PG_PLAYER]->addWidget( _bitrate);
  #endif
  if(_vuwidget) pages[PG_PLAYER]->addWidget( _vuwidget);
  pages[PG_PLAYER]->addWidget(_clock);
  pages[PG_SCREENSAVER]->addWidget(_clock);
  pages[PG_PLAYER]->addPage(_footer);

  if(_metabackground) pages[PG_DIALOG]->addWidget( _metabackground);
  pages[PG_DIALOG]->addWidget(_meta);
  pages[PG_DIALOG]->addWidget(_nums);
  
  #if !defined(DSP_LCD) && DSP_MODEL!=DSP_NOKIA5110
    pages[PG_DIALOG]->addPage(_footer);
  #endif
  #if !defined(DSP_LCD)
  if(_plbackground) {
    pages[PG_PLAYLIST]->addWidget( _plbackground);
    _plbackground->setHeight(_plwidget->itemHeight());
    _plbackground->moveTo({0,(uint16_t)(_plwidget->currentTop()-playlistConf.widget.textsize*2), (int16_t)playlBGConf.width});
  }
  #endif
  pages[PG_PLAYLIST]->addWidget(_plcurrent);
  pages[PG_PLAYLIST]->addWidget(_plwidget);

#ifndef DSP_LCD
  _btWidget = new BluetoothWidget();
  _btWidget->init(config.theme.clock, config.theme.background);
  pages[PG_BLUETOOTH]->addWidget(_btWidget);
#endif

  for(const auto& p: pages) _pager->addPage(p);
}

void Display::_apScreen() {
  if(_boot) _pager->removePage(_boot);
  #ifndef DSP_LCD
    _boot = new Page();
    #if DSP_MODEL!=DSP_NOKIA5110
      #if DSP_INVERT_TITLE || defined(DSP_OLED)
      _boot->addWidget(new FillWidget(metaBGConf, config.theme.metafill));
      #else
      _boot->addWidget(new FillWidget(metaBGConfInv, config.theme.metafill));
      #endif
    #endif
    ScrollWidget *bootTitle = (ScrollWidget*) &_boot->addWidget(new ScrollWidget("*", apTitleConf, config.theme.meta, config.theme.metabg));
    bootTitle->setText("ёRadio AP Mode");
    TextWidget *apname = (TextWidget*) &_boot->addWidget(new TextWidget(apNameConf, 30, false, config.theme.title1, config.theme.background));
    apname->setText(LANG::apNameTxt);
    TextWidget *apname2 = (TextWidget*) &_boot->addWidget(new TextWidget(apName2Conf, 30, false, config.theme.clock, config.theme.background));
    apname2->setText(apSsid);
    TextWidget *appass = (TextWidget*) &_boot->addWidget(new TextWidget(apPassConf, 30, false, config.theme.title1, config.theme.background));
    appass->setText(LANG::apPassTxt);
    TextWidget *appass2 = (TextWidget*) &_boot->addWidget(new TextWidget(apPass2Conf, 30, false, config.theme.clock, config.theme.background));
    appass2->setText(apPassword);
    ScrollWidget *bootSett = (ScrollWidget*) &_boot->addWidget(new ScrollWidget("*", apSettConf, config.theme.title2, config.theme.background));
    bootSett->setText(config.ipToStr(WiFi.softAPIP()), LANG::apSettFmt);
    _pager->addPage(_boot);
    _pager->setPage(_boot);
  #else
    dsp.apScreen();
  #endif
}

void Display::_start() {
  if(_boot) _pager->removePage(_boot);
  #ifdef USE_NEXTION
    nextion.wake();
  #endif
  if (network.status != CONNECTED && network.status != SDREADY) {
    _apScreen();
    #ifdef USE_NEXTION
      nextion.apScreen();
    #endif
    _bootStep = 2;
    return;
  }
  #ifdef USE_NEXTION
    //nextion.putcmd("page player");
    nextion.start();
  #endif
  _buildPager();
  _mode = PLAYER;
  config.setTitle(LANG::const_PlReady);
  
  if(_heapbar)  _heapbar->lock(!config.store.audioinfo);
  
  if(_weather)  _weather->lock(!config.store.showweather);
  if(_weather && config.store.showweather)  _weather->setText(LANG::const_getWeather);

  if(_vuwidget) _vuwidget->lock();
  if(_rssi)     _setRSSI(WiFi.RSSI());
  #ifndef HIDE_IP
    if(_volip) _volip->setText(config.ipToStr(WiFi.localIP()), iptxtFmt);
  #endif
  if(config.getMode()==PM_SDCARD && clockMove.width>=0) _clock->moveTo(clockMove);
  _pager->setPage( pages[PG_PLAYER]);
  _volume();
  _station();
  _time(false);
  _bootStep = 2;
  pm.on_display_player();
}

void Display::_showDialog(const char *title){
  dsp.setScrollId(NULL);
  _pager->setPage( pages[PG_DIALOG]);
  #ifdef META_MOVE
    _meta->moveTo(metaMove);
  #endif
  _meta->setAlign(WA_CENTER);
  _meta->setText(title);
}

void Display::_swichMode(displayMode_e newmode) {
  #ifdef USE_NEXTION
    //nextion.swichMode(newmode);
    nextion.putRequest({NEWMODE, newmode});
  #endif
  if (newmode == _mode || (network.status != CONNECTED && network.status != SDREADY && newmode != BLUETOOTH && _mode != BLUETOOTH)) return;
  #if DSP_MODEL==DSP_GC9A01A
    const bool returningFromVolume = newmode == PLAYER && _mode == VOL;
  #else
    const bool returningFromVolume = false;
  #endif
  _mode = newmode;
  dsp.setScrollId(NULL);
if (newmode == PLAYER) {
    if(player.isRunning() || config.getMode()==PM_SDCARD)
      if(clockMove.width<0) _clock->moveBack(); else _clock->moveTo(clockMove);
    else
      _clock->moveBack();
    #ifdef DSP_LCD
      if (!returningFromVolume) dsp.clearDsp();
    #endif
    numOfNextStation = 0;
    #ifdef META_MOVE
      _meta->moveBack();
    #endif
    _meta->setAlign(metaConf.widget.align);
    _meta->setText(config.station.name);
    _nums->setText("");
    config.isScreensaver = false;
    _pager->setPage(pages[PG_PLAYER], false, !returningFromVolume);

    // === ���� �����������: ������ ����� ������ ����������� ������� �������� ===
    // ��� ��� ����� ������ � ������ public, �� �������� ��� ��������.
    // ��� �������������� ������� ���� blockForceFirstDraw ��� ������ ������ �� ������������.
    #if BITRATE_FULL
      if (_fullbitrate) {
         _fullbitrate->_reset(); 
         _fullbitrate->setActive(true);
      }
    //#else
    //  if (_bitrate) {
    //     _bitrate->_reset(); // ���� � ��� ������� ��������� ������, ���������� ����� ��������, ��� ��� � TextWidget ��� _reset(). 
    //     // ����� ����� ��������, ��� �������� ���������� ������� ����� ����������:
    //     ((BitrateWidget*)_bitrate)->_reset();
    //     _bitrate->setActive(true);
    //  }
    //#endif
	    #else
      if (_bitrate) {
         _bitrate->setActive(true);   // TextWidget::_draw() ������ ������
      }
    #endif
    // ==================================================================

    if (config.store.faderon) {
      display.wakeup();   // ������� ������� ����� fader (ctrls_on_loop > brightnessOn)
    } else {
      config.setDspOn(config.store.dspon, false);   // ���������, ���� fader ��������
    }
    pm.on_display_player();
}

  if (newmode == BLUETOOTH) {
    config.isScreensaver = false;
    #ifdef DSP_LCD
      _showDialog("Bluetooth");
    #else
      if (_btWidget) _btWidget->resetActivityBlink();
      _pager->setPage(pages[PG_BLUETOOTH]);
    #endif
    if (config.store.faderon) display.wakeup();
  }

  if (newmode == SCREENSAVER || newmode == SCREENBLANK) {
    config.isScreensaver = true;
    
    // ��������� ������ ���������� ������ ��� ������ ������ (������� ������������� TFT)
    uint16_t lt = (dsp.width()  - _clock->clockWidth()) / 2;
    int16_t  ft;

    #if DSP_MODEL==DSP_GC9A01A
      // ��������� ������ ��� �������� �������
      ft = (dsp.height() - _clock->dateSize()) / 2 + 30;
      lt = lt - 10;
    #else
      // ��������� ������ ��� ������������� (����������) TFT �������
      #ifndef CLOCKFONT5x7
        // ���� ����� GFX (���������� Y ���� �� ������� ����� ������)
        ft = (dsp.height() + _clock->dateSize()) / 2 + 25; 
      #else
        // ���� ����� ����������� 5x7 (���������� Y � ������� ���� �������)
        ft = (dsp.height() - _clock->dateSize()) / 2;
      #endif
    #endif

    if(ft < 0) ft = 0;
    
    // Ƹ���� ��������� ���� � ����� ������ ���� ��� ��� ����� � �����������
    _clock->moveTo({lt, (uint16_t)ft, 0});

    _pager->setPage( pages[PG_SCREENSAVER]);

    if (newmode == SCREENBLANK) {
      //dsp.clearClock();
      _clock->clear();
      config.setDspOn(false, false);
    }
  }else{
    // ��� ������ �� ������������ ���������� ���� �� �� ������� ����� � ������
    #if DSP_MODEL==DSP_GC9A01A
    // �� ������� ������ ���� ����� �������: ��� ��������������� � VU, ����� � STOP.
    // moveBack() ��������, �.�. setPositionNoRedraw � _time() ����� ���� �������.
    if (player.isRunning() || config.getMode()==PM_SDCARD) {
      _clock->moveTo(clockMove);   // ������� ��� VU
    } else {
      _clock->moveBack();          // �� ������ screensaver � STOP
    }
    #endif
    
    config.screensaverTicks=SCREENSAVERSTARTUPDELAY;
    config.screensaverPlayingTicks=SCREENSAVERSTARTUPDELAY;
    config.isScreensaver = false;
  }

  if (newmode == VOL) {
    #ifndef HIDE_VOLPAGE
      #ifndef HIDE_IP
        _showDialog(LANG::const_DlgVolume);
      #else
        _showDialog(config.ipToStr(WiFi.localIP()));
      #endif
    #endif
    _nums->setText(config.store.volume, numtxtFmt);
  }
  if (newmode == LOST)      _showDialog(LANG::const_DlgLost);
  if (newmode == UPDATING)  _showDialog(LANG::const_DlgUpdate);
  if (newmode == SLEEPING)  _showDialog("SLEEPING");
  if (newmode == SDCHANGE)  _showDialog(LANG::const_waitForSD);
  if (newmode == INFO || newmode == SETTINGS || newmode == TIMEZONE || newmode == WIFI) _showDialog(LANG::const_DlgNextion);
  if (newmode == NUMBERS) _showDialog("");
  if (newmode == STATIONS) {
    _pager->setPage( pages[PG_PLAYLIST]);
    _plcurrent->setText("");
    currentPlItem = config.lastStation();
    _drawPlaylist();
  }
  
}

void Display::resetQueue(){
  if(displayQueue!=NULL) xQueueReset(displayQueue);
}

void Display::_drawPlaylist() {
  //dsp.drawPlaylist(currentPlItem);
  _plwidget->drawPlaylist(currentPlItem);
  timekeeper.waitAndReturnPlayer(10);			// ************************************
}

void Display::_drawNextStationNum(uint16_t num) {
  timekeeper.waitAndReturnPlayer(30);
  _meta->setText(config.stationByNum(num));
  _nums->setText(num, "%d");
}

void Display::putRequest(displayRequestType_e type, int payload){
  if(displayQueue==NULL) return;
  requestParams_t request;
  request.type = type;
  request.payload = payload;
  xQueueSend(displayQueue, &request, DSQ_SEND_DELAY);
  #ifdef USE_NEXTION
    nextion.putRequest(request);
  #endif
}

void Display::_layoutChange(bool played){
  if(config.store.vumeter && _vuwidget){
    if(played){
      _clock->clearDate();
      if(_vuwidget) _vuwidget->unlock();
      //_clock->moveTo(clockMove);
      if(clockMove.width<0) _clock->moveBack(); else _clock->moveTo(clockMove);
      if(_weather) _weather->moveTo(weatherMoveVU);
    }else{
      if(_vuwidget) if(!_vuwidget->locked()) _vuwidget->lock();
      if(config.getMode()!=PM_SDCARD){
        #if DSP_MODEL!=DSP_GC9A01A
        _clock->moveBack();
        #endif
      }
      if(_weather) _weather->moveBack();
    }
  }else{
    if(played){
      if(_vuwidget) _vuwidget->clearArea();
      if(clockMove.width<0) _clock->moveBack(); else _clock->moveTo(clockMove);
      if(_weather) _weather->moveTo(weatherMove);
      //_clock->moveBack();
    }else{
      if(_weather) _weather->moveBack();
      if(config.getMode()!=PM_SDCARD){
        #if DSP_MODEL!=DSP_GC9A01A
        _clock->moveBack();
        #endif
      }
    }
  }
}

#ifdef CPU_LOAD
uint32_t Display::_calculateCpuUsage() {
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate >= 500) { // Update every 500ms
        lastUpdate = millis();
        return perfmon_get_cpu_usage(0); // Get first core load
    }
    return 0;
}
#endif

void Display::loop() { static bool layoutIsPlaying = false, keepLayoutOnStart = false;
  if(_bootStep==0) {
    _pager->begin();
    _bootScreen();
    return;
  }
  if(displayQueue==NULL || _locked) return;
  _pager->loop();
#ifdef USE_NEXTION
  nextion.loop();
#endif
  requestParams_t request;
  if(xQueueReceive(displayQueue, &request, DSP_QUEUE_TICKS)){
    bool pm_result = true;
    pm.on_display_queue(request, pm_result);
    if(pm_result)
      switch (request.type){
        case NEWMODE: _swichMode((displayMode_e)request.payload); break;
        case CLOSEPLAYLIST: player.sendCommand({PR_PLAY, request.payload}); break;
        case CLOCK: 
          if(_mode==PLAYER || _mode==SCREENSAVER) _time(request.payload==1); 
          /*#ifdef USE_NEXTION
            if(_mode==TIMEZONE) nextion.localTime(network.timeinfo);
            if(_mode==INFO)     nextion.rssi();
          #endif*/
          break;
        case NEWTITLE: _title(); break;
        case NEWSTATION: _station(); break;
        case NEXTSTATION: _drawNextStationNum(request.payload); break;
        case DRAWPLAYLIST: _drawPlaylist(); break;
        case DRAWVOL: _volume(); break;
        case DBITRATE: {
            char buf[20]; 
            // ���� ������� ����� 0 (���� �����������), ����� ��������, �� ���� ������ ������ �������
            if (config.station.bitrate == 0) {
              snprintf(buf, sizeof(buf), "---");
            } else {
              snprintf(buf, sizeof(buf), bitrateFmt, config.station.bitrate); 
            }

            if(_bitrate) { 
              _bitrate->setText(buf); 
            } 
            if(_fullbitrate) { 
              _fullbitrate->setBitrate(config.station.bitrate); 
              _fullbitrate->setFormat(config.configFmt); 
            } 
          }
          break;








        case AUDIOINFO: if(_heapbar)  { _heapbar->lock(!config.store.audioinfo); _heapbar->setValue(player.inBufferFilled()); } break;
        case SHOWVUMETER: {
          if(_vuwidget){
            _vuwidget->lock(!config.store.vumeter); 
            _layoutChange(player.isRunning()); layoutIsPlaying = player.isRunning();
          }
          break;
        }
        case SHOWWEATHER: {
          if(_weather) _weather->lock(!config.store.showweather);
          if(!config.store.showweather){
            #ifndef HIDE_IP
            if(_volip) _volip->setText(config.ipToStr(WiFi.localIP()), iptxtFmt);
            #endif
          }else{
            if(_weather) _weather->setText(LANG::const_getWeather);
          }
          break;
        }
        case NEWWEATHER: {
          if(_weather && timekeeper.weatherBuf) _weather->setText(timekeeper.weatherBuf);
          break;
        }
        case BOOTSTRING: {
          if(_bootstring) _bootstring->setText(config.ssids[request.payload].ssid, LANG::bootstrFmt);
          /*#ifdef USE_NEXTION
            char buf[50];
            snprintf(buf, 50, bootstrFmt, config.ssids[request.payload].ssid);
            nextion.bootString(buf);
          #endif*/
          break;
        }
        case WAITFORSD: {
          if(_bootstring) _bootstring->setText(LANG::const_waitForSD);
          break;
        }
        case SDFILEINDEX: {
          if(_mode == SDCHANGE) _nums->setText(request.payload, "%d");
          break;
        }
        case DSPRSSI: if(_rssi){ _setRSSI(request.payload); } if (_heapbar && config.store.audioinfo) _heapbar->setValue(player.isRunning()?player.inBufferFilled():0); break;
        case PSTART: if (!keepLayoutOnStart) _layoutChange(true); layoutIsPlaying = true; keepLayoutOnStart = false; break;
        case PSTOP:
  if (request.payload == 1) keepLayoutOnStart = layoutIsPlaying; else { _layoutChange(false); layoutIsPlaying = false; keepLayoutOnStart = false; }
  break;
        case DSP_START: _start();  break;
        case NEWIP: {
          #ifndef HIDE_IP
            if(_volip) _volip->setText(config.ipToStr(WiFi.localIP()), iptxtFmt);
          #endif
          break;
        }
        default: break;

        // check if there are more messages waiting in the Q, in this case break the loop() and go
        // for another round to evict next message, do not waste time to redraw the screen, etc...
        if (uxQueueMessagesWaiting(displayQueue))
          return;
      }
  }
  dsp.loop();

    static uint32_t lastCpuUpdate = 0;
    static uint32_t lastValue = 0;
#ifdef CPU_LOAD
    if (_mode==PLAYER && network.status == CONNECTED && _bootStep==2) { 			// Check if we're on player page
       cpuWidget.setActive(true);

       if (millis() - lastCpuUpdate >= 1000) { 		// Update every second
           uint32_t cpuUsage = _calculateCpuUsage();

           if (cpuUsage != lastValue) { 		// Update widget only if value changed
               char buf[20];
               snprintf(buf, sizeof(buf), "cpu:%d%%", cpuUsage);
               cpuWidget.setText(buf);
               lastValue = cpuUsage;

//           if (force) cpuWidget->setActive(true); 	// Force widget update
           }
           lastCpuUpdate = millis();
       }
    }
    else cpuWidget.setActive(false);
#endif

#ifndef DSP_LCD		/////////// BATTERY LOOP //////////////////////////
  if (config.store.batteryon && _mode==PLAYER && network.status == CONNECTED && _bootStep==2 && !config.isScreensaver) {	
    static uint32_t lastBatteryUpdate = 0;
       if (millis() - lastBatteryUpdate >= 1000) { // ��������� ������ �������
          readBattery();
          BatteryShow();
          lastBatteryUpdate = millis();
       }
  }
#endif				//////////////////////////////////////////////////////////////////////////

/*
  #if I2S_DOUT==255
  player.computeVUlevel();
  #endif
*/
}

void Display::_setRSSI(int rssi) {
  if(!_rssi) return;
#if RSSI_DIGIT
  _rssi->setText(rssi, rssiFmt);
  return;
#endif
  char rssiG[3];
  int rssi_steps[] = {RSSI_STEPS};
  if(rssi >= rssi_steps[0]) strlcpy(rssiG, "\004\006", 3);
  if(rssi >= rssi_steps[1] && rssi < rssi_steps[0]) strlcpy(rssiG, "\004\005", 3);
  if(rssi >= rssi_steps[2] && rssi < rssi_steps[1]) strlcpy(rssiG, "\004\002", 3);
  if(rssi >= rssi_steps[3] && rssi < rssi_steps[2]) strlcpy(rssiG, "\003\002", 3);
  if(rssi <  rssi_steps[3] || rssi >=  0) strlcpy(rssiG, "\001\002", 3);
  _rssi->setText(rssiG);
}

void Display::_station() {
  _meta->setAlign(metaConf.widget.align);
  _meta->setText(config.station.name);
/*#ifdef USE_NEXTION
  nextion.newNameset(config.station.name);
  nextion.bitrate(config.station.bitrate);
  nextion.bitratePic(ICON_NA);
#endif*/
}

char *split(char *str, const char *delim) {
  char *dmp = strstr(str, delim);
  if (dmp == NULL) return NULL;
  *dmp = '\0'; 
  return dmp + strlen(delim);
}

void Display::_title() {
  if (strlen(config.station.title) > 0) {
    char tmpbuf[strlen(config.station.title)+1];
    strlcpy(tmpbuf, config.station.title, strlen(config.station.title)+1);
    char *stitle = split(tmpbuf, " - ");
    if(stitle && _title2){
      _title1->setText(tmpbuf);
      _title2->setText(stitle);
    }else{
      _title1->setText(config.station.title);
      if(_title2) _title2->setText("");
    }
    /*#ifdef USE_NEXTION
      nextion.newTitle(config.station.title);
    #endif*/
    
  }else{
    _title1->setText("");
    if(_title2) _title2->setText("");
  }
  if (player_on_track_change) player_on_track_change();
  pm.on_track_change();
}

void Display::_time(bool redraw) {
  
#if LIGHT_SENSOR!=255
  if(config.store.dspon) {
    config.store.brightness = AUTOBACKLIGHT(analogRead(LIGHT_SENSOR));
    config.setBrightness();
  }
#endif

  // ===== ��� �������� ������ GC9A01A: ������� ����� � screensaver �������
  // � _swichMode() ����� moveTo(). ����� setPositionNoRedraw ������ �� ������������,
  // ����� �� ������ ���� ������� � moveBack() ���������� ���� �� ����. =====


  _clock->draw(redraw);
}






void Display::_volume() {
  if(_volbar) _volbar->setValue(config.store.volume);
  #ifndef HIDE_VOL
    if(_voltxt) _voltxt->setText(config.store.volume, voltxtFmt);
  #endif
  if(_mode==VOL) {
    timekeeper.waitAndReturnPlayer(3);
    _nums->setText(config.store.volume, numtxtFmt);
  }
  /*#ifdef USE_NEXTION
    nextion.setVol(config.store.volume, _mode == VOL);
  #endif*/
}

void Display::flip(){ dsp.flip(); }

void Display::invert(){ dsp.invert(); }

void  Display::setContrast(){
  #if DSP_MODEL==DSP_NOKIA5110
    dsp.setContrast(config.store.contrast);
  #endif
}

bool Display::deepsleep(){
#if defined(LCD_I2C) || defined(DSP_OLED) || BRIGHTNESS_PIN!=255
  dsp.sleep();
  return true;
#endif
  return false;
}

void Display::wakeup(){
#if defined(LCD_I2C) || defined(DSP_OLED) || BRIGHTNESS_PIN!=255
  dsp.wake();
#endif
}

#ifndef DSP_LCD		////////////////////////////////// BATTERY ///////////////////////////////////////////
// ======== ��������������� ������� ��� �������� ����� � �������� �� ������� ==========
int Display::getRawPercent(float voltage) {
    if (voltage >= vs[20]) return 100; 		// 4.03V � ����
    if (voltage <= vs[0]) return 0; 			// 2.60V � ����

    for (uint8_t i = 0; i < 20; i++) {
        if (voltage >= vs[i] && voltage < vs[i + 1]) {
            float segmentRange = vs[i+1] - vs[i];
            float offsetInSegment = voltage - vs[i];
            // �������� ������������ ������ 5%-�� ����
            return (i * 5) + (int)round((offsetInSegment / segmentRange) * 5.0f);
        }
    }
    return 0;
}
// =================== ������ � ������ ���������� � ������ ==========================
void Display::readBattery() {
    // 1. ������ ��� � ����������� (100 �������) � ������������
    uint32_t sumMilliVolts = 0;
    // ������ �������� � ������������ �������� ����� API ����
    for(uint8_t i = 0; i < BAT_SAMPLES; i++) {
        sumMilliVolts += analogReadMilliVolts(ADC_PIN); 	// ���������� ���������� ���������� eFuse
    }

    //  �������� ��������� � ������� �� ������� ����� ��������
    float rawVolt = ((sumMilliVolts / (float)BAT_SAMPLES) / 1000.0f) * ((ADC_R1 + ADC_R2) / ADC_R2) + config.store.batoffset;
    if (rawVolt < 0.0f) rawVolt = 0.0f;

    // ���������� ��� ���������� � ���������� ������
    float instantDiff = abs(rawVolt - Volt);

    // �������������� ����������� ��������� (���� ��������� ����)
    // �� ���� 3.x ��� S3 � ESP32 ��� ������� ������ ��� �� ���������, ����� �������� �� � �������� ��������:
    //#ifdef ARDUINO_ESP32S3_DEV // S3 ������ ����� ������, ��������� ����� �� �������������
       // rawVolt = rawVolt * 1.02f; // ������ ������������, ���� ���� �� 2%
    //#endif

    // 2. ����������� ���������� (Fast Track ��� ������� > 80��, �������� "������� �������", EMA-������)
    if (instantDiff > 0.07f) {		// ������� Fast Track (�����������/���������� �������)
        Volt = rawVolt;			// ���������� ����� ������� ��� ������ (����/����� ������)
        lockTimer = 0; 			// ������� ����������, ��� ���� ������
    } 
    else if (instantDiff > 0.01f) {	// "������� ������" (���������� ������� - ���������, ����� � �.�.)
        Volt = rawVolt; 			// ��������� ���������� ���������, ����� ������������ ����� �����
        lockTimer = millis(); 		// ������ ���������� ��������� �� 5 ������
    }
    else {					// ������� ����� (������� EMA-������)
        Volt = (rawVolt * 0.1f) + (Volt * 0.9f);	// ������� ���������� ���� (10% ������, 90% �������)
    }

    // 3. �������� ��������� ������� (����� � ������) � ������ ����
    float diffV = Volt - chargingState.lastVoltage;	// ������� �� 1 ���. ����� ������� ���������� Volt � ���������� ���������������
    bool isLocked = (millis() - lockTimer < 5000); // �������� ����������

    // ���������� �������� (������ Fast Track ��� ��������� ���� ������)
    if (abs(diffV) > 0.07f) {
        bool oldCharging = Charging;
        Charging = (diffV > 0); 				// ������� ������� �� ������

       // ������ �������� �����: ��������� ��������
       if (Charging && !oldCharging) {
           startPercent = getRawPercent(chargingState.lastVoltage); 	// ����� ������ % �� ������
           voltAtStart = Volt; 						// ��������� ������� ����� ������ (���� ��� ���������)
           smoothDisplayLevel = (float)startPercent; 		// ������� �������� �������
       }
        chargingState.chargeCount = Charging ? 15 : 0;
        chargingState.dischargeCount = Charging ? 0 : 15;
    }
    else if (!isLocked) { 		// ������ ���������� ������ (������ ���� ��� ���������� �� "�������� ������" �� 5 ������
        if (diffV > 0.001f) { 			// ���� (�������)
            if (chargingState.chargeCount < 15) chargingState.chargeCount++;
            if (chargingState.dischargeCount > 0) chargingState.dischargeCount--;
        } else if (diffV < -0.001f) { 	// ������� (��������)
            if (chargingState.dischargeCount < 15) chargingState.dischargeCount++;
            if (chargingState.chargeCount > 0) chargingState.chargeCount--;
        }

        // ������������� ������ (5 ���������� �������� �����/�������)
        if (chargingState.chargeCount >= 5) Charging = true;
        if (chargingState.dischargeCount >= 5) Charging = false;
    }

    // ������������ ������
//    if (Volt > 4.12f) Charging = true; 
    if (Volt < 3.60f && diffV < 0) Charging = false;

    chargingState.lastVoltage = Volt; 	// ��������� ������� ���������� �������� ��� ���������� ���������

    // 4. ������ ������������ ��������� (smoothDisplayLevel)
    if (smoothDisplayLevel < 0) smoothDisplayLevel = (float)getRawPercent(Volt); 	// ���� ��� ������ ������ � ��������������

    if (Charging) {
        // ����� �������: ��������� �� (voltAtStart) �� (4.25V)
        float vRange = 4.25f - voltAtStart;		// ������� ��������: �� ����, ��� ���� �� �������, �� ��������� (4.25)
        if (vRange < 0.05f) vRange = 0.05f; 	// ������ ��� ����� ������ �������

        float vProgress = Volt - voltAtStart;
        float target = (float)startPercent + (vProgress / vRange) * (100.0f - (float)startPercent);

       if (target > 100.0f) target = 100.0f;
       if (target < (float)startPercent) target = (float)startPercent; // �� ���� ������ ���� ������

        // ������ ������ (0.1) ��� ������������ ����
        smoothDisplayLevel = (target * 0.1f) + (smoothDisplayLevel * 0.9f);
    } else {
        // --- �������� ---
        int rawPercent = getRawPercent(Volt);
        voltAtStart = 0; 		// ���������� ��������� ������� ��� ���������� ����� �������

        // ������������� ������: ���� ��������� ������, ����� � ��������
        float k = ((float)rawPercent < smoothDisplayLevel) ? 0.2f : 0.01f;
        smoothDisplayLevel = ((float)rawPercent * k) + (smoothDisplayLevel * (1.0f - k));
    }

    // 5. �������� ����������
    ChargeLevel = (uint8_t)round(smoothDisplayLevel);
    if (ChargeLevel > 100) ChargeLevel = 100;
}
/////////////////////////////// BATTERY SHOW //////////////////////////////////////////////
  void Display::BatteryShow() {
// ================= ��������� �������  � ��������� =====================
  char batbuf[5] = "";
  #ifdef PSFBUFFER
     uint16_t batcolor = 0;
     dsp.setTextSize(BatFS);		// ��������� TextSize
  #endif
  #ifdef DSP_OLED
     dsp.setTextSize(1);
  #endif
  if (Charging) {			// ���� ��� ������� - �������� ���������� - ���� ����������� (Cyan)
     #ifdef PSFBUFFER
        batcolor = dsp.color565(0, 255, 255); 			// ����������� (Cyan)
     #endif
     #ifdef DSP_OLED
//        dsp.setTextColor(config.theme.background, config.theme.clock); 		// (0%) ��������� ���������� �����
//        dsp.setTextColor(config.theme.clock, config.theme.background);			// �������
     #endif
     if (g == 1) strcpy(batbuf,"\xA0\xA2\x9E\x9F"); 				// 2 ����. � �����
     if (g == 2) strcpy(batbuf,"\xA0\x9E\x9E\xA3"); 				// 2 ����. �� �����
     if (g == 3) strcpy(batbuf,"\x9D\x9E\xA2\xA3"); 			// 2 ����. � ������
     if (g >= 4) {g = 0; strcpy(batbuf,"\x9D\xA2\xA2\x9F");} 		// 2 ����. � ��������
     g++;
  } else if (Volt < 2.8) { 	// ��������� ������� - �������� ���������� - ������� (Red)
     #ifdef PSFBUFFER
        batcolor = dsp.color565(255, 0, 0); 			// (0%) ��������� ���� ������� (Red)
     #endif
     #ifdef DSP_OLED
//        dsp.setTextColor(config.theme.background, config.theme.clock); 		// (0%) ��������� ���������� �����
        dsp.setTextColor(config.theme.clock, config.theme.background);			// �������
     #endif
     if (g == 1) strcpy(batbuf,"\xA0\xA2\xA2\xA3"); 				// ������ - 6 ��.
     if (g >= 2) {g = 0; strcpy(batbuf,"\x9D\x9E\x9E\x9F");} 		// ������ - 0 ��.
     g++;
  } else { 				// ����������� ���������
     #ifdef PSFBUFFER
       if (Volt >= 3.82)      {batcolor = dsp.color565(100, 255, 150); strcpy(batbuf, "\xA0\xA2\xA2\xA3"); }    //������ 85% (6 ����.) - ���.
       else if (Volt >= 3.72) {batcolor = dsp.color565(50, 255, 100); strcpy(batbuf, "\x9D\xA2\xA2\xA3"); }    //��70 �� 85% (5 ����.) - ���.
       else if (Volt >= 3.61) {batcolor = dsp.color565(0, 255, 0);     strcpy(batbuf, "\x9D\xA1\xA2\xA3"); }    //�� 55 �� 70% (4 ����.) - ���.
       else if (Volt >= 3.46) {batcolor = dsp.color565(75, 255, 0);    strcpy(batbuf, "\x9D\x9E\xA2\xA3"); }    //�� 40 �� 55% (3 ����.) - ���.
       else if (Volt >= 3.33) {batcolor = dsp.color565(150, 255, 0);   strcpy(batbuf, "\x9D\x9E\xA1\xA3"); }   //�� 25 �� 40% (2 ����.) - ���.
       else if (Volt >= 3.20) {batcolor = dsp.color565(255, 255, 0);   strcpy(batbuf, "\x9D\x9E\x9E\xA3"); } //�� 10 �� 25% (1 ����.) - ���.
       else if (Volt >= 2.8)  {batcolor = dsp.color565(255, 0, 0);     strcpy(batbuf, "\x9D\x9E\x9E\x9F"); }      //�� 0 �� 10% (0 ����.) - ����.
     #endif
     #ifdef DSP_OLED
//        dsp.setTextColor(config.theme.background, config.theme.clock); 		// (0%) ��������� ���������� �����
//        dsp.setTextColor(config.theme.clock, config.theme.background);			// �������
        if (Volt >= 3.82)      {strcpy(batbuf, "\xA0\xA2\xA2\xA3"); } 		//������ 85% (6 ����.)
        else if (Volt >= 3.72) {strcpy(batbuf, "\x9D\xA2\xA2\xA3"); } 		//��70 �� 85% (5 ����.)
        else if (Volt >= 3.61) {strcpy(batbuf, "\x9D\xA1\xA2\xA3"); } 		//�� 55 �� 70% (4 ����.)
        else if (Volt >= 3.46) {strcpy(batbuf, "\x9D\x9E\xA2\xA3"); } 		//�� 40 �� 55% (3 ����.)
        else if (Volt >= 3.33) {strcpy(batbuf, "\x9D\x9E\xA1\xA3"); } 		//�� 25 �� 40% (2 ����.)
        else if (Volt >= 3.20) {strcpy(batbuf, "\x9D\x9E\x9E\xA3"); } 		//�� 10 �� 25% (1 ����.)
        else if (Volt >= 2.8)  {strcpy(batbuf, "\x9D\x9E\x9E\x9F"); } 		//�� 0 �� 10% (0 ����.)
     #endif
  }
     #ifdef PSFBUFFER
        dsp.setTextColor(batcolor, config.theme.background);
     #endif
     dsp.setCursor(BatX, BatY);
     dsp.print(batbuf);

// ======= ����� �������� �������� ����������  �� ������� ===================
  #ifdef PSFBUFFER
     if (config.store.voltageon) {		// ========== ������ ������ ����������
        dsp.setTextSize(VoltFS); 			// ��������� TextSize
        dsp.setCursor(VoltX, VoltY); 		// ��������� ��������� ��� ������ ����������

        if(ROTATE_90){				// ���� ����� "ROTATE_90"
           char voltbuf[12];
           snprintf(voltbuf, sizeof(voltbuf), "%.3f", Volt);
           dsp.printf("Bat: %sv", voltbuf);		// ����� ���������� (������� ������)
        }else{

        #if DSP_MODEL==DSP_ST7735			// ������ ���  ������� ST7735
        if (player.status()!=PLAYING) {			// � ���� �� ����� "Play" � �� ����� "ROTATE_90"
    // ========================== ��������� ������������ ================================
            dsp.drawRect(8, 71, 4, 3, dsp.color565(192, 192, 192));
            dsp.drawRect(23, 71, 4, 3, dsp.color565(192, 192, 192));
            dsp.drawRect(5, 74, 25, 3, dsp.color565(192, 192, 192));
            dsp.drawRect(6, 76, 23, 17, dsp.color565(192, 192, 192));
            dsp.drawRect(5, 92, 25, 3, dsp.color565(192, 192, 192));
    // ======================= ����� �������� �������� ����������  �� ������� ==================
            char voltbuf[6];
            snprintf(voltbuf, sizeof(voltbuf), "%.2f", Volt);
            dsp.printf(voltbuf);				// ����� ���������� (������� ������)
        }
        #else
           char voltbuf[7];
           snprintf(voltbuf, sizeof(voltbuf), "%.3fv", Volt);
           dsp.printf(voltbuf);				// ����� ���������� (������� ������)
        #endif
        }
     } 				// =========== ����� ������ ����������

   // ======= ����� ��������� �������� ������ �� ������� =======================
     char procbuf[6];
   //  snprintf(procbuf, sizeof(procbuf), "%3i%%", ChargeLevel);		// ������ ������
   //  snprintf(procbuf, sizeof(procbuf), "%i%% ", ChargeLevel); 		// � �������� � ����� - ������ �����
     sprintf(procbuf, "%3i", ChargeLevel);		// ������ ������
     strlcat(procbuf, "%%", sizeof(procbuf));
     dsp.setTextSize(ProcFS); 					// setTextSize
     dsp.setCursor(ProcX, ProcY); 				// ��������� ��������� ��� ������
     dsp.printf(procbuf); 			// ����� ��������� ������ ������� (������� ������)
  #endif
  #ifdef DSP_OLED
     char procbuf[6];
     sprintf(procbuf, "%4i", ChargeLevel);		// ������ ������
     strlcat(procbuf, "%%", sizeof(procbuf));
//     dsp.setTextColor(config.theme.clock, config.theme.background);
     dsp.setCursor(ProcX, ProcY); 			// ��������� ��������� ��� ������ ��������� ������
     if (e == 1) dsp.printf(procbuf);	  		// ����� ��������� ������ ������� � ��������
     if (e > 2) {
        if (config.store.voltageon) {  			// ���� ���������� ���������
        char voltbuf[7];
        snprintf(voltbuf, sizeof(voltbuf), "%.2fv", Volt);
//       if (display.mode()!=PLAYER) {			// � ���� �� ����� "Player"
//       if (player.status()!=PLAYING) {			// � ���� �� ����� "Play"
//          dsp.setCursor(ProcX, ProcY);			// ��������� ��������� ��� ������ ����������
             dsp.printf(voltbuf);     /* } */			// ����� ����������
        }
     }							// ����� ������ ����������
     if (e > 3) {e = 0;}
     e++;
  #endif
  }
#endif				/////////////////////////////////////////////////////////////////////////////////
//============================================================================================================================
#else // !DUMMYDISPLAY
//============================================================================================================================
void Display::init(){
  _createDspTask();
  #ifdef USE_NEXTION
  nextion.begin(true);
  #endif
}
void Display::_start(){
  #ifdef USE_NEXTION
  //nextion.putcmd("page player");
  nextion.start();
  #endif
  config.setTitle(LANG::const_PlReady);
}

void Display::putRequest(displayRequestType_e type, int payload){
  if(type==DSP_START) _start();
  #ifdef USE_NEXTION
    requestParams_t request;
    request.type = type;
    request.payload = payload;
    nextion.putRequest(request);
  #else
    if(type==NEWMODE) mode((displayMode_e)payload);
  #endif
}
//============================================================================================================================
#endif // DUMMYDISPLAY