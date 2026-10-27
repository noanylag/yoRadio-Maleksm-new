#line 1 "D:\\Maleksm\\yoRadio-1\\yoRadio\\src\\displays\\conf\\displayST7789_76conf.h"
/*************************************************************************************
    ST7789 284x76 displays configuration file.
    Copy this file to yoRadio/src/displays/conf/displayST7789conf_custom.h
    and modify it
    More info on https://github.com/e2002/yoradio/wiki/Widgets#widgets-description
*************************************************************************************/

#ifndef displayST7789_76conf_h
#define displayST7789_76conf_h

#define DSP_WIDTH       284
#define DSP_HEIGHT      76
#define TFT_FRAMEWDT    2
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2

#define bootLogoTop     8

//#define HIDE_HEAPBAR
#define HIDE_VOL
//#define HIDE_VU
//#define HIDE_TITLE2
//  #define RSSI_DIGIT true

//  #define BATTERY_ON      config.store.batteryon			// Статус батарейки)
  #define BatX      115		// X coordinate for batt. (Координата X для батарейки)
  #define BatY      50		// Y coordinate for batt. (Координата Y для батарейки)
  #define BatFS     1		// FontSize for batt. (Размер шрифта для батарейки)
  #define ProcX     140		// X coordinate for percent (Координата X для процентов заряда)
  #define ProcY     50		// Y coordinate for percent (Координата Y для процентов заряда)
  #define ProcFS    1		// FontSize for percent (Размер шрифта для процентов заряда)
  #define VoltX      128	// X coordinate for voltage (Координата X для напряжения)
  #define VoltY      40		// Y coordinate for voltage (Координата Y для напряжения)
  #define VoltFS     1		// FontSize for voltage (Размер шрифта для напряжения)

/* SROLLS  */                            /* {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
const ScrollConfig metaConf     PROGMEM = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 2, WA_LEFT }, 140, false, MAX_WIDTH, 5000, 2, 25 };
const ScrollConfig title1Conf     PROGMEM = {{ TFT_FRAMEWDT, 20, 1, WA_LEFT }, 140, false, DSP_WIDTH/2+23, 5000, 2, 25 };
const ScrollConfig title2Conf     PROGMEM = {{ TFT_FRAMEWDT, 30, 1, WA_LEFT }, 140, false, DSP_WIDTH/2+23, 5000, 2, 25 };
const ScrollConfig playlistConf   PROGMEM = {{ TFT_FRAMEWDT, 30, 1, WA_LEFT }, 140, false, MAX_WIDTH, 500, 2, 25 };
const ScrollConfig apTitleConf   PROGMEM = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 2, WA_CENTER }, 140, false, MAX_WIDTH, 0, 2, 25 };
const ScrollConfig apSettConf   PROGMEM = {{ TFT_FRAMEWDT+10, 64, 1, WA_LEFT }, 140, false, MAX_WIDTH, 0, 2, 25 };
const ScrollConfig weatherConf PROGMEM = {{ TFT_FRAMEWDT, DSP_HEIGHT-15, 1, WA_LEFT }, 250, false, MAX_WIDTH, 0, 2, 25 }; // Weather  (погода)

/* BACKGROUNDS  */                       /* {{ left, top, fontsize, align }, width, height, outlined } */
const FillConfig   metaBGConf     PROGMEM = {{ 0, 0,  0, WA_LEFT }, DSP_WIDTH, 18, false };
const FillConfig   metaBGConfInv  PROGMEM = {{ 0, 19, 0, WA_LEFT }, DSP_WIDTH, 1,  false };
const FillConfig   volbarConf     PROGMEM = {{ TFT_FRAMEWDT, DSP_HEIGHT-6, 0, WA_LEFT }, DSP_WIDTH-TFT_FRAMEWDT*2, 3, true };
const FillConfig  playlBGConf     PROGMEM = {{ 0, 26, 0, WA_LEFT }, DSP_WIDTH, 12, false };
const FillConfig  heapbarConf    PROGMEM = {{ 0, 74, 0, WA_LEFT }, DSP_WIDTH, 2, false };

/* WIDGETS  */                           /* { left, top, fontsize, align } */
const WidgetConfig bootstrConf  PROGMEM = { 0, DSP_HEIGHT-10, 1, WA_CENTER };
const WidgetConfig bitrateConf   PROGMEM = { TFT_FRAMEWDT+24, 50, 1, WA_LEFT };
//const WidgetConfig voltxtConf     PROGMEM = { TFT_FRAMEWDT, 108, 1, WA_RIGHT };
const WidgetConfig  iptxtConf     PROGMEM = { TFT_FRAMEWDT, 40, 1, WA_LEFT };
const WidgetConfig   rssiConf      PROGMEM = { TFT_FRAMEWDT, 50, 1, WA_LEFT };
const WidgetConfig numConf      PROGMEM = { TFT_FRAMEWDT, 56, 0, WA_CENTER };
const WidgetConfig apNameConf     PROGMEM = { 0, 21, 1, WA_CENTER };
const WidgetConfig apName2Conf    PROGMEM = { 0, 33, 1, WA_CENTER };
const WidgetConfig apPassConf    PROGMEM = { 0, 48, 1, WA_CENTER };
const WidgetConfig apPass2Conf  PROGMEM = { 0, 65, 1, WA_CENTER };
const WidgetConfig clockConf      PROGMEM = { 0, 56, 0, WA_RIGHT };  /* 35 is a fixed font size */
const WidgetConfig vuConf         PROGMEM = { TFT_FRAMEWDT, 60, 1, WA_CENTER };
#ifdef CPU_LOAD
    const WidgetConfig  cpuConf   PROGMEM = { TFT_FRAMEWDT, 40, 1, WA_LEFT };
#endif

const WidgetConfig bootWdtConf  PROGMEM = { 0, DSP_HEIGHT-23, 1, WA_CENTER };
const ProgressConfig bootPrgConf  PROGMEM = { 90, 10, 4 };

/* BANDS  */                  /* { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
const VUBandsConfig bandsConf     PROGMEM = { DSP_WIDTH/2-TFT_FRAMEWDT-4, 9, 2, 1, 20, 8 };

/* STRINGS  */
const char      numtxtFmt[]    PROGMEM = "%d";
const char      rssiFmt[]    PROGMEM = "%d";
const char      iptxtFmt[]    PROGMEM = "%s";
//const char       voltxtFmt[]    PROGMEM = "%d";
const char      bitrateFmt[]    PROGMEM = "%d kBs";

/* MOVES  */                             /* { left, top, width (0 - auto, -1 - lock )} */
const MoveConfig    clockMove     PROGMEM = { 0, 0, -1 };
const MoveConfig   weatherMove    PROGMEM = {TFT_FRAMEWDT, DSP_HEIGHT-15, MAX_WIDTH };
const MoveConfig   weatherMoveVU  PROGMEM = {TFT_FRAMEWDT, 40, DSP_WIDTH/2+23 };

#endif
