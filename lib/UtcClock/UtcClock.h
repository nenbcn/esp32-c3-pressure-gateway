#include <cstdint>

const long gmtOffsetSeconds = 0;
const int daylightOffsetSeconds = 0;

class UtcClock {
    public:
        UtcClock(const char *ntpServerMain, const char *ntpServerBackup);
        void init();
        uint64_t getTime(uint64_t millisTimestamp);
        
    private:
        bool hasLastSyncExpired() const;
        void syncronize();

        const char *ntpServerMain;
        const char *ntpServerBackup;

        bool isSyncronized = false;
        uint64_t lastSyncMillis = 0;
        uint64_t lastSyncUnixMs = 0;
};
