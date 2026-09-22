/*************************************************************************************
    ST7789 240x320 displays configuration file (ROTATE_90).
    Copy this file to yoRadio/src/displays/conf/displayST7789conf_custom.h
    and modify it
    More info on https://github.com/e2002/yoradio/wiki/Widgets#widgets-description
*************************************************************************************/

#ifndef displayST7789_R90conf_h
#define displayST7789_R90conf_h

#define DSP_WIDTH       240
#define DSP_HEIGTH       320
#define TFT_FRAMEWDT    4
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2

#if BITRATE_FULL
  #define TITLE_FIX 44
#else
  #define TITLE_FIX 0
#endif
#define bootLogoTop     68

//  #define BATTERY_ON      config.store.batteryon			// Статус батарейки)
  #define BatX      192			// X coordinate for batt. (Координата X для батарейки)
  #define BatY      262			// Y coordinate for batt. (Координата Y для батарейки)
  #define BatFS     2		// FontSize for batt. (Размер шрифта для батарейки)
  #define ProcX     142			// X coordinate for percent (Координата X для процентов заряда)
  #define ProcY     262			// Y coordinate for percent (Координата Y для процентов заряда)
  #define ProcFS    2		// FontSize for percent (Размер шрифта для процентов заряда)
  #define VoltX      TFT_FRAMEWDT	// X coordinate for voltage (Координата X для напряжения)
  #define VoltY      262			// Y coordinate for voltage (Координата Y для напряжения)
  #define VoltFS     2		// FontSize for voltage (Размер шрифта для напряжения)

/* SROLLS  */                            /* {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
const ScrollConfig metaConf    PROGMEM = {{ TFT_FRAMEWDT, TFT_FRAMEWDT+4, 3, WA_CENTER }, 140, false, DSP_WIDTH, 5000, 5, 30 };
const ScrollConfig title1Conf     PROGMEM = {{ TFT_FRAMEWDT, 44, 2, WA_LEFT }, 140, false, MAX_WIDTH, 5000, 4, 30 };
const ScrollConfig title2Conf     PROGMEM = {{ TFT_FRAMEWDT, 66, 2, WA_LEFT }, 140, false, MAX_WIDTH, 5000, 4, 30 };
const ScrollConfig playlistConf   PROGMEM = {{ TFT_FRAMEWDT, 152, 2, WA_LEFT }, 140, false, MAX_WIDTH, 1000, 4, 30 };
const ScrollConfig apTitleConf    PROGMEM = {{ TFT_FRAMEWDT, TFT_FRAMEWDT+4, 3, WA_CENTER }, 140, false, MAX_WIDTH, 0, 4, 20 };
const ScrollConfig apSettConf     PROGMEM = {{ TFT_FRAMEWDT, 280, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 4, 30 };
const ScrollConfig weatherConf    PROGMEM = {{ TFT_FRAMEWDT, 80, 2, WA_LEFT }, 250, false, MAX_WIDTH, 0, 4, 40 }; // Weather (погода)

/* BACKGROUNDS  */                       /* {{ left, top, fontsize, align }, width, height, outlined } */
const FillConfig   metaBGConf     PROGMEM = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 36, false };
const FillConfig   metaBGConfInv  PROGMEM = {{ TFT_FRAMEWDT, 34, 0, WA_LEFT }, MAX_WIDTH, 2, false };
const FillConfig   volbarConf     PROGMEM = {{ 0, (BATTERY_ON)?302:300, 0, WA_LEFT }, DSP_WIDTH, 10, true };
const FillConfig  playlBGConf     PROGMEM = {{ 0, 107, 0, WA_LEFT }, DSP_WIDTH, 24, false };
const FillConfig  heapbarConf     PROGMEM = {{ 0, 315, 0, WA_LEFT }, MAX_WIDTH, 5, false };

/* WIDGETS  */                           /* { left, top, fontsize, align } */
const WidgetConfig bootstrConf    PROGMEM = { 0, 188, 1, WA_CENTER };
const WidgetConfig bitrateConf    PROGMEM = { 155, 282,  1, WA_LEFT};
const WidgetConfig voltxtConf     PROGMEM = {  155, (BATTERY_ON)?282:278, 2, WA_LEFT };
const WidgetConfig  iptxtConf     PROGMEM = { TFT_FRAMEWDT, (BATTERY_ON)?282:278, 2, WA_LEFT };
const WidgetConfig   rssiConf     PROGMEM = { 0, (BATTERY_ON)?282:278, 2, WA_RIGHT };
const WidgetConfig numConf        PROGMEM = {  0, 185,  0, WA_CENTER };
const WidgetConfig apNameConf     PROGMEM = { TFT_FRAMEWDT, 100, 2, WA_CENTER };
const WidgetConfig apName2Conf    PROGMEM = { TFT_FRAMEWDT, 130, 2, WA_CENTER };
const WidgetConfig apPassConf     PROGMEM = { TFT_FRAMEWDT, 180, 2, WA_CENTER };
const WidgetConfig apPass2Conf    PROGMEM = { TFT_FRAMEWDT, 210, 2, WA_CENTER };
const WidgetConfig  clockConf     PROGMEM = { TFT_FRAMEWDT, (BATTERY_ON)?166:168, 0, WA_RIGHT };  /* 52 is a fixed font size */
const WidgetConfig vuConf         PROGMEM = { TITLE_FIX+2, (BATTERY_ON)?210:216, 1, WA_CENTER };		// VU горизонт
const WidgetConfig bootWdtConf    PROGMEM = { TFT_FRAMEWDT, 220, 2, WA_CENTER };
#ifdef CPU_LOAD
    const WidgetConfig  cpuConf   PROGMEM = { 170, (BATTERY_ON)?282:278, 1, WA_LEFT };
#endif

const ProgressConfig bootPrgConf  PROGMEM = { 100, 14, 4 };
const BitrateConfig fullbitrateConf PROGMEM = {{0, (BATTERY_ON)?210:216, 2, WA_RIGHT}, 42 };

/* BANDS  */                             /* { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
const VUBandsConfig bandsConf     PROGMEM = { (DSP_WIDTH-TITLE_FIX-2)/2, 42, TFT_FRAMEWDT, 1, (!BBOX)?20:15, 8 };	//Studio/Boombox

/* STRINGS  */
const char         numtxtFmt[]    PROGMEM = "%d";
const char           rssiFmt[]    PROGMEM = "WiFi %d";
//const char          iptxtFmt[]    PROGMEM = "\010 %s";
const char          iptxtFmt[]    PROGMEM = "%s";
//const char         voltxtFmt[]    PROGMEM = "\023\025%d";
const char         voltxtFmt[]    PROGMEM = "";
const char        bitrateFmt[]    PROGMEM = "%d kBs";
const char           cpuFmt[]    PROGMEM = "cpu:%d";

/* MOVES  */                             /* { left, top, width (0 - auto, -1 - lock )} */
const MoveConfig    clockMove     PROGMEM = { TFT_FRAMEWDT, (BATTERY_ON)?166:168, -1 };
const MoveConfig   weatherMove    PROGMEM = { TFT_FRAMEWDT, 88, MAX_WIDTH };
const MoveConfig   weatherMoveVU  PROGMEM = { TFT_FRAMEWDT, 88, MAX_WIDTH };

#endif
