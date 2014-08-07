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
bool LogToStdOut = true;

void KineticLogger_Init(const char* logFile)
{
    LogToStdOut = true;
    if (logFile != NULL)
    {
        FILE* fd;
        strcpy(LogFile, logFile);
        fd = fopen(LogFile, "w");
        if (fd > 0)
        {
            fclose(fd);
            LogToStdOut = false;
        }
        else
        {
            KineticLogger_LogPrintf("Failed to initialize logger with file: fopen('%s') => fd=%d", logFile, fd);
        }
    }
}

void KineticLogger_Log(const char* message)
{
    if (LogToStdOut)
    {
        fprintf(stdout, "%s\n", message);
    }
    else if (LogFile != NULL)
    {
        FILE* fd = fopen(LogFile, "a");
        fprintf(fd, "%s\n", message);
        fclose(fd);
    }
}

int KineticLogger_LogPrintf(const char* format, ...)
{
   va_list arg_ptr;
   char buffer[1024];
   int result;

   va_start(arg_ptr, format);
   result = vsprintf(buffer, format, arg_ptr);
   va_end(arg_ptr);

   KineticLogger_Log(buffer);

   return(result);
}
