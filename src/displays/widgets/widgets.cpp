#include "../../core/options.h"
#if DSP_MODEL!=DSP_DUMMY
#include "../dspcore.h"
#include "Arduino.h"
#include "widgets.h"
#include "../../core/player.h"    //  for VU widget
#include "../../core/network.h"   //  for Clock widget
#include "../../core/config.h"
#include "../tools/l10n.h"
#include "../tools/psframebuffer.h"

/************************
      FILL WIDGET
 ************************/
void FillWidget::init(FillConfig conf, uint16_t bgcolor){
  Widget::init(conf.widget, bgcolor, bgcolor);
  _width = conf.width;
  _height = conf.height;
  
}

void FillWidget::_draw(){
  if(!_active) return;
  dsp.fillRect(_config.left, _config.top, _width, _height, _bgcolor);
}

void FillWidget::setHeight(uint16_t newHeight){
  _height = newHeight;
  //_draw();
}
/************************
      TEXT WIDGET
 ************************/
TextWidget::~TextWidget() {
  free(_text);
  free(_oldtext);
}

void TextWidget::_charSize(uint8_t textsize, uint8_t& width, uint16_t& height){
#ifndef DSP_LCD
  width = textsize * CHARWIDTH;
  height = textsize * CHARHEIGHT;
#else
  width = 1;
  height = 1;
#endif
}

void TextWidget::init(WidgetConfig wconf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor) {
  Widget::init(wconf, fgcolor, bgcolor);
  _buffsize = buffsize;
  _text = (char *) malloc(sizeof(char) * _buffsize);
  memset(_text, 0, _buffsize);
  _oldtext = (char *) malloc(sizeof(char) * _buffsize);
  memset(_oldtext, 0, _buffsize);
  _charSize(_config.textsize, _charWidth, _textheight);
  _textwidth = _oldtextwidth = _oldleft = 0;
  _uppercase = uppercase;
}

void TextWidget::setText(const char* txt) {
  strlcpy(_text, utf8Rus(txt, _uppercase), _buffsize);
  _textwidth = strlen(_text) * _charWidth;
  if (strcmp(_oldtext, _text) == 0) return;
  if (_active) dsp.fillRect(_oldleft == 0 ? _realLeft() : min(_oldleft, _realLeft()),  _config.top, max(_oldtextwidth, _textwidth), _textheight, _bgcolor);
  _oldtextwidth = _textwidth;
  _oldleft = _realLeft();
  if (_active) _draw();
}

void TextWidget::setText(int val, const char *format){
  char buf[_buffsize];
  snprintf(buf, _buffsize, format, val);
  setText(buf);
}

void TextWidget::setText(const char* txt, const char *format){
  char buf[_buffsize];
  snprintf(buf, _buffsize, format, txt);
  setText(buf);
}

uint16_t TextWidget::_realLeft(bool w_fb) {
  uint16_t realwidth = (_width>0 && w_fb)?_width:dsp.width();
  switch (_config.align) {
    case WA_CENTER: return (realwidth - _textwidth) / 2; break;
    case WA_RIGHT: return (realwidth - _textwidth - (!w_fb?_config.left:0)); break;
    default: return !w_fb?_config.left:0; break;
  }
}

void TextWidget::_draw() {
  if(!_active) return;
  dsp.setTextColor(_fgcolor, _bgcolor);
  dsp.setCursor(_realLeft(), _config.top);
  dsp.setFont();
  dsp.setTextSize(_config.textsize);
  dsp.print(_text);
  strlcpy(_oldtext, _text, _buffsize);
}

/************************
      SCROLL WIDGET
 ************************/
ScrollWidget::ScrollWidget(const char* separator, ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor) {
  init(separator, conf, fgcolor, bgcolor);
}

ScrollWidget::~ScrollWidget() {
  free(_fb);
  free(_sep);
  free(_window);
}

void ScrollWidget::init(const char* separator, ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor) {
  TextWidget::init(conf.widget, conf.buffsize, conf.uppercase, fgcolor, bgcolor);
  _sep = (char *) malloc(sizeof(char) * 4);
  memset(_sep, 0, 4);
  snprintf(_sep, 4, " %.*s ", 1, separator);
  _x = conf.widget.left;
  _startscrolldelay = conf.startscrolldelay;
  _scrolldelta = conf.scrolldelta;
  _scrolltime = conf.scrolltime;
  _charSize(_config.textsize, _charWidth, _textheight);
  _sepwidth = strlen(_sep) * _charWidth;
  _width = conf.width;
  _backMove.width = _width;
  _window = (char *) malloc(sizeof(char) * (MAX_WIDTH / _charWidth + 1));
  memset(_window, 0, (MAX_WIDTH / _charWidth + 1));  // +1?
  _doscroll = false;
  #ifdef PSFBUFFER
  _fb = new psFrameBuffer(dsp.width(), dsp.height());
  uint16_t _rl = (_config.align==WA_CENTER)?(dsp.width()-_width)/2:_config.left;
  _fb->begin(&dsp, _rl, _config.top, _width, _textheight, _bgcolor);
  #endif
}

void ScrollWidget::_setTextParams() {
  if (_config.textsize == 0) return;
  if(_fb->ready()){
  #ifdef PSFBUFFER
    _fb->setTextSize(_config.textsize);
    _fb->setTextColor(_fgcolor, _bgcolor);
  #endif
  }else{
    dsp.setTextSize(_config.textsize);
    dsp.setTextColor(_fgcolor, _bgcolor);
  }
}

bool ScrollWidget::_checkIsScrollNeeded() {
  return _textwidth > _width;
}

void ScrollWidget::setText(const char* txt) {
  strlcpy(_text, utf8Rus(txt, _uppercase), _buffsize - 1);
  if (strcmp(_oldtext, _text) == 0) return;
  _textwidth = strlen(_text) * _charWidth;
  _x = _fb->ready()?0:_config.left;
  _doscroll = _checkIsScrollNeeded();
  if (dsp.getScrollId() == this) dsp.setScrollId(NULL);
  _scrolldelay = millis();
  if (_active) {
    _setTextParams();
    if (_doscroll) {
      if(_fb->ready()){
      #ifdef PSFBUFFER
        _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
        _fb->setCursor(0, 0);
        snprintf(_window, _width / _charWidth + 1, "%s", _text); //TODO
        _fb->print(_window);
        _fb->display();
      #endif
      } else {
        dsp.fillRect(_config.left,  _config.top, _width, _textheight, _bgcolor);
        dsp.setCursor(_config.left, _config.top);
        snprintf(_window, _width / _charWidth + 1, "%s", _text); //TODO
        dsp.setClipping({_config.left, _config.top, _width, _textheight});
        dsp.print(_window);
        dsp.clearClipping();
      }
    } else {
      if(_fb->ready()){
      #ifdef PSFBUFFER
        _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
        _fb->setCursor(_realLeft(true), 0);
        _fb->print(_text);
        _fb->display();
      #endif
      } else {
        dsp.fillRect(_config.left, _config.top, _width, _textheight, _bgcolor);
        dsp.setCursor(_realLeft(), _config.top);
        //dsp.setClipping({_config.left, _config.top, _width, _textheight});
        dsp.print(_text);
        //dsp.clearClipping();
      }
    }
    strlcpy(_oldtext, _text, _buffsize);
  }
}

void ScrollWidget::setText(const char* txt, const char *format){
  char buf[_buffsize];
  snprintf(buf, _buffsize, format, txt);
  setText(buf);
}

