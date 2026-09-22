#ifndef AUDIOHANDLERS_H
#define AUDIOHANDLERS_H

//=============================================//
//              Audio handlers                 //
//=============================================//

#pragma once

#include <Arduino.h>
#include "../audioI2S/Audio.h"
#include "../displays/tools/l10n.h"
//#include "audiohelpers.h"

//#ifdef USE_NEXTION
//extern decltype(nextion) nextion;  // Nextion отображаемый объект (extern)
//#endif

// Глобальные или классовые переменные
String currentArtist = "";
String currentTitle = "";
String currentAlbum = "";
uint16_t currentStationId = static_cast<uint16_t>(-1);

void audio_info(Audio::msg_t m); 			// Обработка (прототип) функции INFO (заголовочный файл из Audio-library)
void processID3(const char *msg);
void audio_bitrate(const char *info);
bool printable(const char *info);
void audio_showstation(const char *info);
void audio_showstreamtitle(const char *info);
void audio_error(const char *info);
//void audio_id3artist(const char *info);
void audio_setTitleSafe(const char *info);
void audio_icy_description(const char *info);
void audio_beginSDread();
void audio_id3data(const char *info);
void audio_eof();
void audio_progress(uint32_t startpos, uint32_t endpos);
void seekSD();
void removeBOM(char *s);
bool cleanMeta(const char *src, char *dst, size_t dstSize);
void _utf8_clean(char *s);

/*     // callbacks ---------------------------------------------------------
  typedef enum {evt_info = 0, evt_id3data, evt_eof, evt_name, evt_icydescription, evt_streamtitle, evt_bitrate, evt_icyurl, evt_icylogo,
        evt_genre, evt_lasthost, evt_image, evt_lyrics, evt_log,} event_t;	*/

static void safeStrCopy(char *dst, const char *src, size_t dstSize) {
  if (!dst || !src || dstSize == 0) return;
  strlcpy(dst, src, dstSize);  // Убедитесь, что оно завершается нулевым символом.
}

/*  Если возникает событие, оно обрабатывается аудиобиблиотекой Schreibfaul1. Профессиональная, событийная версия: в основном работает на основе m.e (event_t), m.s используется только для отладки/совместимости. */

