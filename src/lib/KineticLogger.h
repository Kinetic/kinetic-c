#ifndef _KINETIC_LOGGER_H
#define _KINETIC_LOGGER_H

#define KINETIC_LOG_FILE "kinetic.log"

void KineticLogger_Init(const char* log_file);
void KineticLogger_Log(const char* message);

#define LOG(message) KineticLogger_Log(message);

#endif // _KINETIC_LOGGER_H
