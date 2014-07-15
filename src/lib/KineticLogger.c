/*
 * kinetic-c-client
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

#include "KineticLogger.h"
#include "KineticTypes.h"
#include <stdio.h>
#include <string.h>

static char LogFile[256] = "";
bool LogToStdErr = true;

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
