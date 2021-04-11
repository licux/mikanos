#pragma once

enum LogLevel{
    kError = 3,
    kWarn = 4,
    kInfo = 6,
    kDebug = 7,
};

/** @brief update global log priority threshold */
void SetLogLevel(LogLevel level);

/** @brief Logging with specifiled priority
 * @param level log prioriaty
 * @param format log mesasge
 */
int Log(LogLevel level, const char* format, ...);