void ScrollWidget::loop() {
  if(_locked) return;
  if (!_doscroll || _config.textsize == 0 || (dsp.getScrollId() != NULL && dsp.getScrollId() != this)) return;
  uint16_t fbl = _fb->ready()?0:_config.left;
  if (_checkDelay(_x == fbl ? _startscrolldelay : _scrolltime, _scrolldelay)) {
    _calcX();
    if (_active) _draw();
  }
}

void ScrollWidget::_clear(){
  if(_fb->ready()){
    #ifdef PSFBUFFER
    _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
    _fb->display();
    #endif
  } else {
    dsp.fillRect(_config.left, _config.top, _width, _textheight, _bgcolor);
  }
}

void ScrollWidget::_draw() {
  if(!_active || _locked) return;
  _setTextParams();
  if (_doscroll) {
    uint16_t fbl = _fb->ready()?0:_config.left;
    uint16_t _newx = fbl - _x;
    const char* _cursor = _text + _newx / _charWidth;
    uint16_t hiddenChars = _cursor - _text;
    uint8_t addChars = _fb->ready()?2:1;
    if (hiddenChars < strlen(_text)) {
    //TODO
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wformat-truncation="
      snprintf(_window, _width / _charWidth + addChars, "%s%s%s", _cursor, _sep, _text);
    #pragma GCC diagnostic pop
    } else {
      const char* _scursor = _sep + (_cursor - (_text + strlen(_text)));
      snprintf(_window, _width / _charWidth + addChars, "%s%s", _scursor, _text);
    }
    if(_fb->ready()){
    #ifdef PSFBUFFER
      _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
      _fb->setCursor(_x + hiddenChars * _charWidth, 0);
      _fb->print(_window);
      _fb->display();
    #endif
    } else {
      dsp.setCursor(_x + hiddenChars * _charWidth, _config.top);
      dsp.setClipping({_config.left, _config.top, _width, _textheight});
      dsp.print(_window);
      #ifndef DSP_LCD
        dsp.print(" ");
      #endif
      dsp.clearClipping();
    }
  } else {
    if(_fb->ready()){
    #ifdef PSFBUFFER
      _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
      _fb->setCursor(_realLeft(true), 0);
      _fb->print(_text);
      _fb->display();
    #endif
    } else {
      dsp.fillRect(_config.left, _config.top, _width, _textheight, _bgcolor);
      dsp.setCursor(_realLeft(), _config.top);
      dsp.setClipping({_realLeft(), _config.top, _width, _textheight});
      dsp.print(_text);
      dsp.clearClipping();
    }
  }
}

void ScrollWidget::_calcX() {
  if (!_doscroll || _config.textsize == 0) return;
  _x -= _scrolldelta;
  uint16_t fbl = _fb->ready()?0:_config.left;
  if (-_x > _textwidth + _sepwidth - fbl) {
    _x = fbl;
    dsp.setScrollId(NULL);
  } else {
    dsp.setScrollId(this);
  }
}

bool ScrollWidget::_checkDelay(int m, uint32_t &tstamp) {
  if (millis() - tstamp > m) {
    tstamp = millis();
    return true;
  } else {
    return false;
  }
}

void ScrollWidget::_reset(){
  dsp.setScrollId(NULL);
  _x = _fb->ready()?0:_config.left;
  _scrolldelay = millis();
  _doscroll = _checkIsScrollNeeded();
  #ifdef PSFBUFFER
  _fb->freeBuffer();
  uint16_t _rl = (_config.align==WA_CENTER)?(dsp.width()-_width)/2:_config.left;
  _fb->begin(&dsp, _rl, _config.top, _width, _textheight, _bgcolor);
  #endif
}

/************************
      SLIDER WIDGET
 ************************/
void SliderWidget::init(FillConfig conf, uint16_t fgcolor, uint16_t bgcolor, uint32_t maxval, uint16_t oucolor) {
  Widget::init(conf.widget, fgcolor, bgcolor);
  _width = conf.width; _height = conf.height; _outlined = conf.outlined; _oucolor = oucolor, _max = maxval;
  _oldvalwidth = _value = 0;
}

void SliderWidget::setValue(uint32_t val) {
  _value = val;
  if (_active && !_locked) _drawslider();

}

void SliderWidget::_drawslider() {
  uint16_t valwidth = map(_value, 0, _max, 0, _width - _outlined * 2);
  if (_oldvalwidth == valwidth) return;
  dsp.fillRect(_config.left + _outlined + min(valwidth, _oldvalwidth), _config.top + _outlined, abs(_oldvalwidth - valwidth), _height - _outlined * 2, _oldvalwidth > valwidth ? _bgcolor : _fgcolor);
  _oldvalwidth = valwidth;
}

void SliderWidget::_draw() {
  if(_locked) return;
  _clear();
  if(!_active) return;
  if (_outlined) dsp.drawRect(_config.left, _config.top, _width, _height, _oucolor);
  uint16_t valwidth = map(_value, 0, _max, 0, _width - _outlined * 2);
  dsp.fillRect(_config.left + _outlined, _config.top + _outlined, valwidth, _height - _outlined * 2, _fgcolor);
}

void SliderWidget::_clear() {
//  _oldvalwidth = 0;
  dsp.fillRect(_config.left, _config.top, _width, _height, _bgcolor);
}
void SliderWidget::_reset() {
  _oldvalwidth = 0;
}
/************************
      VU WIDGET		idea Witaliy76	https://github.com/Witaliy76/Yoradio_RGB_Panel
 ************************/
//VU-���� EXPERIMENTAL
static uint32_t holdTimeL = 1500; // ����� ��������� ���� ��� ������ ������ (��, ��� 500 ��)
static uint32_t holdTimeR = 1500; // ����� ��������� ���� ��� ������� ������ (��, ��� 500 ��)

#if !defined(DSP_LCD) && !defined(DSP_OLED)
VuWidget::~VuWidget() {
  if(_canvas) free(_canvas);
}

void VuWidget::init(WidgetConfig wconf, VUBandsConfig bands, uint16_t vumaxcolor, uint16_t vumincolor, uint16_t bgcolor) {
  Widget::init(wconf, bgcolor, bgcolor);
  _vumaxcolor = vumaxcolor;
  _vumidcolor = config.theme.vumid; 	//  �� �������
  _vumincolor = vumincolor;
  _bands = bands;
  _canvas = new Canvas(_bands.width * 2 + _bands.space, _bands.height);
//  _canvas = new Canvas(_config.align ? (_bands.width * 2 + _bands.space) : _bands.width, _bands.height);
}

