#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "P4Localization.h"

using namespace mg::p4;

int main() {
    constexpr uint8_t languageCount =
        static_cast<uint8_t>(P4Language::Count);
    assert(languageCount == 7U);

    for (uint8_t index = 0; index < languageCount; ++index) {
        const P4Language language = static_cast<P4Language>(index);
        assert(isValidP4Language(index));
        assert(std::strlen(p4LanguageName(language)) > 0U);
        setP4Language(language);
        const P4UiTexts& text = p4Texts();
        assert(std::strlen(text.mainTitle) > 0U);
        assert(std::strlen(text.oneToOne) > 0U);
        assert(std::strlen(text.subDTest) > 0U);
        assert(std::strlen(text.subDTestInfo) > 0U);
        assert(std::strlen(text.customMap) > 0U);
        assert(std::strlen(text.customMapInfo) > 0U);
        assert(std::strlen(text.start) > 0U);
        assert(std::strlen(text.pause) > 0U);
        assert(std::strlen(text.scanPause) > 0U);
        assert(std::strlen(text.stopOnError) > 0U);
        assert(std::strlen(text.nonStop) > 0U);
        assert(std::strlen(text.testSpeed) > 0U);
        assert(std::strlen(text.legendWaiting) > 0U);
        assert(std::strlen(text.legendScanning) > 0U);
        assert(std::strlen(text.legendOk) > 0U);
        assert(std::strlen(text.legendOpen) > 0U);
        assert(std::strlen(text.legendShort) > 0U);
        assert(std::strlen(text.legendWrong) > 0U);
        assert(std::strlen(text.legendResistance) > 0U);
        const char* selected = p4SelectText("TR", "EN", "NL", "DE", "FR", "ES", "PL");
        const char* expected[] = {"TR", "EN", "NL", "DE", "FR", "ES", "PL"};
        assert(std::strcmp(selected, expected[index]) == 0);
    }

    assert(!isValidP4Language(languageCount));
    assert(std::strcmp(p4LanguageName(P4Language::Turkish), "Türkçe") == 0);
    assert(std::strcmp(p4LanguageName(P4Language::French), "Français") == 0);
    assert(std::strcmp(p4LanguageName(P4Language::Spanish), "Español") == 0);
    setP4Language(P4Language::Turkish);
    assert(std::strcmp(p4Texts().language, "Dil") == 0);
    assert(std::strcmp(p4Texts().scanPause, "TARA / DURAKLAT") == 0);
    assert(std::strcmp(p4Texts().legendShort, "KISA DEVRE") == 0);
    assert(std::strcmp(p4Texts().legendWrong, "YANLIŞ BAĞLANTI") == 0);
    setP4Language(P4Language::English);
    assert(std::strcmp(p4Texts().scanPause, "SCAN / PAUSE") == 0);
    std::cout << "7-language localization table + selector passed.\n";
    return 0;
}
