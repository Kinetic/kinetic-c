/*
* kinetic-c
* Copyright (C) 2014 Seagate Technology.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#include "kinetic_logger.h"
#include "kinetic_types.h"
#include <stdio.h>
#include <string.h>

static char LogFile[256] = "";
bool LogToConsole = true;

void KineticLogger_Init(const char* logFile)
{
    LogToConsole = true;
    if (logFile != NULL)
    {
        FILE* fd;
        strcpy(LogFile, logFile);
        fd = fopen(LogFile, "w");
        if (fd != stdout && fd != stderr && fd != stdin)
        {
            fclose(fd);
            LogToConsole = false;
        }
        else
        {
            fprintf(stderr, "Failed to initialize logger with file: fopen('%s') => fd=%zd", logFile, fd);
            fflush(fd);
        }
    }
}

void KineticLogger_Log(const char* message)
{
    FILE* fd = NULL;
    if (message == NULL)
    {
        return;
    }

    fd = LogToConsole ? stderr : fopen(LogFile, "a");
    if (fd >= 0)
    {
        fprintf(fd, "%s\n", message);
        fflush(fd);

        // Don't close std/already-opened streams
        if (LogFile != NULL && fd != stdout && fd != stderr && fd != stdin)
        {
            fclose(fd);
        }
    }
}

int KineticLogger_LogPrintf(const char* format, ...)
{
    int result = -1;

    if (format != NULL)
    {
        va_list arg_ptr;
        char buffer[1024];
        va_start(arg_ptr, format);
        result = vsprintf(buffer, format, arg_ptr);
        KineticLogger_Log(buffer);
        va_end(arg_ptr);
    }

   return(result);
}