void VuWidget::_draw(){
  if(!_active || _locked) return;
  static float measL = 0, measR = 0;
  static uint8_t peakHoldL = 0, peakHoldR = 0;
  static uint32_t peakHoldTimeL = 0, peakHoldTimeR = 0;
  uint8_t numBands = _bands.perheight;

float currentFade;
if (config.store.vufadespeed > 0) {
    currentFade = config.store.vufadespeed; // ���� � ���� ���-�� ��������� - ����� ���
} else {
    currentFade = _bands.fadespeed; // ���� � ������ ����� - ����� �� ������� �������
}

  uint16_t vulevel = player.getVUlevel();				// *******************************

// ************** ������� ���� ����������� �������� ************************************************
 // ����������� ���������� ��� �������� ��������� � ������������ ��������
  static float currentMax = 1.0f;			// ������� ������������ �������� (�� 1 �� 255)
  static float gain = 1.0f;				// ������� ����������� ��������
//  static uint32_t lastPeakTime = 0;		// ����� ���������� ������ ����

 // ��������� ����� �������� (0-255)
  uint8_t rawL = (vulevel >> 8) & 0xFF;
  uint8_t rawR = vulevel & 0xFF;
  
  // 1. ����� ���� � ������� �����
  uint8_t frameMax = (rawL > rawR) ? rawL : rawR;
  if (frameMax > currentMax) { currentMax = frameMax; }		// ���������� ������� �� ��������
  

  // 2. ������� �������� ���� (��������� �� ����-����) ����� ������� �� "�����" �� ���������, ��������� �������� currentMax
  if (currentMax > 1.0f) { currentMax -= 0.10f; }				//     ( currentMax -= 0.05f;) �������� ���������, ����� ����������

  // 3. ������� ������������ �������� ( ����~85% � 217 �� 255)
  // ������������ �������� (��������, �� ����� 5�), ����� � ������ �� ����� ����
//  float targetGain = 217.0f / (currentMax + 1.0f); 
  float targetGain = 240.0f / (currentMax + 1.0f); 
  if (targetGain > 5.0f) targetGain = 5.0f;
  if (targetGain < 1.0f) targetGain = 1.0f;

  // ������� ��������� ������ ������������ �������� (������� 3-4 �������)
//  gain = (gain * 0.98f) + (targetGain * 0.02f);
  gain = (gain * 0.90f) + (targetGain * 0.10f);

  // 4. ��������� �������� � ������������ ��� ���������� ������� (numBands)
  uint16_t boostedL = (uint16_t)(rawL * gain);
  uint16_t boostedR = (uint16_t)(rawR * gain);

  // ������������ ������, ����� �� ����� �� �������
  if (boostedL > 255) boostedL = 255;
  if (boostedR > 255) boostedR = 255;

  uint8_t activeL = numBands * boostedL / 255;
  uint8_t activeR = numBands * boostedR / 255;
// **************** ����� ���� ����������� �������� ****************************************************

  // --- Peak Hold ---
  // ��������� ������: ���� fading peakHold > 0 � ������ ������� �� ����� ��� ���� ��������, ���������� ���� � ������
  if (peakHoldL > 0 && activeL >= peakHoldL) {
    peakHoldL = activeL;
    peakHoldTimeL = millis();
  } else if (activeL > 0 && activeL > peakHoldL) {
    peakHoldL = activeL;
    peakHoldTimeL = millis();
  } else if (millis() - peakHoldTimeL > holdTimeL && peakHoldL > 0) {
    peakHoldL = 0;
  }
  if (peakHoldR > 0 && activeR >= peakHoldR) {
    peakHoldR = activeR;
    peakHoldTimeR = millis();
  } else if (activeR > 0 && activeR > peakHoldR) {
    peakHoldR = activeR;
    peakHoldTimeR = millis();
  } else if (millis() - peakHoldTimeR > holdTimeR && peakHoldR > 0) {
    peakHoldR = 0;
  }
  // --- ����� Peak Hold ---

  // ����� �����
    if (activeL >= measL) measL = activeL; 			// ��������� �����
    else {
      if (measL > currentFade * 0.1f) measL -= (currentFade * 0.1f); 	// ������� ������� ����
      else measL = 0;
      if (measL < activeL) measL = activeL;
    }
  // ������ �����
    if (activeR >= measR) measR = activeR;
    else {
      if (measR > currentFade * 0.1f) measR -= (currentFade * 0.1f);
      else measR = 0;
      if (measR < activeR) measR = activeR;
    }

  if(measL>numBands) measL=numBands;
  if(measR>numBands) measR=numBands;

  // ���� �������� VU-���� ������ fading peakHold � ����� peakHold
  if (measL >= peakHoldL) peakHoldL = measL;
  if (measR >= peakHoldR) peakHoldR = measR;

  // ������� �������
  _canvas->fillRect(0, 0, _bands.width * 2 + _bands.space, _bands.height, _bgcolor);

  // ����� � ������ ������ (��������)
  uint8_t redBands = std::max(2, numBands / 5);
  uint8_t BandsWidht = _bands.width / numBands - _bands.vspace;
  uint8_t BandsHeight = _bands.height / numBands - _bands.vspace;
  uint8_t spacecorrect = _bands.width - BandsWidht * numBands - _bands.vspace * (numBands-1);	// ��������� ������ ����������

   if(_config.align){											// ���� WA_CENTER ��� WA_RIGHT - �������������
      #ifndef BOOMBOX_STYLE															// Studio
         BandsWidht = _bands.width*2 / numBands - _bands.vspace;
            int yR = (_bands.height + _bands.space)/2 - 1;
         for (uint8_t i = 0; i < numBands; ++i) {
           int xLR = i * (BandsWidht + _bands.vspace);
           uint16_t colorLR = (i >= 4*redBands) ? _vumaxcolor : (i >= 3*redBands) ? _vumidcolor : _vumincolor;
           _canvas->fillRect(xLR, 0, BandsWidht, (_bands.height - _bands.space/2)/2, i < measL ? colorLR : _bgcolor);
           _canvas->fillRect(xLR, yR, BandsWidht, (_bands.height - _bands.space/2)/2, i < measR ? colorLR : _bgcolor);
         }
      #else																			// Boomboks
         for (uint8_t i = 0; i < numBands; ++i) {
           int xL = spacecorrect + (numBands-1 - i) * (BandsWidht + _bands.vspace);
           int xR = _bands.width + _bands.space + i * (BandsWidht + _bands.vspace);
           uint16_t colorLR = (i >= 4*redBands) ? _vumaxcolor : (i >= 3*redBands) ? _vumidcolor : _vumincolor;
           _canvas->fillRect(xL, 0, BandsWidht, _bands.height, i < measL ? colorLR : _bgcolor);
           _canvas->fillRect(xR, 0, BandsWidht, _bands.height, i < measR ? colorLR : _bgcolor);
         }
      #endif
   }else{													// ����� WA_LEFT - Standart (�����������)
           int xR = _bands.width + _bands.space;
         for (uint8_t i = 0; i < numBands; ++i) {
           int yLR = (numBands - i) * (BandsHeight + _bands.vspace);
           uint16_t colorLR = (i >= 4*redBands) ? _vumaxcolor : (i >= 3*redBands) ? _vumidcolor : _vumincolor;
           _canvas->fillRect(0, yLR, _bands.width, BandsHeight, i < measL ? colorLR : _bgcolor);
           _canvas->fillRect(xR, yLR, _bands.width, BandsHeight, i < measR ? colorLR : _bgcolor);
         }
   }

  // --- Peak Hold ������������ ---
  uint16_t peakColorL = (peakHoldL > 4*redBands) ? _vumaxcolor : (peakHoldL > 3*redBands) ? _vumidcolor : _vumincolor;
  uint16_t peakColorR = (peakHoldR > 4*redBands) ? _vumaxcolor : (peakHoldR > 3*redBands) ? _vumidcolor : _vumincolor;

  // Fading (���������)
  float fadeL = 1.0f - float(millis() - peakHoldTimeL) / holdTimeL;
  if (fadeL < 0.2f) fadeL = 0.2f;
  float fadeR = 1.0f - float(millis() - peakHoldTimeR) / holdTimeR;
  if (fadeR < 0.2f) fadeR = 0.2f;

  auto fadeColor = [](uint16_t color, float fade) -> uint16_t {
    uint8_t r = ((color >> 11) & 0x1F) * fade;
    uint8_t g = ((color >> 5) & 0x3F) * fade;
    uint8_t b = (color & 0x1F) * fade;
    return ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);
  };

  uint16_t fadedPeakColorL = fadeColor(peakColorL, fadeL);
  uint16_t fadedPeakColorR = fadeColor(peakColorR, fadeR);

  if(_config.align){																// ���� �������������
     #ifndef BOOMBOX_STYLE															// Studio
         BandsWidht = _bands.width*2 / numBands - _bands.vspace;
//         peakColorL = (peakHoldL < redBands) ? _vumaxcolor : (peakHoldL < 3*redBands) ? _vumidcolor : _vumincolor;
         if (peakHoldL > 0 && peakHoldL <= numBands) {
           int x = (peakHoldL-1) * (BandsWidht + _bands.vspace) + BandsWidht/2;
           _canvas->fillRect(x, 0, BandsWidht/2, (_bands.height-2)/2, fadedPeakColorL);
         }
         if (peakHoldR > 0 && peakHoldR <= numBands) {
           int x = (peakHoldR-1) * (BandsWidht + _bands.vspace) + BandsWidht/2;
           int yR = (_bands.height + _bands.space)/2 - 1;
           _canvas->fillRect(x, yR, BandsWidht/2, (_bands.height-2)/2, fadedPeakColorR);
         }
     #else																			// Boomboks
         if (peakHoldL > 0 && peakHoldL <= numBands) {
           int xL = spacecorrect + (numBands - peakHoldL+1) * (BandsWidht + _bands.vspace) - BandsWidht;
           _canvas->fillRect(xL, 0, BandsWidht/2, _bands.height, fadedPeakColorL);
         }
         if (peakHoldR > 0 && peakHoldR <= numBands) {
           int xR = _bands.width + _bands.space + (peakHoldR-1) * (BandsWidht + _bands.vspace) + BandsWidht/2;
           _canvas->fillRect(xR, 0, BandsWidht/2, _bands.height, fadedPeakColorR);
         }
     #endif
  }else{																		// ����� - Standart (�����������)
         if (peakHoldL > 0 && peakHoldL <= numBands) {
           int yL = (numBands - peakHoldL+1) * (BandsHeight + _bands.vspace) - BandsWidht;
           _canvas->fillRect(0, yL, _bands.width, BandsHeight/2, fadedPeakColorL);
         }
         if (peakHoldR > 0 && peakHoldR <= numBands) {
           int xR = _bands.width + _bands.space;
           int yR = (numBands - peakHoldR+1) * (BandsHeight + _bands.vspace) - BandsWidht;
           _canvas->fillRect(xR, yR, _bands.width, BandsHeight/2, fadedPeakColorR);
         }
  }

  // ���������� �������
  dsp.drawRGBBitmap(_config.left, _config.top, _canvas->getBuffer(), _bands.width * 2 + _bands.space, _bands.height);
}