void audio_info(Audio::msg_t m) {
  // Безопасные строки для печати
  const char *s = (m.s != nullptr) ? m.s : "";
  const char *msg = (m.msg != nullptr) ? m.msg : "";

  // Вывод в последовательный порт (debug)
//  telnet.printf("##AUDIO.INFO#:  e:%d  %s: %s\n", static_cast<int>(m.e), s, msg);
  telnet.printf("##AUDIO.INFO#:  %s: %s\n", s, msg);

  if (player.lockOutput) return; 		// Если выходные данные заблокированы, ничего не обновляется.
  if (!msg) return; 				// Если нет сообщения, то обрабатывать нечего.
  if (strstr(msg, "Account already in use") != nullptr || strstr(msg, "HTTP/1.0 401") != nullptr) player.setError(msg);	// Типичные ошибки

  // --------------------------------------------------------------------
  // ОБРАБОТКА СОБЫТИЙ в зависимости от типа сообщения (на основе m.e)
  // --------------------------------------------------------------------
  switch (m.e) {

    // ----- Общая информация, формат, продолжительность SD-карты и т. д. -----
    case Audio::evt_info:
    {
      // Распознавание формата
      if (strstr(msg, "MPEG-1 Layer III") != nullptr) config.setBitrateFormat(BF_MP3);
      else if (strstr(msg, "MP3") != nullptr) config.setBitrateFormat(BF_MP3);
      else if (strstr(msg, "AAC") != nullptr) config.setBitrateFormat(BF_AAC);
      else if (strstr(msg, "M4A") != nullptr) config.setBitrateFormat(BF_AAC);
      else if (strstr(msg, "FLAC") != nullptr) config.setBitrateFormat(BF_FLAC);
      else if (strstr(msg, "WAV") != nullptr) config.setBitrateFormat(BF_WAV);
      else if (strstr(msg, "OGG") != nullptr) config.setBitrateFormat(BF_OGG);
      else if (strstr(msg, "VORBIS") != nullptr) config.setBitrateFormat(BF_VOR);
      else if (strstr(msg, "OPUS") != nullptr) config.setBitrateFormat(BF_OPU);

      // Режим SD: "готовность к потоковой передаче" → перейти к сохраненной позиции
      if (strstr(msg, "stream ready") != nullptr) { seekSD();  }
      // Режим SD: Audio-Data-Start
      else if (strstr(msg, "Audio-Data-Start:") != nullptr) { player.sd_min = atoi(msg + strlen("Audio-Data-Start:")); }
      // Режим SD: Общая продолжительность (длительность аудиозаписи:)
      else if (strstr(msg, "Audio-Length:") != nullptr) {
        uint32_t audioLength = static_cast<uint32_t>(atoi(msg + strlen("Audio-Length:")));
        player.sd_max = player.sd_min + audioLength;
        netserver.requestOnChange(SDLEN, 0);  // Отправка диапазона ползунка в веб-браузер.
      }
    display.putRequest(DBITRATE);
    } break;

     case Audio::evt_id3data: {			// ----- ID3 метаданные (MP3) -----
      audio_id3data(msg);
      // Идентификатор станции определяется по номеру трека (важно для воспроизведения в стандартном разрешении).
      if (strstr(msg, "Track:") != nullptr) {
        currentStationId = static_cast<uint16_t>(atoi(msg + strlen("Track:")));
      }
      char metaBuf[BUFLEN];
      if (cleanMeta(msg, metaBuf, sizeof(metaBuf))) {
        processID3(metaBuf);	// processID3 извлекает строки "Исполнитель" / "Название".
      }
    } break;

   case Audio::evt_bitrate: {			// ----- сообщение битрейта -----
      audio_bitrate(msg);	
    } break;

    case Audio::evt_streamtitle: {		// ----- Stream title (ICY) -----
      char metaBuf[BUFLEN];
      if (cleanMeta(msg, metaBuf, sizeof(metaBuf))) audio_setTitleSafe(metaBuf);
    } break;

    case Audio::evt_icydescription: {		// ----- ICY description -----
//        audio_icy_description(msg);
        char metaBuf[BUFLEN];
        if (cleanMeta(msg, metaBuf, sizeof(metaBuf))) audio_icy_description(metaBuf);
        else audio_icy_description(msg);
    } break;

    case Audio::evt_genre: {			// ----- ICY genre -----
//        audio_icy_genre(msg);
    } break;

    case Audio::evt_name: {			// ----- Чтение названия станции. (station_name) -----
      char metaBuf[BUFLEN];
      if (cleanMeta(msg, metaBuf, sizeof(metaBuf))) {
        #ifdef NAME_STRIM 			// Если показывать имя станции из стрима
          config.setStation(metaBuf);
          display.putRequest(NEWSTATION);
          netserver.requestOnChange(STATION, 0);
        #else
//          audio_setTitleSafe(metaBuf);
          audio_icy_description(metaBuf);
       #endif
      }
      else audio_icy_description(msg);
    } break;

    case Audio::evt_icyurl: {			// ----- ICY URL -----
    } break;

    case Audio::evt_image: {			// ----- Изображение / обложка (APIC) -----
    } break;

    case Audio::evt_eof: {			// ----- Конец файла (режим SD) -----
       audio_eof(); config.vuThreshold =0;
    } break;

    case Audio::evt_log: {			// ----- Журналы событий (ошибки, диагностика) -----
    } break;

    // ----- Неиспользуемые / вновь поступающие события -----
    case Audio::evt_lasthost:
    case Audio::evt_icylogo:
    case Audio::evt_lyrics:
    default:
      // Пока специальная обработка не предусмотрена, но она видна в журнале отладки.
      break;
  }
  // --------------------------------------------------------------------
  // Дополнительная текстовая интерпретация (на основе содержимого m.msg), которая не привязана строго к типу события.
  // --------------------------------------------------------------------
    const char *ici = strstr(msg, "icy-name: ");			// icy-name: --> Многие радиостанции передают имя в таком виде.
    if (ici != nullptr) {
      char icyName[BUFLEN] = {0};
      safeStrCopy(icyName, ici + strlen("icy-name: "), sizeof(icyName));
      char metaBuf[BUFLEN];
      if (cleanMeta(icyName, metaBuf, sizeof(metaBuf)) && strlen(metaBuf) > 0) {
        #ifdef NAME_STRIM 			// Если показывать имя станции из стрима
          config.setStation(metaBuf);
          display.putRequest(NEWSTATION);
          netserver.requestOnChange(STATION, 0);
        #else
//          audio_setTitleSafe(metaBuf);
          audio_icy_description(msg);
        #endif
      }
    }

}

/************************************* */
/******* Функции по внешнему вызову ******/
/************************************* */

/* Если остановить воспроизведение музыки в режиме SD и перезапустить его, воспроизведение начнётся с начала. Когда появится сообщение «поток готов», следует вернуться к сохранённой точке остановки. */
void seekSD() {
  if (config.getMode() == PM_SDCARD && config.sdResumePos > 0) {
    if (currentStationId == -1) {
      uint32_t offset = 0;
      if (config.sdResumePos > player.sd_min) {
        offset = config.sdResumePos - player.sd_min;
      }
      player.setAudioFilePosition(offset);
    }
  }
}

