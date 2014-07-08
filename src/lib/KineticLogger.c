#include "KineticLogger.h"
#include "KineticTypes.h"
#include <stdio.h>
#include <string.h>

static char LogFile[256] = "";
bool LogToStdErr = false;

void KineticLogger_Init(const char* log_file)
{
    if (log_file == NULL)
    {
        LogToStdErr = true;
    }
    else
    {
        FILE* fd;
        strcpy(LogFile, log_file);
        fd = fopen(LogFile, "w");
        fclose(fd);
    }
}

void KineticLogger_Log(const char* message)
{
    if (LogToStdErr)
    {
        fprintf(stderr, "%s\n", message);
    }
    else if (LogFile != NULL)
    {
        FILE* fd = fopen(LogFile, "a");
        fprintf(fd, "%s\n", message);
        fclose(fd);
    }
}