void VuWidget::loop(){
  if(_active || !_locked) _draw();
}

void VuWidget::clearArea(){
  dsp.fillRect(0, _config.top, dsp.width(), _bands.height, _bgcolor);
}

void VuWidget::_clear(){
  dsp.fillRect(_config.left, _config.top, _bands.width * 2 + _bands.space, _bands.height, _bgcolor);
}
#else // DSP_LCD
VuWidget::~VuWidget() { }
void VuWidget::init(WidgetConfig wconf, VUBandsConfig bands, uint16_t vumaxcolor, uint16_t vumincolor, uint16_t bgcolor) {
  Widget::init(wconf, bgcolor, bgcolor);
}
void VuWidget::_draw(){ }
void VuWidget::loop(){ }
void VuWidget::clearArea(){ }
void VuWidget::_clear(){ }
#endif

/************************
      NUM & CLOCK
 ************************/
#if !defined(DSP_LCD)
  #if TIME_SIZE<19 //19->NOKIA
  const GFXfont* Clock_GFXfontPtr = nullptr;
  #define CLOCKFONT5x7
  #else
  const GFXfont* Clock_GFXfontPtr = &Clock_GFXfont;
  #endif
#endif //!defined(DSP_LCD)

#if !defined(CLOCKFONT5x7) && !defined(DSP_LCD)
  inline GFXglyph *pgm_read_glyph_ptr(const GFXfont *gfxFont, uint8_t c) {
    return gfxFont->glyph + c;
  }
  uint8_t _charWidth(unsigned char c){
    GFXglyph *glyph = pgm_read_glyph_ptr(&Clock_GFXfont, c - 0x20);
    return pgm_read_byte(&glyph->xAdvance);
  }
  uint16_t _textHeight(){
    GFXglyph *glyph = pgm_read_glyph_ptr(&Clock_GFXfont, '8' - 0x20);
    return pgm_read_byte(&glyph->height);
  }
#else //!defined(CLOCKFONT5x7) && !defined(DSP_LCD)
  uint8_t _charWidth(unsigned char c){
  #ifndef DSP_LCD
    return CHARWIDTH * TIME_SIZE;
  #else
    return 1;
  #endif
  }
  uint16_t _textHeight(){
    return CHARHEIGHT * TIME_SIZE;
  }
#endif
uint16_t _textWidth(const char *txt){
  uint16_t w = 0, l=strlen(txt);
  for(uint16_t c=0;c<l;c++) w+=_charWidth(txt[c]);
//  #if DSP_MODEL==DSP_ILI9225
//  return w+l;
//  #else
  return w;
//  #endif
}

/************************
      NUM WIDGET
 ************************/
NumWidget::~NumWidget() {
#ifdef PSFBUFFER
  delete _fb;
#endif
}

void NumWidget::init(WidgetConfig wconf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor) {
  Widget::init(wconf, fgcolor, bgcolor);
  _buffsize = buffsize;
  _text = (char *) malloc(sizeof(char) * _buffsize);
  memset(_text, 0, _buffsize);
  _oldtext = (char *) malloc(sizeof(char) * _buffsize);
  memset(_oldtext, 0, _buffsize);
  _textwidth = _oldtextwidth = _oldleft = 0;
  _uppercase = uppercase;
  _textheight = TIME_SIZE/*wconf.textsize*/;
#ifdef PSFBUFFER
  uint16_t maxDigitWidth = 0;
  for (char digit = '0'; digit <= '9'; digit++) {
    const char digitText[] = { digit, '\0' };
    maxDigitWidth = max(maxDigitWidth, _textWidth(digitText));
  }
  uint16_t availableWidth = dsp.width();
  if (_config.align != WA_CENTER) {
    availableWidth = _config.left < dsp.width() ? dsp.width() - _config.left : 0;
  }
  uint16_t maxDigits = maxDigitWidth && _buffsize > 1
      ? min(static_cast<uint16_t>(_buffsize - 1), static_cast<uint16_t>(availableWidth / maxDigitWidth))
      : 0;
  uint16_t fbWidth = maxDigitWidth * maxDigits;
  if (fbWidth > 0 && _textheight > 0) {
    _fb = new psFrameBuffer(fbWidth, _textheight);
    if (_config.align == WA_CENTER) {
      _fbLeft = (dsp.width() - fbWidth) / 2;
    } else if (_config.align == WA_RIGHT) {
      _fbLeft = dsp.width() - fbWidth - _config.left;
    } else {
      _fbLeft = _config.left;
    }
    _fb->begin(&dsp, _fbLeft, _config.top - _textheight, fbWidth, _textheight, _bgcolor);
  }
#endif
}

