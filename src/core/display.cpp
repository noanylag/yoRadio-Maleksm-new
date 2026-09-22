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
  // Константы для оптимизации
  #define BAT_SAMPLES 100       // Количество замеров для точности

  float ADC_R1 = R1;		// Номинал резистора на плюс (+)
  float ADC_R2 = R2;		// Номинал резистора на минус (-)
//  float DELTA = config.store.batoffset;	// Величина коррекции напряжения батареи

  uint8_t g, e = 1;			// Счётчики для мигалок и осреднений
  bool Charging = false;		// Признак, что подключено зарядное устройство
  float Volt = 0.0f; 			// Напряжение на батарее
  uint32_t lockTimer = 0;    	// Глобальная переменная для блокировки тренда определения зарядки
  uint8_t ChargeLevel;
  static float smoothDisplayLevel = -1.0f;
  static float voltAtStart = 0.0f;
  int startPercent = 0;     		// Процент, который был ДО зарядки

// ========= Массив напряжений на батарее, соответствующий проценту оставшегося заряда: 
  float vs[22] = {2.80, 3.10, 3.20, 3.26, 3.29, 3.33, 3.37, 3.41, 3.46, 3.51, 3.56, 3.61, 3.65, 3.69, 3.72, 3.75, 3.78, 3.82, 3.88, 3.95, 4.03, 4.25};

  // Структура для хранения состояния зарядки
  struct {
    float lastVoltage = 0;
    uint32_t lastCheck = 0;
    uint8_t chargeCount = 0;    // Счетчик последовательных увеличений напряжения
    uint8_t dischargeCount = 0; // Счетчик последовательных падений напряжения
  } chargingState;
#endif			/////////////////////////////////////////////////////////////////////////////////

#ifdef CPU_LOAD
// Объявляем текстовый виджет для отображения загрузки CPU
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

Page *pages[] = { new Page(), new Page(), new Page(), new Page() };

