#include "powertrack.h"

// Install RTCLib by Adafruit.
// Tested against version 1.2.0
#include <RTClib.h>

enum {PRE_INIT, NOT_PRESENT, UNINITIALIZED, INITIALIZED} e_rtcState;

RTC_PCF8523 rtc;

void rtcInit()
{
  if (!rtc.begin()) {
    e_rtcState = NOT_PRESENT;
    return;
  }
  
  e_rtcState = rtc.initialized() ? INITIALIZED : UNINITIALIZED;
}
// temporary storage while setting RTC
DateTime dt;
TimeSpan tm;

// value that will be used for the accessors and gets set by rtcReadTime()
DateTime now;

void rtcReadTime() {
  now = rtc.now();
}

int rtcYear() {
  return now.year();
}

int rtcMonth() {
  return now.month();
}

int rtcDay() {
  return now.day();
}

int rtcHour() {
  return now.hour();
}

int rtcMinute() {
  return now.minute();
}

int rtcSecond() {
  return now.second();
}

char *rtcPresentTime()
{
  static char buf[20] = (__TIME__);
  rtcReadTime();
  sprintf(buf,"%04d/%02d/%02d %02d:%02d:%02d", 
    now.year(), now.month(), now.day(),
    now.hour(), now.minute(), now.second() );
  return buf;
}

//HardwareSerial &mp = monitorPort;

void rtcSetTime(char *s)
{
  int hour,min,sec;
  sscanf(s, "%d:%d:%d", &hour, &min, &sec );
  tm = TimeSpan(0,hour,min,sec);
}

void rtcSetDate (char *s) 
{
  int year, month, day;
  sscanf(s, "%d/%d/%d", &year, &month, &day );
  dt = DateTime(year, month, day, 0, 0, 0);
}

void rtcAdjust()
{
  DateTime t = dt+tm;
  rtc.adjust(dt+tm);
}

long rtcGetTime() {
  rtcReadTime();
  return now.unixtime();
}