void NumWidget::setText(const char* txt) {
  strlcpy(_text, txt, _buffsize);
  _getBounds();
  if (strcmp(_oldtext, _text) == 0) return;
  uint16_t realth = _textheight;
#if defined(DSP_OLED) && DSP_MODEL!=DSP_SSD1322
  if(Clock_GFXfontPtr==nullptr) realth = _textheight * 8; //CHARHEIGHT
#endif
  if (_active
#ifdef PSFBUFFER
      && (!_fb || !_fb->ready())
#endif
  )
  #ifndef CLOCKFONT5x7
    dsp.fillRect(_oldleft == 0 ? _realLeft() : min(_oldleft, _realLeft()),  _config.top-_textheight+1, max(_oldtextwidth, _textwidth), realth, _bgcolor);
  #else
    dsp.fillRect(_oldleft == 0 ? _realLeft() : min(_oldleft, _realLeft()),  _config.top, max(_oldtextwidth, _textwidth), realth, _bgcolor);
  #endif

  _oldtextwidth = _textwidth;
  _oldleft = _realLeft();
  if (_active) _draw();
}

void NumWidget::setText(int val, const char *format){
  char buf[_buffsize];
  snprintf(buf, _buffsize, format, val);
  setText(buf);
}

void NumWidget::_getBounds() {
  _textwidth= _textWidth(_text);
}

void NumWidget::_draw() {
#ifndef DSP_LCD
  if(!_active || TIME_SIZE<2) return;
#endif
  if(!_active) return;
#ifdef PSFBUFFER
  if (_fb && _fb->ready()) {
    _fb->clear();
    _fb->setTextSize(Clock_GFXfontPtr == nullptr ? TIME_SIZE : 1);
    _fb->setFont(Clock_GFXfontPtr);
    _fb->setTextColor(_fgcolor, _bgcolor);
    _fb->setCursor(_realLeft() - _fbLeft, _textheight);
    _fb->print(_text);
    _fb->setFont();
    _fb->display();
    dsp.setFont();
    strlcpy(_oldtext, _text, _buffsize);
    return;
  }
#endif
#ifndef DSP_LCD
  dsp.setTextSize(Clock_GFXfontPtr==nullptr?TIME_SIZE:1);
  dsp.setFont(Clock_GFXfontPtr);
  dsp.setTextColor(_fgcolor, _bgcolor);
#endif
  dsp.setCursor(_realLeft(), _config.top);
  dsp.print(_text);
  strlcpy(_oldtext, _text, _buffsize);
  dsp.setFont();
}

/**************************
      PROGRESS WIDGET
 **************************/
void ProgressWidget::_progress() {
  char buf[_width + 1];
  snprintf(buf, _width, "%*s%.*s%*s", _pg <= _barwidth ? 0 : _pg - _barwidth, "", _pg <= _barwidth ? _pg : 5, ".....", _width - _pg, "");
  _pg++; if (_pg >= _width + _barwidth) _pg = 0;
  setText(buf);
}

bool ProgressWidget::_checkDelay(int m, uint32_t &tstamp) {
  if (millis() - tstamp > m) {
    tstamp = millis();
    return true;
  } else {
    return false;
  }
}

void ProgressWidget::loop() {
  if (_checkDelay(_speed, _scrolldelay)) {
    _progress();
  }
}

void BluetoothWidget::init(uint16_t fgcolor, uint16_t bgcolor){
  Widget::init({0, 0, 0, WA_CENTER}, fgcolor, bgcolor);
  _backgroundVisible = true;
  _activityBlinking = false;
  _activityStart = 0;
}

void BluetoothWidget::setBackgroundVisible(bool visible){
  if (_backgroundVisible == visible) return;
  _backgroundVisible = visible;
  if (_active && !_locked) _draw();
}

void BluetoothWidget::resetActivityBlink(){
  _activityBlinking = false;
  _activityStart = 0;
  setBackgroundVisible(true);
}

void BluetoothWidget::loop(){
#if defined(BLUETOOTH_ENABLED) && BLUETOOTH_ENABLED && defined(BLUETOOTH_ACTIVITY_PIN) && BLUETOOTH_ACTIVITY_PIN != 255
  const uint32_t now = millis();
  const bool activity = digitalRead(BLUETOOTH_ACTIVITY_PIN) == BLUETOOTH_ACTIVITY_ACTIVE_LEVEL;
  if (!activity) {
    _activityBlinking = false;
    setBackgroundVisible(true);
    return;
  }

  if (!_activityBlinking) {
    _activityBlinking = true;
    _activityStart = now;
    setBackgroundVisible(true);
    return;
  }

  const uint32_t phase = (now - _activityStart) % 1500;
  setBackgroundVisible(phase < 1000);
#endif
}

void BluetoothWidget::_draw(){
#ifndef DSP_LCD
  if(!_active) return;
  const int16_t side = min(dsp.width(), dsp.height());
  const int16_t iconHeight = side * 53 / 100;
  const int16_t iconWidth = iconHeight * 3 / 4;
  const uint8_t textSize = side / 75 < 1 ? 1 : (side / 75 > 3 ? 3 : side / 75);
  const int16_t textWidth = 9 * 6 * textSize;
  const int16_t markSize = max<uint8_t>(4, textSize * 2);
  const int16_t gap = side * 9 / 100;
  const int16_t contentHeight = iconHeight + gap + 8 * textSize;
  const int16_t iconTop = (dsp.height() - contentHeight) / 2;
  const int16_t iconLeft = (dsp.width() - iconWidth) / 2;
  const int16_t centerX = dsp.width() / 2;
  const int16_t centerY = iconTop + iconHeight / 2;
  const int16_t halfHeight = iconHeight * 43 / 100;
  const int16_t halfWidth = iconWidth * 30 / 100;
  dsp.fillRoundRect(iconLeft, iconTop, iconWidth, iconHeight, iconWidth / 2,
                    _backgroundVisible ? 0x2DB4 : _bgcolor);
  for(int8_t offset = -3; offset <= 3; ++offset){
    dsp.drawLine(centerX + offset, centerY - halfHeight, centerX + offset, centerY + halfHeight, _fgcolor);
    dsp.drawLine(centerX + offset, centerY, centerX + halfWidth + offset, centerY - halfHeight / 2, _fgcolor);
    dsp.drawLine(centerX + halfWidth + offset, centerY - halfHeight / 2, centerX + offset, centerY - halfHeight, _fgcolor);
    dsp.drawLine(centerX + offset, centerY, centerX + halfWidth + offset, centerY + halfHeight / 2, _fgcolor);
    dsp.drawLine(centerX + halfWidth + offset, centerY + halfHeight / 2, centerX + offset, centerY + halfHeight, _fgcolor);
    dsp.drawLine(centerX + offset, centerY, centerX - halfWidth + offset, centerY - halfHeight / 2, _fgcolor);
    dsp.drawLine(centerX + offset, centerY, centerX - halfWidth + offset, centerY + halfHeight / 2, _fgcolor);
  }

  const int16_t fullWidth = textWidth + markSize + 3;
  const int16_t textLeft = (dsp.width() - fullWidth) / 2;
  const int16_t textTop = iconTop + iconHeight + gap;
  dsp.setFont();
  dsp.setTextSize(textSize);
  dsp.setTextColor(_fgcolor, _bgcolor);
  dsp.setCursor(textLeft, textTop);
  dsp.print("Bluetooth");
  const int16_t markRadius = markSize / 2;
  const int16_t markCenterX = textLeft + textWidth + markRadius + 2;
  const int16_t markCenterY = textTop + markRadius;
  dsp.drawCircle(markCenterX, markCenterY, markRadius, _fgcolor);
  dsp.setTextSize(1);
  dsp.setCursor(markCenterX - 2, markCenterY - 4);
  dsp.print("R");
#endif
}

