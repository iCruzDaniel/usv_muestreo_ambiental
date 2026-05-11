/**
 * TimeManager.h — Gestión de tiempo UTC y formato ISO 8601.
 */

#pragma once
#include <Arduino.h>
#include <time.h>

class TimeManager {
public:
    /**
     * Sincroniza el tiempo del sistema con una época UNIX (segundos) y ms.
     * Útil para sincronizar con GPS o NTP.
     */
    static void sync(uint32_t unixTimeS, uint32_t ms = 0) {
        struct timeval tv;
        tv.tv_sec  = unixTimeS;
        tv.tv_usec = ms * 1000;
        settimeofday(&tv, nullptr);
        _synced = true;
    }

    /**
     * Retorna el timestamp actual en formato ISO 8601: "YYYY-MM-DDTHH:MM:SS.mmmZ"
     */
    static String getIsoTimestamp() {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        
        time_t nowtime = tv.tv_sec;
        struct tm* nowtm = gmtime(&nowtime);
        
        char buf[32];
        // Formato: 2025-10-07T17:55:00.123Z
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", nowtm);
        
        char finalBuf[40];
        snprintf(finalBuf, sizeof(finalBuf), "%s.%03ldZ", buf, tv.tv_usec / 1000);
        
        return String(finalBuf);
    }

    static bool isSynced() { return _synced; }

private:
    static bool _synced;
};
