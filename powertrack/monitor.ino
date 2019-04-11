/////////////////////////////////////////////////////////////////////////////////////
// Config and debug monitor, accessible over serial port
/////////////////////////////////////////////////////////////////////////////////////

#include "powertrack.h"
#include <EEPROM.h>

#define LINE_BUF_LEN 30 // max number of bytes in a command
#define MAX_ARG_LEN 12 // max characters per arg
#define MAX_ARG_NUM 3  // max number of args per line

bool error_flag = false;
int inbufpos;

char helpstring[] = 
      "def                  - dump EEPROM fields" CRLF
      "inv                  - invalidate EEPROM (use to reinitialize or to test init code)" CRLF
      "com                  - commit: save in-memory config fields to EEPROM" CRLF
      "set index value      - set config field[index] to value (in memory)" CRLF
      "rtc                  - read current RTC setting" CRLF
      "rtc time hh:mm:ss    - set RTC time (not into hardware)" CRLF
      "rtc date yyyy/mm/dd  - set RTC date (not into hardware)" CRLF
      "rtc adj              - actually adjust RTC" CRLF
      "vs                   - Victron status, print out current Victron parameters" CRLF
      "ls                   - list SD card root files" CRLF
      "dmp filename         - dump contents of specified file" CRLF
      "era filename         - erase specified file" CRLF
      ;
      
/////////////////////////////////////////////////////////////////////////////////////
// monitorInit
// Init (early, called from main)
/////////////////////////////////////////////////////////////////////////////////////
void monitorInit() {
  monitorPort.begin(9600);
  monitorPort.setTimeout(10000); // 5 seconds
}

/////////////////////////////////////////////////////////////////////////////////////
// monitorInit
// Init (late, called from main)
/////////////////////////////////////////////////////////////////////////////////////
void monitorInit2() {
  statuslogWriteLine("ALBATROS power system v 1.0");
  statuslogWriteLine(rtcPresentTime());
  monitorPort.println("Serial monitor - commands:");
  monitorPort.print(helpstring);
  inbufpos = 0;
}

/////////////////////////////////////////////////////////////////////////////////////
// monitorLoopHandler
// Loop handler (called from main)
/////////////////////////////////////////////////////////////////////////////////////
void monitorLoopHandler() {
  char mon_buf[LINE_BUF_LEN];
  char *command, *arg1, *arg2;
  char c;
  
  while (EOF != (c = monitorPort.read())) {
    if ('\b' == c || 0x7f == c) { // backspace or rubout
      if (0 != inbufpos) {
        inbufpos--;
        monitorPort.print("\b \b");
      }
    }
    else {
      if (inbufpos+1 < LINE_BUF_LEN) {
        mon_buf[inbufpos++] = c;
        monitorPort.print(c);
      }
    }
  }

  // no fixing up needed, if there's nothing in the buffer
  if (0 == inbufpos)
    return;

  // If the last character was CR, we're done; replace it with null
  // Else quit and come back later
  if ('\r' == mon_buf[inbufpos-1])
    mon_buf[inbufpos-1] = 0;
  else
    return;

  // show what was received as a command line
  monitorPort.print("<");
  monitorPort.print(mon_buf);
  monitorPort.println(">");

  // parse the line into ' ' separated arguments
  command = strtok(mon_buf, " ");
  arg1 = strtok(NULL, " ");
  arg2 = strtok(NULL, " ");
  inbufpos = 0;
  
  if (0 == strcmp(command, "def")) {
    cfgDumpFieldValues(monitorPort);
  }
  else if (0 == strcmp(command, "inv")) {
    cfg_invalidateEE();
  }  
  else if (0 == strcmp(command, "com")) {
    cfg_saveConfig();
  }
  else if (0 == strcmp(command, "set")) {
    cfg_set(arg1, arg2);
  }
  else if (0 == strcmp(command, "rtc")) {
    if (NULL == *arg1) {  // no arguments - display time
      //present = rtc.now().unixtime();   // I really think we will want YYYY/MM/DD HH:MM:SS
      monitorPort.print("current time: ");
      monitorPort.println(rtcPresentTime());
    }
    else if (0 == strcmp(arg1, "date")) {  
      monitorPort.print("setting date to: <");
      monitorPort.print(arg2);
      monitorPort.println(">");
      rtcSetDate(arg2);
    }
    else if (0 == strcmp(arg1, "time")) {  
      monitorPort.print("setting time to: <");
      monitorPort.print(arg2);
      monitorPort.println(">");
      rtcSetTime(arg2);
    }    
    else if (0 == strcmp(arg1, "adj")) {  
      monitorPort.println("adjusting rtc");
      rtcAdjust();
    }
  }
  else if (0 == strcmp(command, "vs")) {  
    monitorPort.println("Victron status:");
    victronDumpStatus(monitorPort);
  }    
  else if (0 == strcmp(command, "ls")) {  
    monitorPort.println("SD card root files:");
    loggerRootDir(monitorPort);
  }    
  else if (0 == strcmp(command, "dmp")) {  
    monitorPort.println(arg1);
    loggerDumpFile(monitorPort, arg1);
  }  
  else if (0 == strcmp(command, "era")) {  
    monitorPort.println(arg1);
    loggerEraseFile(monitorPort, arg1);
  }  
  else {
      monitorPort.print(helpstring);
  } 
}
