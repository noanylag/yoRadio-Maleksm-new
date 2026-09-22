/*************************************************************************************
    ILI9488 320X480 displays configuration file (ROTATE_90).
    Copy this file to yoRadio/src/displays/conf/displayILI9488conf_custom.h
    and modify it
    More info on https://github.com/e2002/yoradio/wiki/Widgets#widgets-description
*************************************************************************************/

#ifndef displayILI9488_R90conf_h
#define displayILI9488_R90conf_h

#define DSP_WIDTH       320
#define DSP_HEIGHT      480
#define TFT_FRAMEWDT    6
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2

/*#if BITRATE_FULL
  #define TITLE_FIX 48
#else
  #define TITLE_FIX 0
#endif*/
#define bootLogoTop     110

//  #define BATTERY_ON      config.store.batteryon			// Статус батарейки)
  #define BatX      DSP_WIDTH-50			// X coordinate for batt. (Координата X для батарейки)
  #define BatY      DSP_HEIGHT-73			// Y cordinate for batt. (Координата Y для батарейки)
  #define BatFS     2				// FontSize for batt. (Размер шрифта для батарейки)
  #define ProcX     DSP_WIDTH-102			// X coordinate for percent (Координата X для процентов заряда)
  #define ProcY     DSP_HEIGHT-73			// Y coordinate for percent (Координата Y для процентов заряда)
  #define ProcFS    2				// FontSize for percent (Размер шрифта для процентов заряда)
  #define VoltX      TFT_FRAMEWDT			// X coordinate for voltage (Координата X для напряжения)
  #define VoltY      DSP_HEIGHT-73			// Y coordinate for voltage (Координата Y для напряжения)
  #define VoltFS     2				// FontSize for voltage (Размер шрифта для напряжения)

/* SROLLS  */                       /* {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
const ScrollConfig metaConf   PROGMEM = {{ TFT_FRAMEWDT, TFT_FRAMEWDT+4, 4, WA_LEFT }, 140, false, MAX_WIDTH+15, 5000, 5, 40 };
const ScrollConfig title1Conf   PROGMEM = {{ TFT_FRAMEWDT, 63, 3, WA_LEFT }, 140, false, MAX_WIDTH, 5000, 5, 40 };
const ScrollConfig title2Conf    PROGMEM = {{ TFT_FRAMEWDT, 95, 3, WA_LEFT }, 140, false, MAX_WIDTH, 5000, 5, 40 };
const ScrollConfig playlistConf  PROGMEM = {{ TFT_FRAMEWDT, 226, 2, WA_LEFT }, 140, false, MAX_WIDTH, 1000, 5, 40 };
const ScrollConfig apTitleConf  PROGMEM = {{ TFT_FRAMEWDT, TFT_FRAMEWDT+4, 4, WA_CENTER }, 140, false, MAX_WIDTH, 0, 5, 40 };
const ScrollConfig apSettConf  PROGMEM = {{ TFT_FRAMEWDT, 420, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 5, 40 };
const ScrollConfig weatherConf    PROGMEM = {{ TFT_FRAMEWDT, 125, 2, WA_LEFT }, 250, false, MAX_WIDTH, 0, 5, 40 };  // Weather (погода)

/* BACKGROUNDS  */                       /* {{ left, top, fontsize, align }, width, height, outlined } */
const FillConfig   metaBGConf     PROGMEM = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 50, false };
const FillConfig   metaBGConfInv  PROGMEM = {{ 0, 50, 0, WA_LEFT }, DSP_WIDTH, 2, false };
const FillConfig   volbarConf     PROGMEM = {{ 0, (BATTERY_ON)?DSP_HEIGHT-25:DSP_HEIGHT-27, 0, WA_LEFT }, DSP_WIDTH, 12, true };
const FillConfig  playlBGConf    PROGMEM = {{ 0, 138, 0, WA_LEFT }, DSP_WIDTH, 36, false };
const FillConfig  heapbarConf   PROGMEM = {{ 0, DSP_HEIGHT-7, 0, WA_LEFT }, DSP_WIDTH, 6, false };

