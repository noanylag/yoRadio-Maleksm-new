#ifndef _my_theme_h
#define _my_theme_h

/*
    Theming of color displays
    DSP_ST7735, DSP_ST7789, DSP_ILI9341, DSP_GC9106, DSP_ILI9225, DSP_ST7789_240
    ***********************************************************************
    *    !!! This file must be in the root directory of the sketch !!!    *
    ***********************************************************************
    Uncomment (remove double slash //) from desired line to apply color
    Exsample Color Table: https://colorscheme.ru/html-colors.html
*/
#define ENABLE_THEME
#ifdef  ENABLE_THEME

/*-----------------------------------------------------------------------------------------------*/
/*       | COLORS             |   values (0-255)  |                                              */
/*       | color name         |    R    G    B    |                                              */
/*-----------------------------------------------------------------------------------------------*/
#define COLOR_BACKGROUND     0,  0,  0
#define COLOR_STATION_NAME   106,  255,  185      //77,  255,  255
#define COLOR_STATION_BG    0,  0,  0
#define COLOR_STATION_FILL    0,  0,  0
#define COLOR_SNG_TITLE_1    255,  122,  0
#define COLOR_SNG_TITLE_2    255,  255,  0
#define COLOR_WEATHER    255,  0,  216
#define COLOR_VU_MAX    255,  0,  0
#define COLOR_VU_MIN  0,  255,  0
//#define COLOR_CLOCK    255,  255,  255
#define COLOR_CLOCK  254,241,0   //77,  255,  255 //255, 115, 132  //(255,85,99)
//#define COLOR_CLOCK    255,62,62
#define COLOR_CLOCK_BG   0,  0,  0
#define COLOR_SECONDS    254,241,0
#define COLOR_DAY_OF_W    255,  255,  0
#define COLOR_DATE    255,  255,  0
#define COLOR_BUFFER    157,  171,  251
#define COLOR_IP    144, 198, 255  //165, 162, 132 //144, 198, 255 //41,  189,  207
#define COLOR_RSSI   144, 198, 255 //0,  148,  255
#define COLOR_VOLBAR_OUT    255,  0,  0
#define COLOR_VOLBAR_IN    255,  255,  0
#define COLOR_DIGITS    231,  211,  90
#define COLOR_BITRATE    0,  255,  0   // 106,  255,  185  //#define COLOR_DIVIDER    0,  255,  0
//#define COLOR_BITRATE    231,  163,  146
#define COLOR_HEAP    255, 168, 162
#define COLOR_PL_CURRENT    0, 0, 0
//#define COLOR_PL_CURRENT_BG    91, 118, 255  /136,231,92
#define COLOR_PL_CURRENT_BG    111,231,71
#define COLOR_PL_CURRENT_FILL    149,231,159
#define COLOR_PLAYLIST_0    255, 255, 255
#define COLOR_PLAYLIST_1    255, 255, 255
#define COLOR_PLAYLIST_2    255, 255, 255
#define COLOR_PLAYLIST_3    255, 255, 255
#define COLOR_PLAYLIST_4    255, 255, 255
#define COLOR_VOLUME_VALUE    254, 241, 0    

#endif  /* #ifdef  ENABLE_THEME */
#endif  /* #define _my_theme_h  */