/**************************
      CLOCK WIDGET
 **************************/
void ClockWidget::init(WidgetConfig wconf, uint16_t fgcolor, uint16_t bgcolor){
  Widget::init(wconf, fgcolor, bgcolor);
  _timeheight = _textHeight();
  _fullclock = TIME_SIZE>35 || DSP_MODEL==DSP_ILI9225 || DSP_MODEL==DSP_ST7735;		// *****************
  if(_fullclock) _superfont = TIME_SIZE / 17; //magick
  else if(TIME_SIZE==19 || TIME_SIZE==2) _superfont=1;
  else _superfont=0;
  _space = (5*_superfont)/2; //magick
  #ifndef HIDE_DATE
  if(_fullclock){
    _dateheight = _superfont<3?1:2;								// *********************************
    _clockheight = _timeheight + _space + CHARHEIGHT * _dateheight;
  } else {
    _clockheight = _timeheight;
  }
  #else
    _clockheight = _timeheight;
  #endif
  _getTimeBounds();
#ifdef PSFBUFFER
  _fb = new psFrameBuffer(dsp.width(), dsp.height());
  _begin();
#endif
}

void ClockWidget::_begin(){
#ifdef PSFBUFFER
  _fb->begin(&dsp, _clockleft, _config.top-_timeheight, _clockwidth, _clockheight+1, config.theme.background);
#endif
}

bool ClockWidget::_getTime(){
  strftime(_timebuffer, sizeof(_timebuffer), "%H:%M", &network.timeinfo);
  bool ret = network.timeinfo.tm_sec==0 || _forceflag!=network.timeinfo.tm_year;
  _forceflag = network.timeinfo.tm_year;
  return ret;
}

uint16_t ClockWidget::_left(){
  if(_fb->ready()) return 0; else return _clockleft;
}
uint16_t ClockWidget::_top(){
  if(_fb->ready()) return _timeheight; else return _config.top;
}

void ClockWidget::_getTimeBounds() {
  _timewidth = _textWidth(_timebuffer);
  uint8_t fs = _superfont>0?_superfont:TIME_SIZE;
  uint16_t rightside = (DSP_MODEL==DSP_ST7735)?0:(CHARWIDTH * fs * 2); // seconds	// *************************
  if(_fullclock){
    rightside += (DSP_MODEL==DSP_ST7735)?0:(_space*2+1); //2space+vline	// *********************************
    _clockwidth = _timewidth+rightside;
  } else {
    if(_superfont==0)
      _clockwidth = _timewidth;
    else
      _clockwidth = _timewidth + rightside;
  }
  switch(_config.align){
    case WA_LEFT: _clockleft = _config.left; break;
    case WA_RIGHT: _clockleft = dsp.width()-_clockwidth-_config.left; break;
    default:
      _clockleft = (dsp.width()/2 - _clockwidth/2)+_config.left;
      break;
  }
  char buf[4];
  strftime(buf, 4, "%H", &network.timeinfo);
  _dotsleft=_textWidth(buf);
}

#ifndef DSP_LCD

Adafruit_GFX& ClockWidget::getRealDsp(){
#ifdef PSFBUFFER
  if (_fb && _fb->ready()) return *_fb;
#endif
  return dsp;
}

void ClockWidget::_printClock(bool force){
  auto& gfx = getRealDsp();
  gfx.setTextSize(Clock_GFXfontPtr==nullptr?TIME_SIZE:1);
  gfx.setFont(Clock_GFXfontPtr);
  bool clockInTitle=!config.isScreensaver && _config.top<_timeheight; //DSP_SSD1306x32

  if(force){
    _clearClock();
    _getTimeBounds();
    #ifndef DSP_OLED
    if(CLOCKFONT_MONO) {
      gfx.setTextColor(config.theme.clockbg, config.theme.background);
      gfx.setCursor(_left(), _top());
      gfx.print("88:88");
    }
    #endif
    if(clockInTitle)
      gfx.setTextColor(config.theme.meta, config.theme.metabg);
    else
      gfx.setTextColor(config.theme.clock, config.theme.background);
      gfx.setCursor(_left(), _top());
      gfx.print(_timebuffer);
      if(_fullclock){
      // lines, date & dow
      bool fullClockOnScreensaver = (!config.isScreensaver || (_fb->ready() && FULL_SCR_CLOCK));
      _linesleft = _left()+_timewidth+_space;
      if(fullClockOnScreensaver){
        gfx.setFont();
        gfx.setTextSize(_superfont);
   #if DSP_MODEL!=DSP_ST7735    
        gfx.drawFastVLine(_linesleft, _top()-_timeheight, _timeheight, config.theme.div);
        gfx.drawFastHLine(_linesleft, _top()-(_timeheight)/2, CHARWIDTH * _superfont * 2 + _space, config.theme.div);
        gfx.setCursor(_linesleft+_space+1, _top()-CHARHEIGHT * _superfont);
        gfx.setTextColor(config.theme.dow, config.theme.background);
        gfx.print(utf8Rus(LANG::dow[network.timeinfo.tm_wday], false));
   #endif
        sprintf(_tmp, "%2d %s %d", network.timeinfo.tm_mday,LANG::mnths[network.timeinfo.tm_mon], network.timeinfo.tm_year+1900);
        #ifndef HIDE_DATE
          strlcpy(_datebuf, utf8Rus(_tmp, false), sizeof(_datebuf));								// ********************************
          uint16_t _datewidth = strlen(_datebuf) * CHARWIDTH*_dateheight;
   #if DSP_MODEL==DSP_ST7735
            _datewidth += 30;
   #endif
          gfx.setTextSize(_dateheight);
          #if DSP_MODEL==DSP_GC9A01A
            int16_t dateLeft = (static_cast<int16_t>(dsp.width()) - static_cast<int16_t>(_datewidth)) / 2;
            #ifdef PSFBUFFER
              if (_fb && _fb->ready()) dateLeft -= _clockleft;
            #endif
            gfx.setCursor(dateLeft, _top() + _space);
          #else
            gfx.setCursor(_left()+(_clockwidth - _datewidth)/2, _top() + _space);						// ********************************
          #endif
          gfx.setTextColor(config.theme.date, config.theme.background);
          gfx.print(_datebuf);
   #if DSP_MODEL==DSP_ST7735
            gfx.setTextColor(config.theme.dow, config.theme.background);
            sprintf(_tmp, " (%s)", LANG::dow[network.timeinfo.tm_wday]);
            strlcpy(_datebuf, utf8Rus(_tmp, false), sizeof(_datebuf));
            gfx.print(_datebuf);
   #endif
        #endif
      }
    }
  }

  if((_fullclock || _superfont>0) && DSP_MODEL!=DSP_ST7735){								// ********************************
    gfx.setFont();
    gfx.setTextSize(_superfont);
    if(!_fullclock){
      #ifndef CLOCKFONT5x7
      gfx.setCursor(_left()+_timewidth+_space, _top()-_timeheight+_space);
      #else
      gfx.setCursor(_left()+_timewidth+_space, _top());
      #endif
    }else{
      gfx.setCursor(_linesleft+_space+1, _top()-_timeheight);
    }
    gfx.setTextColor(config.theme.seconds, config.theme.background);
    sprintf(_tmp, "%02d", network.timeinfo.tm_sec);
    gfx.print(_tmp);
  }

  gfx.setTextSize(Clock_GFXfontPtr==nullptr?TIME_SIZE:1);
  gfx.setFont(Clock_GFXfontPtr);
  #ifndef DSP_OLED
    gfx.setTextColor(dots ? config.theme.clock : (CLOCKFONT_MONO?config.theme.clockbg:config.theme.background), config.theme.background);
  #else
    if(clockInTitle)
      gfx.setTextColor(dots ? config.theme.meta:config.theme.metabg, config.theme.metabg);
    else
      gfx.setTextColor(dots ? config.theme.clock:config.theme.background, config.theme.background);
  #endif
  dots=!dots;
  gfx.setCursor(_left()+_dotsleft, _top());
  gfx.print(":");
  gfx.setFont();
  if(_fb->ready()) _fb->display();
}