/* WIDGETS  */                           /* { left, top, fontsize, align } */
const WidgetConfig bootstrConf    PROGMEM = { 0, 235, 2, WA_CENTER };
const WidgetConfig bitrateConf    PROGMEM = { 0, (BATTERY_ON)?DSP_HEIGHT-48:DSP_HEIGHT-52, 2, WA_RIGHT };
const WidgetConfig voltxtConf     PROGMEM = { 180, (BATTERY_ON)?DSP_HEIGHT-48:DSP_HEIGHT-52, 2, WA_LEFT };
const WidgetConfig  iptxtConf     PROGMEM = { TFT_FRAMEWDT, (BATTERY_ON)?DSP_HEIGHT-48:DSP_HEIGHT-52, 2, WA_LEFT };
const WidgetConfig   rssiConf     PROGMEM = { TFT_FRAMEWDT-5, (BATTERY_ON)?DSP_HEIGHT-48-6:DSP_HEIGHT-52-6, 3, WA_RIGHT };
const WidgetConfig numConf        PROGMEM = { 0, 275, 0, WA_CENTER };
const WidgetConfig apNameConf     PROGMEM = { TFT_FRAMEWDT, 140, 3, WA_CENTER };
const WidgetConfig apName2Conf    PROGMEM = { TFT_FRAMEWDT, 180, 3, WA_CENTER };
const WidgetConfig apPassConf     PROGMEM = { TFT_FRAMEWDT, 260, 3, WA_CENTER };
const WidgetConfig apPass2Conf    PROGMEM = { TFT_FRAMEWDT, 300, 3, WA_CENTER };
const WidgetConfig  clockConf     PROGMEM = { TFT_FRAMEWDT, (BATTERY_ON)?245:250, 0, WA_RIGHT };  /* 70 is a fixed font size */
const WidgetConfig vuConf         PROGMEM = { TFT_FRAMEWDT/*+TITLE_FIX+2*/, (BATTERY_ON)?338:352, 1, WA_CENTER };
const WidgetConfig bootWdtConf    PROGMEM = { 0, 280, 2, WA_CENTER };
#ifdef CPU_LOAD
    const WidgetConfig  cpuConf   PROGMEM = { TFT_FRAMEWDT, (BATTERY_ON)?DSP_HEIGHT-48:DSP_HEIGHT-52, 2, WA_LEFT };
#endif

const ProgressConfig bootPrgConf  PROGMEM = { 90, 14, 4 };
const BitrateConfig fullbitrateConf PROGMEM = {{TFT_FRAMEWDT, (BATTERY_ON)?262:272, 3, WA_RIGHT}, 63 };

/* BANDS  */                             /* { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
const VUBandsConfig bandsConf     PROGMEM = { (MAX_WIDTH/*-TFT_FRAMEWDT-TITLE_FIX-2*/)/2, 50, TFT_FRAMEWDT, 1, (!BBOX)?20:15, 10 };	//Studio/Boombox

/* STRINGS  */
const char         numtxtFmt[]    PROGMEM = "%d";
const char           rssiFmt[]    PROGMEM = "WiFi %d";
const char          iptxtFmt[]    PROGMEM = "%s";
const char         voltxtFmt[]    PROGMEM = "\023\025%d";
//const char         voltxtFmt[]    PROGMEM = "";
const char        bitrateFmt[]    PROGMEM = "%d kBs";

/* MOVES  */                             /* { left, top, width (0 - auto, -1 - lock )} */
const MoveConfig    clockMove     PROGMEM = { TFT_FRAMEWDT, (BATTERY_ON)?245:250, -1 };
const MoveConfig   weatherMove    PROGMEM = { TFT_FRAMEWDT, 133, MAX_WIDTH };
const MoveConfig   weatherMoveVU  PROGMEM = { TFT_FRAMEWDT, 133, MAX_WIDTH };

#endif