void processID3(const char *msg) {
  bool updated = false;
  bool updatedAlbum = false;
  if (!msg) return;

  if ((strstr(msg, "Album") == msg) || (strstr(msg, "ALBUM") == msg) || (strstr(msg, "album") == msg)) {
    String s = String(msg).substring(6); 	// "Album:" длина = 6
    s.trim();
    currentAlbum = s;
    updatedAlbum = true;
  }
  if ((strstr(msg, "Artist") == msg) || (strstr(msg, "ARTIST") == msg) || (strstr(msg, "artist") == msg)) {
    String s = String(msg).substring(7); 	// "Artist:" длина = 7
    s.trim();
    currentArtist = s;
    updatedAlbum = true;
  }
  else if ((strstr(msg, "Title") == msg) || (strstr(msg, "TITLE") || (strstr(msg, "title")) == msg)) {
    String s = String(msg).substring(6); 	// "Title:" длина = 6
    s.trim();
    currentTitle = s;
    updated = true;
  }

  if (updatedAlbum) {					// появилась информация Album.
    if (currentArtist.length() > 0 && currentAlbum.length() > 0) currentArtist = currentArtist + " (" + currentAlbum + ")" ;
    else if (currentAlbum.length() > 0) currentArtist = "(" + currentAlbum + ")";
    updatedAlbum = false;
    updated = true;
  }
  if (updated) {						// появилась информация об обновлении, сообщить текущий статус.
    String info;
    if (currentArtist.length() > 0 && currentTitle.length() > 0) info = currentArtist + " - " + currentTitle;
    else if (currentArtist.length() > 0) info = currentArtist + " - ";  		// адрес пока отсутствует
    if (info.length() > 0) config.setTitle(info.c_str());
    updated = false;
  }
}

/*************** BITRATE ***************/
void audio_bitrate(const char *info) {
  if (!info) return;
  if(config.store.audioinfo) telnet.printf("%s %s\n", "##AUDIO.BITRATE#:", info);
//  uint32_t br = static_cast<uint32_t>(atoi(info));
  uint32_t br = atoi(info);
  if (br > 3500) br = br / 1000;
  config.station.bitrate = br;
  display.putRequest(DBITRATE);
#ifdef USE_NEXTION
  nextion.bitrate(config.station.bitrate);
#endif
  netserver.requestOnChange(BITRATE, 0);
}

/*********** PRINTABLE *****************/
bool printable(const char *info) {
  if (!info) return false;
  const unsigned char *p = reinterpret_cast<const unsigned char *>(info);
  while (*p) {
    unsigned char c = *p;
    // Отключение управляющих символов (0x00–0x1F), TAB разрешено по желанию
    if (c < 0x20) {
      if (c != 0x09) return false;
    }
    // ASCII (для печати)
    if (c >= 0x20 && c <= 0x7E) {p++; continue;}
    // UTF-8 multi-byte проверка
    // 2 байта
    if ((c & 0xE0) == 0xC0) {
      if ((p[1] & 0xC0) != 0x80) return false;
      p += 2;
      continue;
    }
    // 3 байта
    if ((c & 0xF0) == 0xE0) {
      if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return false;
      p += 3;
      continue;
    }
    // 4-байтовые символы запретить (они не нужны).
    if ((c & 0xF8) == 0xF0) return false;

    // Всё остальное не так.
    return false;
  }
  return true;
}

void audio_showstation(const char *info) {
  bool p = printable(info) && (info && strlen(info) > 0);
  if (player.remoteStationName) {  // MQTT-переход
    config.setStation(p ? info : config.station.name);
    display.putRequest(NEWSTATION);
    netserver.requestOnChange(STATION, 0);
  }
}

void audio_showstreamtitle(const char *info) {
  if (!info)  return;
  if (strstr(info, "Account already in use") != nullptr || strstr(info, "HTTP/1.0 401") != nullptr) player.setError(info);
  bool p = (strlen(info) > 0) && printable(info);
  if (p) config.setTitle(info);
  else if (strlen(config.station.title) == 0) config.setTitle(config.station.name);
}

void audio_error(const char *info) {
  if (!info) return;
  player.setError(info);
  telnet.printf("##ERROR#:\t%s\r\n", info);
}

/*void audio_id3artist(const char *info) {
//  if (config.store.metaStNameSkip) return;
  config.setStation(info);
  display.putRequest(NEWSTATION);
  netserver.requestOnChange(STATION, 0);
}*/

