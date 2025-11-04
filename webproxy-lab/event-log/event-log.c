#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include "event-log.h"

#define TARGET_DIRECTORY "event-log"
#define FILE_NAME "event-log/proxy-event.log"

void writeEvent(const char *message) {
  FILE *pFile = fopen(FILE_NAME, "a");
  if(pFile == NULL) return;

  time_t currentUtc = time(NULL);
  struct tm *pCurrentLocal = localtime(&currentUtc);

  fprintf(pFile, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n", pCurrentLocal->tm_year + 1900, pCurrentLocal->tm_mon + 1, pCurrentLocal->tm_mday, pCurrentLocal->tm_hour, pCurrentLocal->tm_min, pCurrentLocal->tm_sec, message);
  fclose(pFile);
}