#if !((DSP_MODEL==DSP_ST7735 && DTYPE==INITR_BLACKTAB) || DSP_MODEL==DSP_ST7789 || DSP_MODEL==DSP_ST7796 || DSP_MODEL==DSP_ILI9488 \
 || DSP_MODEL==DSP_ILI9486 || DSP_MODEL==DSP_ILI9341 || DSP_MODEL==DSP_ILI9225 || DSP_MODEL==DSP_ST7789_170)
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
    // Инициализация ADC для батареи
    analogSetAttenuation(ADC_11db); // Установка диапазона

    // Серия быстрых замеров, чтобы "прогреть" АЦП и заполнить фильтр
    uint32_t sum = 0;
    for(int i=0; i<50; i++) sum += analogReadMilliVolts(ADC_PIN);

    float startVolt = ((sum / 50.0f) / 1000.0f) * ((ADC_R1 + ADC_R2) / ADC_R2) + config.store.batoffset;
    if (startVolt < 0.0f) startVolt = 0.0f;

    // Инициализация ключевой переменной текущим значением
    Volt = startVolt;

    // Состояние зарядки: фиксируем стартовую точку для отслеживания тренда
    chargingState.lastVoltage = startVolt;
    chargingState.chargeCount = 0;
    chargingState.dischargeCount = 0;

    lockTimer = millis();		// Запуск таймера блокировки счетчиков на 5 секунд
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
  // Добавляем CPU виджет в страницу плеера
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
    bootTitle->setText("С‘Radio AP Mode");
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
  if (newmode == _mode || (network.status != CONNECTED && network.status != SDREADY)) return;
  _mode = newmode;
  dsp.setScrollId(NULL);
  if (newmode == PLAYER) {
    if(player.isRunning())
      if(clockMove.width<0) _clock->moveBack(); else _clock->moveTo(clockMove);
    else
      _clock->moveBack();
    #ifdef DSP_LCD
      dsp.clearDsp();
    #endif
    numOfNextStation = 0;
    #ifdef META_MOVE
      _meta->moveBack();
    #endif
    _meta->setAlign(metaConf.widget.align);
    _meta->setText(config.station.name);
    _nums->setText("");
    config.isScreensaver = false;
    _pager->setPage( pages[PG_PLAYER]);
    config.setDspOn(config.store.dspon, false);
    pm.on_display_player();
  }
  if (newmode == SCREENSAVER || newmode == SCREENBLANK) {
    config.isScreensaver = true;
    _pager->setPage( pages[PG_SCREENSAVER]);
    if (newmode == SCREENBLANK) {
      //dsp.clearClock();
      _clock->clear();
      config.setDspOn(false, false);
    }
  }else{
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
      if(_vuwidget) _vuwidget->unlock();
      //_clock->moveTo(clockMove);
      if(clockMove.width<0) _clock->moveBack(); else _clock->moveTo(clockMove);
      if(_weather) _weather->moveTo(weatherMoveVU);
    }else{
      if(_vuwidget) if(!_vuwidget->locked()) _vuwidget->lock();
      _clock->moveBack();
      if(_weather) _weather->moveBack();
    }
  }else{
    if(played){
      if(clockMove.width<0) _clock->moveBack(); else _clock->moveTo(clockMove);
      if(_weather) _weather->moveTo(weatherMove);
      //_clock->moveBack();
    }else{
      if(_weather) _weather->moveBack();
      _clock->moveBack();
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

void Display::loop() {
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
            snprintf(buf, 20, bitrateFmt, config.station.bitrate); 
            if(_bitrate) { _bitrate->setText(config.station.bitrate==0?"":buf); } 
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
            _layoutChange(player.isRunning());
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
        case PSTART: _layoutChange(true);   break;
        case PSTOP:  _layoutChange(false);  break;
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
       if (millis() - lastBatteryUpdate >= 1000) { // Обновляем каждую секунду
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

  if(config.isScreensaver && network.timeinfo.tm_sec % 10 == 0){
    #if TIME_SIZE<19
//      uint16_t ft=static_cast<uint16_t>(random(TFT_FRAMEWDT, (dsp.height()-TIME_SIZE*CHARHEIGHT-TFT_FRAMEWDT)));
      uint16_t ft=static_cast<uint16_t>(random(0, (dsp.height()-TIME_SIZE*CHARHEIGHT)));
    #else
//      uint16_t ft=static_cast<uint16_t>(random(TFT_FRAMEWDT+TIME_SIZE, (dsp.height()-_clock->dateSize()-TFT_FRAMEWDT*2)));
//      uint16_t ft=static_cast<uint16_t>(random(TIME_SIZE, (dsp.height()-_clock->dateSize())));
      uint16_t ft=static_cast<uint16_t>(random(TIME_SIZE, dsp.height() - TFT_FRAMEWDT*2 - CHARHEIGHT*2));
    #endif
//    uint16_t lt=static_cast<uint16_t>(random(TFT_FRAMEWDT, (dsp.width()-_clock->clockWidth()-TFT_FRAMEWDT)));
    uint16_t lt=static_cast<uint16_t>(random(0, (dsp.width() - _clock->clockWidth())));
//    uint16_t lt=static_cast<uint16_t>(random(dsp.width() - _clock->clockWidth() - 10,  dsp.width() - _clock->clockWidth() + 3  ));
//    uint16_t lt = static_cast<uint16_t>(random(0,  (dsp.width() - (uint16_t)(TIME_SIZE*3.1f))));
    if(clockConf.align==WA_CENTER) lt-=(dsp.width()-_clock->clockWidth())/2;
//    if(ROTATE_90) lt = clockConf.left + lt + (uint16_t)(TIME_SIZE*3.1f) - dsp.width();
//    else                  lt = clockConf.left + lt - (dsp.width() - (uint16_t)(TIME_SIZE*3.1f))/2;
    //_clock->moveTo({clockConf.left, ft, 0});
    _clock->moveTo({lt, ft, 0});
  }
  _clock->draw(redraw);
  /*#ifdef USE_NEXTION
    nextion.printClock(network.timeinfo);
  #endif*/
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
// ======== Вспомогательная функция для перевода Вольт в проценты по таблице ==========
int Display::getRawPercent(float voltage) {
    if (voltage >= vs[20]) return 100; 		// 4.03V и выше
    if (voltage <= vs[0]) return 0; 			// 2.60V и ниже

    for (uint8_t i = 0; i < 20; i++) {
        if (voltage >= vs[i] && voltage < vs[i + 1]) {
            float segmentRange = vs[i+1] - vs[i];
            float offsetInSegment = voltage - vs[i];
            // Линейная интерполяция внутри 5%-го шага
            return (i * 5) + (int)round((offsetInSegment / segmentRange) * 5.0f);
        }
    }
    return 0;
}
// =================== Чтение и расчёт напряжений и заряда ==========================
void Display::readBattery() {
    // 1. Чтение АЦП с накоплением (100 замеров) в милливольтах
    uint32_t sumMilliVolts = 0;
    // Читаем значение в милливольтах напрямую через API ядра
    for(uint8_t i = 0; i < BAT_SAMPLES; i++) {
        sumMilliVolts += analogReadMilliVolts(ADC_PIN); 	// Используем встроенную калибровку eFuse
    }

    //  Получаем результат в вольтах на батарее через делитель
    float rawVolt = ((sumMilliVolts / (float)BAT_SAMPLES) / 1000.0f) * ((ADC_R1 + ADC_R2) / ADC_R2) + config.store.batoffset;
    if (rawVolt < 0.0f) rawVolt = 0.0f;

    // Переменные для фильтрации и блокировки тренда
    float instantDiff = abs(rawVolt - Volt);

    // Дополнительная программная коррекция (если заводской мало)
    // На ядре 3.x для S3 и ESP32 эти формулы обычно уже не требуются, лучше привести их к простому смещению:
    //#ifdef ARDUINO_ESP32S3_DEV // S3 обычно очень линеен, коррекция может не потребоваться
       // rawVolt = rawVolt * 1.02f; // Пример коэффициента, если врет на 2%
    //#endif

    // 2. Сглаживание напряжения (Fast Track для зарядки > 80мВ, детектор "Грязных скачков", EMA-фильтр)
    if (instantDiff > 0.07f) {		// Честный Fast Track (подключение/отключение зарядки)
        Volt = rawVolt;			// Мгновенный сброс фильтра при скачке (втык/вытык кабеля)
        lockTimer = 0; 			// Снимаем блокировку, это явно кабель
    } 
    else if (instantDiff > 0.01f) {	// "ГРЯЗНЫЙ СКАЧОК" (ПЕРЕХОДНЫЙ ПРОЦЕСС - подсветка, плеер и т.д.)
        Volt = rawVolt; 			// Обновляем напряжение мгновенно, чтобы пользователь видел цифры
        lockTimer = millis(); 		// Ставим блокировку счетчиков на 5 секунд
    }
    else {					// Обычный режим (плавный EMA-фильтр)
        Volt = (rawVolt * 0.1f) + (Volt * 0.9f);	// Плавное усреднение шума (10% нового, 90% старого)
    }

    // 3. Детектор состояния зарядки (Тренд и пороги) и захват Базы
    float diffV = Volt - chargingState.lastVoltage;	// Разница за 1 сек. между текущим сглаженным Volt и предыдущим зафиксированным
    bool isLocked = (millis() - lockTimer < 5000); // Проверка блокировки

    // Мгновенный детектор (Фильтр Fast Track уже пропустил этот скачок)
    if (abs(diffV) > 0.07f) {
        bool oldCharging = Charging;
        Charging = (diffV > 0); 				// Быстрая реакция на скачок

       // Момент втыкания шнура: фиксируем смещение
       if (Charging && !oldCharging) {
           startPercent = getRawPercent(chargingState.lastVoltage); 	// Берем чистый % до прыжка
           voltAtStart = Volt; 						// ФИКСИРУЕМ вольтаж после прыжка (База для схождения)
           smoothDisplayLevel = (float)startPercent; 		// Убираем задержку фильтра
       }
        chargingState.chargeCount = Charging ? 15 : 0;
        chargingState.dischargeCount = Charging ? 0 : 15;
    }
    else if (!isLocked) { 		// Анализ медленного тренда (только если нет блокировки от "Грязного скачка" на 5 секунд
        if (diffV > 0.001f) { 			// Рост (Зарядка)
            if (chargingState.chargeCount < 15) chargingState.chargeCount++;
            if (chargingState.dischargeCount > 0) chargingState.dischargeCount--;
        } else if (diffV < -0.001f) { 	// Падение (Разрядка)
            if (chargingState.dischargeCount < 15) chargingState.dischargeCount++;
            if (chargingState.chargeCount > 0) chargingState.chargeCount--;
        }

        // Подтверждение тренда (5 стабильных итераций роста/падения)
        if (chargingState.chargeCount >= 5) Charging = true;
        if (chargingState.dischargeCount >= 5) Charging = false;
    }

    // Страховочные пороги
//    if (Volt > 4.12f) Charging = true; 
    if (Volt < 3.60f && diffV < 0) Charging = false;

    chargingState.lastVoltage = Volt; 	// Сохраняем текущее сглаженное значение для следующего сравнения

    // 4. Расчет отображаемых процентов (smoothDisplayLevel)
    if (smoothDisplayLevel < 0) smoothDisplayLevel = (float)getRawPercent(Volt); 	// Если это первый запуск — инициализируем

    if (Charging) {
        // ШКАЛА ЗАРЯДКИ: Пропорция от (voltAtStart) до (4.25V)
        float vRange = 4.25f - voltAtStart;		// Считаем диапазон: от того, что было на клеммах, до максимума (4.25)
        if (vRange < 0.05f) vRange = 0.05f; 	// Защита для почти полной батареи

        float vProgress = Volt - voltAtStart;
        float target = (float)startPercent + (vProgress / vRange) * (100.0f - (float)startPercent);

       if (target > 100.0f) target = 100.0f;
       if (target < (float)startPercent) target = (float)startPercent; // Не даем падать ниже старта

        // Мягкий фильтр (0.1) для стабильности цифр
        smoothDisplayLevel = (target * 0.1f) + (smoothDisplayLevel * 0.9f);
    } else {
        // --- РАЗРЯДКА ---
        int rawPercent = getRawPercent(Volt);
        voltAtStart = 0; 		// Сбрасываем стартовый вольтаж для следующего цикла зарядки

        // Асимметричный фильтр: вниз реагируем быстро, вверх — медленно
        float k = ((float)rawPercent < smoothDisplayLevel) ? 0.2f : 0.01f;
        smoothDisplayLevel = ((float)rawPercent * k) + (smoothDisplayLevel * (1.0f - k));
    }

    // 5. Итоговое округление
    ChargeLevel = (uint8_t)round(smoothDisplayLevel);
    if (ChargeLevel > 100) ChargeLevel = 100;
}
/////////////////////////////// BATTERY SHOW //////////////////////////////////////////////
  void Display::BatteryShow() {
// ================= Отрисовка мигалок  и батарейки =====================
  char batbuf[5] = "";
  #ifdef PSFBUFFER
     uint16_t batcolor = 0;
     dsp.setTextSize(BatFS);		// Установка TextSize
  #endif
  #ifdef DSP_OLED
     dsp.setTextSize(1);
  #endif
  if (Charging) {			// Если идёт зарядка - бегающие квадратики - цвет Светлосиний (Cyan)
     #ifdef PSFBUFFER
        batcolor = dsp.color565(0, 255, 255); 			// Светлосиний (Cyan)
     #endif
     #ifdef DSP_OLED
//        dsp.setTextColor(config.theme.background, config.theme.clock); 		// (0%) установка инверсного цвета
//        dsp.setTextColor(config.theme.clock, config.theme.background);			// Светлый
     #endif
     if (g == 1) strcpy(batbuf,"\xA0\xA2\x9E\x9F"); 				// 2 квад. в конце
     if (g == 2) strcpy(batbuf,"\xA0\x9E\x9E\xA3"); 				// 2 квад. по краям
     if (g == 3) strcpy(batbuf,"\x9D\x9E\xA2\xA3"); 			// 2 квад. в начале
     if (g >= 4) {g = 0; strcpy(batbuf,"\x9D\xA2\xA2\x9F");} 		// 2 квад. в середине
     g++;
  } else if (Volt < 2.8) { 	// Требуется зарядка - мигающие квадратики - Красный (Red)
     #ifdef PSFBUFFER
        batcolor = dsp.color565(255, 0, 0); 			// (0%) установка цвет Красный (Red)
     #endif
     #ifdef DSP_OLED
//        dsp.setTextColor(config.theme.background, config.theme.clock); 		// (0%) установка инверсного цвета
        dsp.setTextColor(config.theme.clock, config.theme.background);			// Светлый
     #endif
     if (g == 1) strcpy(batbuf,"\xA0\xA2\xA2\xA3"); 				// полная - 6 кв.
     if (g >= 2) {g = 0; strcpy(batbuf,"\x9D\x9E\x9E\x9F");} 		// пустая - 0 кв.
     g++;
  } else { 				// Статическая батарейка
     #ifdef PSFBUFFER
       if (Volt >= 3.82)      {batcolor = dsp.color565(100, 255, 150); strcpy(batbuf, "\xA0\xA2\xA2\xA3"); }    //больше 85% (6 квад.) - зел.
       else if (Volt >= 3.72) {batcolor = dsp.color565(50, 255, 100); strcpy(batbuf, "\x9D\xA2\xA2\xA3"); }    //от70 до 85% (5 квад.) - зел.
       else if (Volt >= 3.61) {batcolor = dsp.color565(0, 255, 0);     strcpy(batbuf, "\x9D\xA1\xA2\xA3"); }    //от 55 до 70% (4 квад.) - зел.
       else if (Volt >= 3.46) {batcolor = dsp.color565(75, 255, 0);    strcpy(batbuf, "\x9D\x9E\xA2\xA3"); }    //от 40 до 55% (3 квад.) - зел.
       else if (Volt >= 3.33) {batcolor = dsp.color565(150, 255, 0);   strcpy(batbuf, "\x9D\x9E\xA1\xA3"); }   //от 25 до 40% (2 квад.) - зел.
       else if (Volt >= 3.20) {batcolor = dsp.color565(255, 255, 0);   strcpy(batbuf, "\x9D\x9E\x9E\xA3"); } //от 10 до 25% (1 квад.) - жёлт.
       else if (Volt >= 2.8)  {batcolor = dsp.color565(255, 0, 0);     strcpy(batbuf, "\x9D\x9E\x9E\x9F"); }      //от 0 до 10% (0 квад.) - крас.
     #endif
     #ifdef DSP_OLED
//        dsp.setTextColor(config.theme.background, config.theme.clock); 		// (0%) установка инверсного цвета
//        dsp.setTextColor(config.theme.clock, config.theme.background);			// Светлый
        if (Volt >= 3.82)      {strcpy(batbuf, "\xA0\xA2\xA2\xA3"); } 		//больше 85% (6 квад.)
        else if (Volt >= 3.72) {strcpy(batbuf, "\x9D\xA2\xA2\xA3"); } 		//от70 до 85% (5 квад.)
        else if (Volt >= 3.61) {strcpy(batbuf, "\x9D\xA1\xA2\xA3"); } 		//от 55 до 70% (4 квад.)
        else if (Volt >= 3.46) {strcpy(batbuf, "\x9D\x9E\xA2\xA3"); } 		//от 40 до 55% (3 квад.)
        else if (Volt >= 3.33) {strcpy(batbuf, "\x9D\x9E\xA1\xA3"); } 		//от 25 до 40% (2 квад.)
        else if (Volt >= 3.20) {strcpy(batbuf, "\x9D\x9E\x9E\xA3"); } 		//от 10 до 25% (1 квад.)
        else if (Volt >= 2.8)  {strcpy(batbuf, "\x9D\x9E\x9E\x9F"); } 		//от 0 до 10% (0 квад.)
     #endif
  }
     #ifdef PSFBUFFER
        dsp.setTextColor(batcolor, config.theme.background);
     #endif
     dsp.setCursor(BatX, BatY);
     dsp.print(batbuf);

// ======= Вывод цифровых значений напряжения  на дисплей ===================
  #ifdef PSFBUFFER
     if (config.store.voltageon) {		// ========== Начало вывода напряжения
        dsp.setTextSize(VoltFS); 			// Установка TextSize
        dsp.setCursor(VoltX, VoltY); 		// Установка координат для вывода напряжения

        if(ROTATE_90){				// если режим "ROTATE_90"
           char voltbuf[12];
           snprintf(voltbuf, sizeof(voltbuf), "%.3f", Volt);
           dsp.printf("Bat: %sv", voltbuf);		// Вывод напряжения (текущим цветом)
        }else{

        #if DSP_MODEL==DSP_ST7735			// Только для  дисплея ST7735
        if (player.status()!=PLAYING) {			// и если не режим "Play" и не режим "ROTATE_90"
    // ========================== Отрисовка аккумулятора ================================
            dsp.drawRect(8, 71, 4, 3, dsp.color565(192, 192, 192));
            dsp.drawRect(23, 71, 4, 3, dsp.color565(192, 192, 192));
            dsp.drawRect(5, 74, 25, 3, dsp.color565(192, 192, 192));
            dsp.drawRect(6, 76, 23, 17, dsp.color565(192, 192, 192));
            dsp.drawRect(5, 92, 25, 3, dsp.color565(192, 192, 192));
    // ======================= Вывод цифровых значений напряжения  на дисплей ==================
            char voltbuf[6];
            snprintf(voltbuf, sizeof(voltbuf), "%.2f", Volt);
            dsp.printf(voltbuf);				// Вывод напряжения (текущим цветом)
        }
        #else
           char voltbuf[7];
           snprintf(voltbuf, sizeof(voltbuf), "%.3fv", Volt);
           dsp.printf(voltbuf);				// Вывод напряжения (текущим цветом)
        #endif
        }
     } 				// =========== Конец вывода напряжения

   // ======= Вывод процентых значений заряда на дисплей =======================
     char procbuf[6];
   //  snprintf(procbuf, sizeof(procbuf), "%3i%%", ChargeLevel);		// формат вправо
   //  snprintf(procbuf, sizeof(procbuf), "%i%% ", ChargeLevel); 		// с пробелом в конце - формат влево
     sprintf(procbuf, "%3i", ChargeLevel);		// формат вправо
     strlcat(procbuf, "%%", sizeof(procbuf));
     dsp.setTextSize(ProcFS); 					// setTextSize
     dsp.setCursor(ProcX, ProcY); 				// Установка координат для вывода
     dsp.printf(procbuf); 			// Вывод процентов заряда батареи (текущим цветом)
  #endif
  #ifdef DSP_OLED
     char procbuf[6];
     sprintf(procbuf, "%4i", ChargeLevel);		// формат вправо
     strlcat(procbuf, "%%", sizeof(procbuf));
//     dsp.setTextColor(config.theme.clock, config.theme.background);
     dsp.setCursor(ProcX, ProcY); 			// Установка координат для вывода процентов заряда
     if (e == 1) dsp.printf(procbuf);	  		// Вывод процентов заряда батареи с форматом
     if (e > 2) {
        if (config.store.voltageon) {  			// Если напряжение выводится
        char voltbuf[7];
        snprintf(voltbuf, sizeof(voltbuf), "%.2fv", Volt);
//       if (display.mode()!=PLAYER) {			// и если не режим "Player"
//       if (player.status()!=PLAYING) {			// и если не режим "Play"
//          dsp.setCursor(ProcX, ProcY);			// Установка координат для вывода напряжения
             dsp.printf(voltbuf);     /* } */			// Вывод напряжения
        }
     }							// Конец вывода напряжения
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
