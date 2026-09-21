#pragma once

#include <stdint.h>

namespace mg::p4 {

enum class P4Language : uint8_t {
    Turkish,
    English,
    Dutch,
    German,
    French,
    Spanish,
    Polish,
    Count,
};

struct P4UiTexts {
    const char* mainTitle;
    const char* mainSubtitle;
    const char* oneToOne;
    const char* oneToOneInfo;
    const char* subDTest;
    const char* subDTestInfo;
    const char* customMap;
    const char* customMapInfo;
    const char* dummyTest;
    const char* touchDiagnostic;
    const char* settings;
    const char* settingsInfo;
    const char* settingsTitle;
    const char* settingsSubtitle;
    const char* wifi;
    const char* ethernet;
    const char* networkNotConfigured;
    const char* cableLearn;
    const char* qrBarcode;
    const char* reports;
    const char* multiConnector;
    const char* unavailable;
    const char* mainStatus;
    const char* language;
    const char* selectLanguage;
    const char* back;
    const char* start;
    const char* pause;
    const char* scanPause;
    const char* direction;
    const char* stopOnError;
    const char* nonStop;
    const char* controls;
    const char* menu;
    const char* ready;
    const char* scanning;
    const char* paused;
    const char* complete;
    const char* scanAtoB;
    const char* scanBtoA;
    const char* testSpeed;
    const char* controlPanel;
    const char* singleStep;
    const char* resetAbort;
    const char* soundOn;
    const char* soundOff;
    const char* profile;
    const char* nextCable;
    const char* mode;
    const char* standby;
    const char* diagnostic;
    const char* touchTest;
    const char* diagnosticInfo;
    const char* close;
    const char* legendWaiting;
    const char* legendScanning;
    const char* legendOk;
    const char* legendOpen;
    const char* legendShort;
    const char* legendWrong;
    const char* legendResistance;
};

void setP4Language(P4Language language);
P4Language currentP4Language();
const P4UiTexts& p4Texts();

const char* p4SelectText(const char* turkish,
                         const char* english,
                         const char* dutch,
                         const char* german,
                         const char* french,
                         const char* spanish,
                         const char* polish);
const char* p4LanguageName(P4Language language);
bool isValidP4Language(uint8_t value);

}  // namespace mg::p4
