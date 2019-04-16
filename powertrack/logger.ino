/////////////////////////////////////////////////////////////////////////////////////
// Logging functions
// Regular (time based) logging, and event logging
/////////////////////////////////////////////////////////////////////////////////////

// Tested against SD Card library (built-in) version 1.2.2
#include "powertrack.h"
#include <SD.h>

int sampleCount = 0;
long timeSinceLastLogEntry;

/////////////////////////////////////////////////////////////////////////////////////
// loggerInit
// Init (called from main)
/////////////////////////////////////////////////////////////////////////////////////
void loggerInit()
{
  SD.begin(SD_CSpin);
  loggerZeroVariables();
  timeSinceLastLogEntry = rtcGetTime();
}

/////////////////////////////////////////////////////////////////////////////////////
// loggerLoopHandler
// Loop handler (called from main)
/////////////////////////////////////////////////////////////////////////////////////
void loggerLoopHandler()
{
  long now = rtcGetTime();
  if (now - timeSinceLastLogEntry > cfg_fieldValue(ndx_secsPerLog)) {
    loggerLogEntry();
    timeSinceLastLogEntry = now;
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// SD-card utility functions
/////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////
// loggerRootDir
// list directory of SD root, to port p
/////////////////////////////////////////////////////////////////////////////////////////////////
void loggerRootDir (Stream &p)
{
  File dir = SD.open("/");
  while (true)
  {
    File entry = dir.openNextFile();
    if (! entry)
    {
      p.println("** Done **");
      break;
    }
    p.print(entry.name());
    if (entry.isDirectory())
    {
      p.println("/ (DIR!)");
    }
    else
    {
      p.print("\t\t");
      p.println(entry.size(), DEC);
    }
    entry.close();
  }
  dir.close();
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// loggerDumpFile
// dump contents of specified file, to port p
/////////////////////////////////////////////////////////////////////////////////////////////////
void loggerDumpFile(Stream &p, char *filename)
{
  char buf[20];
  char c;
  File f = SD.open(filename);
  if (!f) {
    p.println("File not found");
    return;
  }

  while( EOF != (c = f.read()) ) {    // until EOF
    while (0 == p.write(c));          // keep trying until the character is written (pacing)
    if (0x03 == p.read()) {
      p.write(CRLF);
      goto exit;  // check for CTRL-C
    }
  }
  exit:
  f.close();
}
/////////////////////////////////////////////////////////////////////////////////////////////////
// loggerEraseFile
// erase specified file
/////////////////////////////////////////////////////////////////////////////////////////////////
void loggerEraseFile (Stream &p, char *filename)
{
  SD.remove(filename);
}

enum logVarType {
  lvtNumeric      // numeric value, will accumulate averaged/min/max
 ,lvtEnumSample   // enum value, will sample current value
 ,lvtEnumAccum    // enum value, will accumulate bitmap of values seen in log entry interval
 ,lvtLoad         // special case: read the load state
};

struct {
  char *name;       // name for CSV header
  int sourceIndex;  // index in Victron data array
  logVarType type;  // type
  long sum;         // accumulated sum or bitmap
  long min;         // accumulated min
  long max;         // accumulated max
} logVariables[] = 

{
    {"BatVolt", FI_V,   lvtNumeric}  
  ,{"PVVolt",   FI_VPV, lvtNumeric}  
  ,{"PVPwr",    FI_PPV, lvtNumeric}
  ,{"BatCur",   FI_I,   lvtNumeric}
  ,{"Error",    FI_ERR, lvtEnumSample}
  ,{"State",    FI_CS,  lvtEnumAccum}
  ,{"MPPT",     FI_MPPT,  lvtEnumAccum}
  ,{"Load",     -1,  lvtEnumAccum}      // hack (special case)
};


// receive notification of a new Victron sample
void loggerNotifyVictronSample()
{
  for (int i=0; i< COUNT_OF(logVariables); i++) {
    int ndx = logVariables[i].sourceIndex;
    
    // get the Victron data field, unless it's the load we are logging
    long value = -1 == ndx ? loadctlGetLoadState() : victronGetFieldValue(ndx); 
    // Note: strictly speaking it doesn't make sense to tie the load state sampling to Victron data packets
    // Logically this should be done independently. But practically speaking, if the Victron stops sending
    // data, it probably means something pretty bad.
       
    switch(logVariables[i].type) {
      case lvtNumeric:
        logVariables[i].sum += value;
        logVariables[i].min = min(logVariables[i].min, value);
        logVariables[i].max = max(logVariables[i].max, value);
        break;
      case lvtEnumSample:
        logVariables[i].sum = value;
        break;
      case lvtEnumAccum:
        logVariables[i].sum |= 1 << value;
        break;
    }
  }
  sampleCount++;
  
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// loggerLogEntry
// Check if the day has changed and if so, format a new filename
// Open the file and write a line from the log
// Reset the log variables
/////////////////////////////////////////////////////////////////////////////////////////////////
char logFileName[13]; // 8.3 + null
void loggerLogEntry() 
{
  rtcReadTime();
  // check if the day has changed and if so, format new filename
  long day = rtcGetTime() / DAY_SECONDS;
  
//  monitorPort.println(day);
//  monitorPort.println(logFilenameDay);
//  monitorPort.println(newDay);

  // Format filename based on day
  sprintf(logFileName, "%04d%02d%02d.csv", rtcYear(), rtcMonth(), rtcDay());
  bool newDay = 0 == SD.exists(logFileName);
    
  // open the file
  File f = SD.open(logFileName, FILE_WRITE);
  if (NULL == f) {
    reportStatus(statusSDError, true);
    return;    
  }
  
  if (newDay) {
    // Print the header:
    // time, number of samples, data field names
    
    f.print("Time, smpCount");

    for (int i=0; i< COUNT_OF(logVariables); i++) {
      f.print(", ");
      switch(logVariables[i].type) {
        case lvtNumeric:
          f.print(logVariables[i].name);
          f.print("_sum, ");
          f.print(logVariables[i].name);
          f.print("_min, ");
          f.print(logVariables[i].name);
          f.print("_max");
          break;
        case lvtEnumSample:
        case lvtEnumAccum:
          f.print(logVariables[i].name);
          break;
      }
    }
    f.println();
  }

  // Write out the timestamp and the sample count
  f.print(rtcGetTime());
  f.print(", ");
  f.print(sampleCount);

  // Write out the data line
  for (int i=0; i< COUNT_OF(logVariables); i++) {

    // Print the variables
    f.print(", ");
    switch(logVariables[i].type) {
      case lvtNumeric:
        f.print((float)logVariables[i].sum / sampleCount, 2); // 2 decimals, should be enough precision
        f.print(", ");
        f.print(logVariables[i].min);
        f.print(", ");
        f.print(logVariables[i].max);
        break;
      case lvtEnumSample:
      case lvtEnumAccum:
        f.print(logVariables[i].sum);
        break;
    }
  }

  f.println();
  f.close();

  // reset logger variables
  loggerZeroVariables();
}

void loggerZeroVariables()
{
  for (int i=0; i<COUNT_OF(logVariables); i++) {
    logVariables[i].sum = 0;
    logVariables[i].min = 0x7FFFFFFF;
    logVariables[i].max = 0x80000000;
  }
  sampleCount = 0;
}


/////////////////////////////////////////////////////////////////////////////////////////////////
// Status log functions
//
// A line is written to the status log for various events including state transitions
/////////////////////////////////////////////////////////////////////////////////////////////////

#define statusLogFilename "status.log"

/////////////////////////////////////////////////////////////////////////////////////////////////
// statuslogCheckChange
// Assign new value to old value, printing a log message (and returning true) if it is a change
/////////////////////////////////////////////////////////////////////////////////////////////////
bool statuslogCheckChange(char *string, bool newValue, bool &currentValue)
{
  bool changed = false;
  if (newValue != currentValue) {
    currentValue = newValue;    
    changed = true;
    statusLogPrint(string, newValue);
  }
  return changed;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// Status log print functions for strings, longs and floats
/////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: ugly unsafe literal buffer lengths
void statusLogPrint(char *string, bool flag)
{
  const int buflen = 100;
  char buf[buflen];

  sprintf(buf, "%19s %30s = %5s", rtcPresentTime(), string, flag? "TRUE" : "FALSE");

  statuslogWriteLine(buf);
}

// This one is for logging commands
void statusLogPrint(char *string)
{
  const int buflen = 100;
  char buf[buflen];

  sprintf(buf, "%19s %50s", rtcPresentTime(), string);

  statuslogWriteLine(buf, false);
}

void statusLogPrint(char *string, long l)
{
  const int buflen = 100;
  char buf[buflen];

  sprintf(buf, "%19s %50s = %ld", rtcPresentTime(), string, l);

  statuslogWriteLine(buf);
}

void statusLogPrint(char *string, double d)
{
  const int buflen = 100;
  char buf[buflen];
  char bufd[30];  // buffer for nummber

  dtostrf(d, 7, 3, bufd);

  sprintf(buf, "%19s %50s = %s", rtcPresentTime(), string, bufd);

  statuslogWriteLine(buf);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// statusLogWriteLine
// write a line in the status log
/////////////////////////////////////////////////////////////////////////////////////////////////
void statuslogWriteLine(char *string, bool echo)
{
  char logFileName[13]; // 8.3 + null  
  
  if (echo) monitorPort.println(string);

  // Format filename based on month
  sprintf(logFileName, "%04d%02d.log", rtcYear(), rtcMonth(), rtcDay());
  bool newDay = 0 == SD.exists(logFileName);
    
  // open the file
  File f = SD.open(logFileName, FILE_WRITE);
  if (NULL == f) {
    reportStatus(statusSDError, true);
    return;    
  }

  if (f) {
    f.println(string);
    f.close();
  }  
}

//void statuslogWriteLine(char *string)
//{
//  statuslogWriteLine(string, true);
//}
//
//void loggerLogCommand(char *string)
//{
//  statuslogWriteLine(string, false);
//}