void ClockWidget::_clearClock(){
#ifdef PSFBUFFER
  if(_fb->ready()) _fb->clear();
  else
#endif
#ifndef CLOCKFONT5x7
  dsp.fillRect(_left(), _top()-_timeheight, _clockwidth, _clockheight+1, config.theme.background);
#else
  dsp.fillRect(_left(), _top(), _clockwidth+1, _clockheight+1, config.theme.background);
#endif
}

void ClockWidget::clearDate(){
#if DSP_MODEL==DSP_GC9A01A && !defined(HIDE_DATE)
  if(!_active || config.isScreensaver || !_fullclock || !_datebuf[0]) return;
  uint16_t dateWidth = strlen(_datebuf) * CHARWIDTH * _dateheight;
  int16_t dateLeft = (static_cast<int16_t>(dsp.width()) - static_cast<int16_t>(dateWidth)) / 2;
  dsp.fillRect(dateLeft, _config.top + _space, dateWidth, CHARHEIGHT * _dateheight, config.theme.background);
#endif
}

void ClockWidget::draw(bool force){
  if(!_active) return;
  _printClock(_getTime() || force);
}

void ClockWidget::_draw(){
  if(!_active) return;
  _printClock(true);
}

void ClockWidget::_reset(){
#ifdef PSFBUFFER
  if(_fb->ready()) {
    _getTimeBounds();
    _fb->move(_clockleft, _config.top-_timeheight, _clockwidth, _clockheight+1, false);
  }
#endif
}

void ClockWidget::_clear(){
  _clearClock();
}
#else //#ifndef DSP_LCD

void ClockWidget::_printClock(bool force){
  strftime(_timebuffer, sizeof(_timebuffer), "%H:%M", &network.timeinfo);
  if(force){
    dsp.setCursor(dsp.width()-5, 0);
    dsp.print(_timebuffer);
  }
  dsp.setCursor(dsp.width()-5+2, 0);
  dsp.print((network.timeinfo.tm_sec % 2 == 0)?":":" ");
}

void ClockWidget::_clearClock(){}

void ClockWidget::clearDate(){}

void ClockWidget::draw(bool force){
  if(!_active) return;
  _printClock(true);
}
void ClockWidget::_draw(){
  if(!_active) return;
  _printClock(true);
}
void ClockWidget::_reset(){}
void ClockWidget::_clear(){}
#endif //#ifndef DSP_LCD

/**************************
      BITRATE WIDGET
 **************************/
void BitrateWidget::init(BitrateConfig bconf, uint16_t fgcolor, uint16_t bgcolor){
  Widget::init(bconf.widget, fgcolor, bgcolor);
  _dimension = bconf.dimension;
  _bitrate = 0;
  _format = BF_UNKNOWN;
  _charSize(bconf.widget.textsize, _charWidth, _textheight);
  memset(_buf, 0, 6);
}

void BitrateWidget::setBitrate(uint16_t bitrate){
  _bitrate = bitrate;
//  if(_bitrate>999) _bitrate=999;
  if(_bitrate > 20000)  _bitrate = _bitrate / 1000;
  _draw();
}

void BitrateWidget::setFormat(BitrateFormat format){
  _format = format;
  _draw();
}

//TODO move to parent
void BitrateWidget::_charSize(uint8_t textsize, uint8_t& width, uint16_t& height){
#ifndef DSP_LCD
  width = textsize * CHARWIDTH;
  height = textsize * CHARHEIGHT;
#else
  width = 1;
  height = 1;
#endif
}

// ���������� ���������� ��� ���������� ������� ����������� �� ������ _reset()
static uint16_t blockLastBitrate = 9999;
static BitrateFormat blockLastFormat = BF_UNKNOWN;
static bool blockForceFirstDraw = true;

void BitrateWidget::_draw(){
 
  if(!_active || player.status()!=PLAYING) return;

#if DSP_MODEL==DSP_GC9A01A
  // ===== GC9A01A: ����� + ������� � ���� ������, ��� ���������� =====
  const char* fmt = "";
  switch(_format){
    case BF_MP3:  fmt = "mp3"; break;
    case BF_AAC:  fmt = "aac"; break;
    case BF_FLAC: fmt = "flc"; break;
    case BF_WAV:  fmt = "wav"; break;
    case BF_OGG:  fmt = "ogg"; break;
    case BF_VOR:  fmt = "vor"; break;
    case BF_OPU:  fmt = "opu"; break;
    default: break;
  }

  char brbuf[8];
  if(_bitrate == 0) {
    snprintf(brbuf, sizeof(brbuf), "---");
  } else if(_bitrate < 999) {
    snprintf(brbuf, sizeof(brbuf), "%d", _bitrate);
  } else { 
    float _br = (float)_bitrate / 1000; 
    snprintf(brbuf, sizeof(brbuf), "%.1f", _br); 
  }

  char line[24];
  if(_format == BF_UNKNOWN || _bitrate == 0) {
    snprintf(line, sizeof(line), "--- --- ---   "); 
  } else {
    snprintf(line, sizeof(line), "%s %s KBS  ", fmt, brbuf);
  }

  dsp.setFont();
  dsp.setTextSize(_config.textsize);
  dsp.setTextColor(_fgcolor, _bgcolor);

  uint16_t tw = strlen(line) * _charWidth;
  uint16_t x;
  switch(_config.align){
    case WA_CENTER: x = (dsp.width() - tw) / 2; break;
    case WA_RIGHT:  x = dsp.width() - tw - _config.left; break;
    default:        x = _config.left; break;
  }
  dsp.setCursor(x, _config.top);
  dsp.print(line);

#else
  // ===== ST7789_170 � ��������� �������: ������� ��������� ��� �������� =====

  // ��������� ��������� ����� ������ �������, ����� ��������� ���������
  char codecBuf[5];
  if (player.status() != PLAYING) {
    strlcpy(codecBuf, "...", sizeof(codecBuf));
  } else {
    switch(_format){
      case BF_MP3:  strlcpy(codecBuf, "mp3", sizeof(codecBuf)); break;
      case BF_AAC:  strlcpy(codecBuf, "aac", sizeof(codecBuf)); break;
      case BF_FLAC: strlcpy(codecBuf, "flc", sizeof(codecBuf)); break;
      case BF_WAV:  strlcpy(codecBuf, "wav", sizeof(codecBuf)); break;
      case BF_OGG:  strlcpy(codecBuf, "ogg", sizeof(codecBuf)); break;
      case BF_VOR:  strlcpy(codecBuf, "vor", sizeof(codecBuf)); break;
      case BF_OPU:  strlcpy(codecBuf, "opu", sizeof(codecBuf)); break;
      default:      strlcpy(codecBuf, "...", sizeof(codecBuf)); break;
    }
  }

  // ������ �� ��������: ���� ������� � ����� �� ����������, 
  // ��������� ������� � �� ������� ������� ������ fillRect-��!
  if (!blockForceFirstDraw && _bitrate == blockLastBitrate && _format == blockLastFormat) {
    return; 
  }

  // ��������� �������� � ������
  blockLastBitrate = _bitrate;
  blockLastFormat = _format;
  blockForceFirstDraw = false;

  // 1. ������� ������� ���������� ������� ����� ���������� (��� �������) ������ ����
  dsp.fillRect(_config.left + 1, _config.top + 1, _dimension - 2, _dimension / 2 - 1, _bgcolor);

  // 2. ������� ������� ���������� ������ ����� ���������� (��� �����) ������ ����� (��������)
  dsp.fillRect(_config.left + 1, _config.top + _dimension / 2 + 1, _dimension - 2, _dimension / 2 - 2, _fgcolor);

  // ��������������� ����� � ����������� ������ ��������� ��������
  dsp.drawRect(_config.left, _config.top, _dimension, _dimension, _fgcolor);
  dsp.drawFastHLine(_config.left, _config.top + _dimension / 2, _dimension, _fgcolor);

  dsp.setFont();
  dsp.setTextSize(_config.textsize);
  
  // --- ��������� �������� (����) ---
  dsp.setTextColor(_fgcolor, _bgcolor);

  if (player.status() != PLAYING || _bitrate == 0) {
    snprintf(_buf, 6, "---");
  } else if(_bitrate < 999) {
    snprintf(_buf, 6, "%d", _bitrate);
  } else {
    float _br = (float)_bitrate / 1000;
    snprintf(_buf, 6, "%.1f", _br);
  }

  uint16_t activeLen = strlen(_buf);
  uint16_t startX = _config.left + _dimension / 2 - (_charWidth * activeLen) / 2 + 1;

  dsp.setCursor(startX, _config.top + _dimension / 4 - _textheight/2 + 2);
  dsp.print(_buf);

  // --- ��������� ������ (���) ---
  dsp.setTextColor(_bgcolor, _fgcolor);

  // ������ ������ X ��� ������
  uint16_t codecLen = strlen(codecBuf);
  uint16_t codecX = _config.left + _dimension / 2 - (_charWidth * codecLen) / 2 + 1;

  dsp.setCursor(codecX, _config.top + _dimension - _dimension / 4 - _textheight / 2);
  dsp.print(codecBuf);
#endif
}

