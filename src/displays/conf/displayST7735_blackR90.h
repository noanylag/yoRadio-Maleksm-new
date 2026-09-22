/*************************************************************************************
    ST7735 128x160 displays configuration file (ROTATE_90).
    Copy this file to yoRadio/src/displays/conf/displayST7735conf_custom.h
    and modify it
    More info on https://github.com/e2002/yoradio/wiki/Widgets#widgets-description
*************************************************************************************/
#ifndef displayST7735_blackR90.h
#define displayST7735_blackR90.h

#define DSP_WIDTH       128
#define DSP_HEIGTH       160
#define TFT_FRAMEWDT    2
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2

#if BITRATE_FULL
  #define TITLE_FIX 21
#else
  #define TITLE_FIX 0
#endif
#define bootLogoTop     60

//#define HIDE_IP
//#define HIDE_TITLE2
//#define HIDE_HEAPBAR
//#define HIDE_VU

//  #define BATTERY_ON      config.store.batteryon			// Статус батарейки)
  #define BatX      78		// X coordinate for batt. (Координата X для батарейки)
  #define BatY      131		// Y coordinate for batt. (Координата Y для батарейки)
  #define BatFS     1		// FontSize for batt. (Размер шрифта для батарейки)
  #define ProcX     102	// X coordinate for percent (Координата X для процентов заряда)
  #define ProcY     131	// Y coordinate for percent (Координата Y для процентов заряда)
  #define ProcFS    1		// FontSize for percent (Размер шрифта для процентов заряда)
  #define VoltX      2		// X coordinate for voltage (Координата X для напряжения)
  #define VoltY      131	// Y coordinate for voltage (Координата Y для напряжения)
  #define VoltFS     1		// FontSize for voltage (Размер шрифта для напряжения)

/* SROLLS  */                            /* {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
const ScrollConfig metaConf     PROGMEM = {{ TFT_FRAMEWDT, TFT_FRAMEWDT+1, 2, WA_LEFT }, 140, false, MAX_WIDTH, 5000, 3, 30 };
const ScrollConfig title1Conf      PROGMEM = {{ TFT_FRAMEWDT, 24, 1, WA_LEFT }, 140, true, MAX_WIDTH, 5000, 3, 30 };
const ScrollConfig title2Conf      PROGMEM = {{ TFT_FRAMEWDT, 35, 1, WA_LEFT }, 140, true, MAX_WIDTH, 5000, 3, 30 };
const ScrollConfig playlistConf    PROGMEM = {{ 0, 56, 1, WA_LEFT }, 140, false, MAX_WIDTH, 1000, 4, 30 };
const ScrollConfig apTitleConf    PROGMEM = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 2, WA_CENTER }, 140, false, MAX_WIDTH, 0, 4, 30 };
const ScrollConfig apSettConf    PROGMEM = {{ TFT_FRAMEWDT, 128-TFT_FRAMEWDT-8, 1, WA_LEFT }, 140, false, MAX_WIDTH, 0, 3, 30 };
const ScrollConfig weatherConf  PROGMEM = {{ TFT_FRAMEWDT, 43, 1, WA_LEFT }, 250, false, MAX_WIDTH, 0, 3, 40 };	// Weather (погода)

/* BACKGROUNDS  */                       /* {{ left, top, fontsize, align }, width, height, outlined } */
const FillConfig  metaBGConf      PROGMEM = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 20, false };
const FillConfig  metaBGConfInv  PROGMEM = {{ 0, 20, 0, WA_LEFT }, DSP_WIDTH, 1, false };
const FillConfig  volbarConf         PROGMEM = {{ 0, (BATTERY_ON)?151:150, 0, WA_LEFT }, DSP_WIDTH, 5, true };
const FillConfig  playlBGConf       PROGMEM = {{ 0, 52, 0, WA_LEFT }, DSP_WIDTH, 22, false };
const FillConfig  heapbarConf      PROGMEM = {{ 0, 158, 0, WA_LEFT }, MAX_WIDTH, 2, false };

/* WIDGETS  */                           /* { left, top, fontsize, align } */
const WidgetConfig bootstrConf   PROGMEM = { 0, 110, 1, WA_CENTER };
const WidgetConfig bitrateConf    PROGMEM = { 0, 141, 1, WA_CENTER };
const WidgetConfig voltxtConf     PROGMEM = { TFT_FRAMEWDT, (BATTERY_ON)?141:139, 1, WA_LEFT };
const WidgetConfig  iptxtConf      PROGMEM = { TFT_FRAMEWDT, (BATTERY_ON)?141:139, 1, WA_LEFT };
const WidgetConfig  rssiConf       PROGMEM = { 0, (BATTERY_ON)?141:139, 1, WA_RIGHT };
const WidgetConfig numConf       PROGMEM = { 0, 86, 0, WA_CENTER };
const WidgetConfig apNameConf  PROGMEM = { 0, 40, 1, WA_CENTER };
const WidgetConfig apName2Conf PROGMEM = { 0, 54, 1, WA_CENTER };
const WidgetConfig apPassConf    PROGMEM = { 0, 74, 1, WA_CENTER };
const WidgetConfig apPass2Conf   PROGMEM = { 0, 88, 1, WA_CENTER };
const WidgetConfig  clockConf      PROGMEM = { 5, (BATTERY_ON)?92:93, 0, WA_RIGHT }; 		/* 35 is a fixed font size. */
const WidgetConfig vuConf          PROGMEM = { TITLE_FIX+2, (BATTERY_ON)?106:109, 1, WA_CENTER };
const WidgetConfig bootWdtConf  PROGMEM = { 0, 120, 1, WA_CENTER };
#ifdef CPU_LOAD
    const WidgetConfig  cpuConf   PROGMEM = { 0, (BATTERY_ON)?141:139, 1, WA_LEFT };
#endif

const ProgressConfig bootPrgConf  PROGMEM = { 90, 14, 4 };
const BitrateConfig fullbitrateConf   PROGMEM = {{0, (BATTERY_ON)?106:109, 1, WA_CENTER}, TITLE_FIX };

/* BANDS  */                             /* { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
//const VUBandsConfig bandsConf     PROGMEM = { (DSP_WIDTH-TITLE_FIX-2)/2, 22, TFT_FRAMEWDT, 1, 15, 8 };	//Studio/Boombox
const VUBandsConfig bandsConf     PROGMEM = {  55, 22, TFT_FRAMEWDT, 0, 15, 8 };	//Studio/Boombox

/* STRINGS  */
const char      numtxtFmt[]    PROGMEM = "%d";
const char           rssiFmt[]    PROGMEM = "rs:%d";
//const char          iptxtFmt[]    PROGMEM = "\010%s";
const char          iptxtFmt[]    PROGMEM = "%s";
//const char         voltxtFmt[]    PROGMEM = "\023\025%d";
const char         voltxtFmt[]    PROGMEM = "";
const char        bitrateFmt[]    PROGMEM = "%d";

/* MOVES  */                             /* { left, top, width (0 - auto, -1 - lock )} */
const MoveConfig    clockMove     PROGMEM = { 5, (BATTERY_ON)?92:93, 0};
const MoveConfig   weatherMove    PROGMEM = {TFT_FRAMEWDT, 46, MAX_WIDTH};
const MoveConfig   weatherMoveVU  PROGMEM = {TFT_FRAMEWDT, 46, MAX_WIDTH};

#endif