/* config.setTitle() должно получить строки Title1 и Title2, разделённые дефисом.
 *   netserver.requestOnChange(TITLE, 0);
 *   netserver.loop();
 *   display.putRequest(NEWTITLE);
 * Display::_title() разделяет строки _title1 / _title2 по дефису и обновляет виджеты прокрутки. */
void audio_setTitleSafe(const char *info) {
  if (player.lockOutput) return;
  if (!info) return;
  if(strlen(config.station.title)==0 ||                          					// если пусто
//     config.station.title==config.station.name ||      					// если совпадает с названием станции
//     strcmp(config.station.title, config.station.name) == 0 ||    			// если совпадает с названием станции
     strstr(config.station.title, info) != nullptr ||						// если в нем уже содержится *info
     strstr(config.station.title, " - ") != nullptr ||						// если в нем содержится " - "
      strcmp_P(config.station.title, LANG::const_PlStopped) == 0 ||		// если title = "[связь]" (локализация)
     strstr(config.station.title, "timeout") != nullptr)					// если в нем содержится слово "timeout"
    config.setTitle(info);
  else{
    char out[BUFLEN]= {0};
    strlcat(out, config.station.title, BUFLEN);
    strlcat(out, " - ", BUFLEN);
    strlcat(out, info, BUFLEN);
    config.setTitle(out);
  }
}

void audio_icy_description(const char *info) {
  if (player.lockOutput) return;
  if (!info) return;
  if (strlen(config.station.title) == 0 ||                          				// если пусто
//     config.station.title==config.station.name ||      					// если совпадает с названием станции
//      strcmp(config.station.title, config.station.name) == 0 ||    			// если совпадает с названием станции
//      strstr(config.station.title, info) != nullptr ||						// если в нем уже содержится *info
//      strcmp_P(config.station.title, LANG::const_PlConnect) == 0 ||		// если title = "[связь]" (локализация)
      strstr(config.station.title, "timeout") != nullptr) 				// если содержится слово "timeout"
    config.setTitle(info);
}

void audio_beginSDread() {
  config.setTitle("");
}

void audio_id3data(const char *info) {
  if (player.lockOutput) return;
  if (!info) return;
  telnet.printf("##AUDIO.ID3#: %s\r\n", info);
}

void audio_eof() {
  if (config.getMode() == PM_SDCARD) {
    config.sdResumePos = 0;
    telnet.printf("=== AUDIO EOF: SD -> player.next() ===\n");
    player.next();
  } else {
    player.sendCommand({PR_STOP, 0});
    if(!player.resumeAfterUrl) return;
    if (config.getMode()==PM_WEB){
      player.sendCommand({PR_PLAY, config.lastStation()});
    }else{
      player.setResumeFilePos(config.sdResumePos==0?0:config.sdResumePos-player.sd_min);
      player.sendCommand({PR_PLAY, config.lastStation()});
    }
  }
}

void audio_progress(uint32_t startpos, uint32_t endpos) {
  player.sd_min = startpos;
  player.sd_max = endpos;
  netserver.requestOnChange(SDLEN, 0);
}

void removeBOM(char *s) {
  if (!s) return;
  if (strlen(s) < 3) return;

  if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) {
    memmove(s, s + 3, strlen(s + 3) + 1);
  }
}

bool cleanMeta(const char *src, char *dst, size_t dstSize) {
  if (!src || !dst || dstSize == 0) return false;
  strlcpy(dst, src, dstSize);		// копировать в локальный буфер
  removeBOM(dst);			// BOM удаление
  _utf8_clean(dst);			// UTF-8 чистка
//  if (!printable(dst)) return false;	 // Только проверка (не вносит изменений):
  return true;
}

void _utf8_clean(char *s) {
  char *in = s;
  char *out = s;
  while (*in) {
    unsigned char c = (unsigned char)*in;
    
    if (c == 0xE2 && (unsigned char)in[1] == 0x80 && ((unsigned char)in[2] == 0x8B || (unsigned char)in[2] == 0x8C || (unsigned char)in[2] == 0x8D)) 
      { in += 3; continue; }														// --- ZERO-WIDTH фильтрация символов ---
    if (c == 0xC2 && (unsigned char)in[1] == 0xAD) { in += 2; continue; }				// Мягкий дефис

    if (c == 0xC2 && (unsigned char)in[1] == 0xAB) { in += 2; *out ++ = '<'; continue; }		// входная "кривая" кавычка -> заменяем на простую
    if (c == 0xC2 && (unsigned char)in[1] == 0xBB) { in += 2; *out ++ = '>'; continue; }		// выходная "кривая" кавычка -> заменяем на простую

    // Все остальные UTF-8 без изменений - простое копирование побайтно.
    *out++ = *in++;
  }
  *out = '\0';
}

#endif  // AUDIOHANDLERS_H