void BitrateWidget::_clear() {
#if DSP_MODEL==DSP_GC9A01A
  // ��� GC9A01A � ������� ������� ������� ��� ������ "mp3 128"
  dsp.fillRect(_config.left, _config.top, dsp.width() - _config.left, _textheight, _bgcolor);
#else
  dsp.fillRect(_config.left, _config.top, _dimension*1.08, _dimension, _bgcolor);
#endif
}

void BitrateWidget::_reset() {
  blockForceFirstDraw = true;
  blockLastBitrate = 9999;
  blockLastFormat = BF_UNKNOWN;
}








/**************************
      PLAYLIST WIDGET
 **************************/
void PlayListWidget::init(ScrollWidget* current){
  Widget::init({0, 0, 0, WA_LEFT}, 0, 0);
  _current = current;
  #ifndef DSP_LCD
  _plItemHeight = playlistConf.widget.textsize*(CHARHEIGHT-1)+playlistConf.widget.textsize*4;
  _plTtemsCount = round((float)dsp.height()/_plItemHeight);
  if(_plTtemsCount%2==0) _plTtemsCount++;
  _plCurrentPos = _plTtemsCount/2;
  _plYStart = (dsp.height() / 2 - _plItemHeight / 2) - _plItemHeight * (_plTtemsCount - 1) / 2 + playlistConf.widget.textsize*2;
  #else
  _plTtemsCount = PLMITEMS;
  _plCurrentPos = 1;
  #endif
}

uint8_t PlayListWidget::_fillPlMenu(int from, uint8_t count) {
  int     ls      = from;
  uint8_t c       = 0;
  bool    finded  = false;
  if (config.playlistLength() == 0) {
    while (c < count) _printPLitem(c++, "");
    return c;
  }
  File playlist = config.SDPLFS()->open(REAL_PLAYL, "r");
  File index = config.SDPLFS()->open(REAL_INDEX, "r");
  while (true) {
    if (ls < 1) {
      ls++;
      _printPLitem(c, "");
      c++;
      continue;
    }
    if (!finded) {
      index.seek((ls - 1) * 4, SeekSet);
      uint32_t pos;
      index.readBytes((char *) &pos, 4);
      finded = true;
      index.close();
      playlist.seek(pos, SeekSet);
    }
    bool pla = true;
    while (pla) {
      pla = playlist.available();
      String stationName = playlist.readStringUntil('\n');
      stationName = stationName.substring(0, stationName.indexOf('\t'));
      if(config.store.numplaylist && stationName.length()>0) stationName = String(from+c)+" "+stationName;
      _printPLitem(c, stationName.c_str());
      c++;
      if (c >= count) break;
    }
    break;
  }
  while (c < count) _printPLitem(c++, "");
  playlist.close();
  return c;
}
#ifndef DSP_LCD
void PlayListWidget::drawPlaylist(uint16_t currentItem) {
  _fillPlMenu(currentItem - _plCurrentPos, _plTtemsCount);
}

void PlayListWidget::_printPLitem(uint8_t pos, const char* item){
  char line[BUFLEN];
  const uint16_t left = TFT_FRAMEWDT;
  const uint16_t charWidth = CHARWIDTH * playlistConf.widget.textsize;
  uint16_t maxChars = charWidth && dsp.width() > left
      ? (dsp.width() - left) / charWidth
      : 0;
  maxChars = min(maxChars, (uint16_t)(sizeof(line) - 1));
  const char* translatedItem = utf8Rus(item, false);
  uint16_t length = 0;
  while (length < maxChars && translatedItem[length]) {
    line[length] = translatedItem[length];
    length++;
  }
  while (length < maxChars) line[length++] = ' ';
  line[length] = '\0';

  dsp.setFont();
  dsp.setTextSize(playlistConf.widget.textsize);
  if (pos == _plCurrentPos) {
    dsp.setTextColor(config.theme.plcurrent, config.theme.plcurrentbg);
  } else {
    uint8_t plColor = (abs(pos - _plCurrentPos)-1)>4?4:abs(pos - _plCurrentPos)-1;
    dsp.setTextColor(config.theme.playlist[plColor], config.theme.background);
  }
  dsp.setCursor(left, _plYStart + pos * _plItemHeight);
  dsp.print(line);
}
#else
void PlayListWidget::_printPLitem(uint8_t pos, const char* item){
  if (pos == _plCurrentPos) {
    _current->setText(item);
  } else {
    dsp.setCursor(1, pos);
    char tmp[dsp.width()] = {0};
    strlcpy(tmp, utf8Rus(item, false), dsp.width());
    dsp.print(tmp);
  }
}

void PlayListWidget::drawPlaylist(uint16_t currentItem) {
  dsp.clear();
  _fillPlMenu(currentItem - _plCurrentPos, _plTtemsCount);
  dsp.setCursor(0,1);
  dsp.write(uint8_t(126));
}
#endif


#endif // #if DSP_MODEL!=DSP_DUMMY
