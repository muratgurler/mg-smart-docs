#include "P4Localization.h"

namespace mg::p4 {

namespace {

P4Language activeLanguage = P4Language::Turkish;

const P4UiTexts kTexts[] = {
    {
        "MG SMART TEST - ANA MENÜ",
        "Yapmak istediğiniz işlemi seçin",
        "NORMAL KABLO TESTİ",
        "PE + ayarlanabilir pinler + dS | 1:1 / özel harita",
        "SUB-D KABLO TESTİ",
        "1..N + dS | Sub-D profili",
        "ÖZEL HARİTA",
        "A1->B3, A2->B1 gibi özel bağlantılar",
        "DUMMY TESTİ",
        "DOKUNMATİK DİAGNOSTİK",
        "AYARLAR", "Cihaz, ağ ve diagnostik ayarları",
        "AYARLAR", "Cihaz yapılandırmasını seçin",
        "Wi-Fi", "Ethernet", "Henüz yapılandırılmadı",
        "KABLO ÖĞRENME",
        "QR / BARKOD İLE TEST",
        "RAPORLAR",
        "ÇOKLU KONNEKTÖR TESTİ",
        "Henüz hazır değil",
        "JC1060 hazır | Harici 128 kanal ölçüm kartı bağlı değil",
        "Dil", "DİL SEÇİN", "GERİ", "BAŞLAT", "DURAKLAT",
        "TARA / DURAKLAT", "YÖN",
        "HATADA DUR", "NON-STOP",
        "KONTROLLER", "MENÜ", "HAZIR", "TARANIYOR", "DURAKLATILDI",
        "TAMAMLANDI", "A -> B TARAMASI", "B -> A TARAMASI",
        "TEST HIZI", "SANAL ÖN PANEL KONTROLLERİ", "TEK ADIM",
        "RESET / İPTAL", "SES: AÇIK", "SES: KAPALI", "PROFİL",
        "SONRAKİ KABLO", "MOD", "BEKLEME", "DİAGNOSTİK",
        "DOKUNMATİK TESTİ", "GT911 koordinatlarını beş noktada doğrular.",
        "KAPAT",
        "BEKLİYOR", "TARAMA", "DOĞRU", "AÇIK DEVRE", "KISA DEVRE",
        "YANLIŞ BAĞLANTI", "YÜKSEK DİRENÇ",
    },
    {
        "MG SMART TEST - MAIN MENU",
        "Select the operation you want to perform",
        "NORMAL CABLE TEST",
        "PE + adjustable pins + dS | 1:1 / custom map",
        "SUB-D CABLE TEST",
        "1..N + dS | Sub-D profile",
        "CUSTOM MAP", "Custom pin routing, e.g. A1->B3, A2->B1",
        "DUMMY TEST", "TOUCH DIAGNOSTIC",
        "SETTINGS", "Device, network and diagnostic settings",
        "SETTINGS", "Select device configuration",
        "Wi-Fi", "Ethernet", "Not configured yet", "CABLE LEARNING",
        "QR / BARCODE TEST", "REPORTS", "MULTI-CONNECTOR TEST",
        "Not ready yet",
        "JC1060 ready | External 128-channel scanner not connected",
        "Language", "SELECT LANGUAGE", "BACK", "START", "PAUSE",
        "SCAN / PAUSE", "DIRECTION",
        "STOP ON ERROR", "NON-STOP",
        "CONTROLS", "MENU", "READY", "SCANNING", "PAUSED", "COMPLETE",
        "A -> B SCAN", "B -> A SCAN", "TEST SPEED", "VIRTUAL FRONT PANEL",
        "SINGLE STEP", "RESET / ABORT", "SOUND: ON", "SOUND: OFF", "PROFILE",
        "NEXT CABLE", "MODE", "STANDBY", "DIAGNOSTIC", "TOUCH TEST",
        "Verifies GT911 coordinates at five points.", "CLOSE",
        "WAITING", "SCANNING", "OK", "OPEN CIRCUIT", "SHORT CIRCUIT",
        "WRONG CONNECTION", "HIGH RESISTANCE",
    },
    {
        "MG SMART TEST - HOOFDMENU",
        "Kies de gewenste bewerking",
        "NORMALE KABELTEST",
        "PE + instelbare pinnen + dS | 1:1 / aangepaste kaart",
        "SUB-D KABELTEST",
        "1..N + dS | Sub-D-profiel",
        "AANGEPASTE KAART", "Aangepaste pinroute, bv. A1->B3, A2->B1",
        "DUMMYTEST", "TOUCHDIAGNOSE",
        "INSTELLINGEN", "Apparaat-, netwerk- en diagnose-instellingen",
        "INSTELLINGEN", "Kies apparaatconfiguratie",
        "Wi-Fi", "Ethernet", "Nog niet geconfigureerd", "KABEL LEREN",
        "QR / BARCODETEST", "RAPPORTEN", "MULTI-CONNECTORTEST",
        "Nog niet gereed",
        "JC1060 gereed | Externe 128-kanaals scanner niet aangesloten",
        "Taal", "KIES TAAL", "TERUG", "START", "PAUZE",
        "SCAN / PAUZE", "RICHTING",
        "STOP BIJ FOUT", "NON-STOP",
        "BEDIENING", "MENU", "GEREED", "SCANNEN", "GEPAUZEERD", "VOLTOOID",
        "A -> B SCAN", "B -> A SCAN", "TESTSNELHEID", "VIRTUEEL FRONTPANEEL",
        "ENKELE STAP", "RESET / AFBREKEN", "GELUID: AAN", "GELUID: UIT", "PROFIEL",
        "VOLGENDE KABEL", "MODUS", "STANDBY", "DIAGNOSE", "TOUCHTEST",
        "Controleert GT911-coördinaten op vijf punten.", "SLUITEN",
        "WACHTEN", "SCANNEN", "OK", "OPEN", "KORTSLUITING",
        "VERKEERDE VERBINDING", "HOGE WEERSTAND",
    },
    {
        "MG SMART TEST - HAUPTMENÜ",
        "Gewünschten Vorgang auswählen",
        "NORMALER KABELTEST",
        "PE + einstellbare Pins + dS | 1:1 / Sonderbelegung",
        "SUB-D KABELTEST",
        "1..N + dS | Sub-D-Profil",
        "SONDERBELEGUNG", "Sonderbelegung, z.B. A1->B3, A2->B1",
        "DUMMYTEST", "TOUCH-DIAGNOSE",
        "EINSTELLUNGEN", "Geräte-, Netzwerk- und Diagnoseeinstellungen",
        "EINSTELLUNGEN", "Gerätekonfiguration auswählen",
        "Wi-Fi", "Ethernet", "Noch nicht konfiguriert", "KABEL LERNEN",
        "QR / BARCODE-TEST", "BERICHTE", "MULTI-STECKER-TEST",
        "Noch nicht bereit",
        "JC1060 bereit | Externer 128-Kanal-Scanner nicht verbunden",
        "Sprache", "SPRACHE WÄHLEN", "ZURÜCK", "START", "PAUSE",
        "SCAN / PAUSE", "RICHTUNG",
        "STOPP BEI FEHLER", "NON-STOP",
        "STEUERUNG", "MENÜ", "BEREIT", "SCANNT", "PAUSIERT", "FERTIG",
        "A -> B SCAN", "B -> A SCAN", "TESTTEMPO", "VIRTUELLES FRONTPANEL",
        "EINZELSCHRITT", "RESET / ABBRUCH", "TON: EIN", "TON: AUS", "PROFIL",
        "NÄCHSTES KABEL", "MODUS", "STANDBY", "DIAGNOSE", "TOUCHTEST",
        "Prüft GT911-Koordinaten an fünf Punkten.", "SCHLIESSEN",
        "WARTET", "SCAN", "OK", "OFFEN", "KURZSCHLUSS",
        "FALSCH VERBUNDEN", "HOHER WIDERSTAND",
    },
    {
        "MG SMART TEST - MENU PRINCIPAL",
        "Sélectionnez l'opération souhaitée",
        "TEST DE CÂBLE NORMAL",
        "PE + broches réglables + dS | 1:1 / carte perso.",
        "TEST DE CÂBLE SUB-D",
        "1..N + dS | Profil Sub-D",
        "CARTE PERSONNALISÉE", "Câblage personnalisé, ex. A1->B3, A2->B1",
        "TEST DUMMY", "DIAGNOSTIC TACTILE",
        "RÉGLAGES", "Réglages appareil, réseau et diagnostic",
        "RÉGLAGES", "Sélectionnez la configuration",
        "Wi-Fi", "Ethernet", "Pas encore configuré",
        "APPRENTISSAGE CÂBLE", "TEST QR / CODE-BARRES", "RAPPORTS",
        "TEST MULTI-CONNECTEUR", "Pas encore disponible",
        "JC1060 prêt | Scanner externe 128 canaux non connecté",
        "Langue", "CHOISIR LA LANGUE", "RETOUR", "DÉMARRER", "PAUSE",
        "SCAN / PAUSE", "DIRECTION",
        "ARRÊT SUR ERREUR", "NON-STOP",
        "COMMANDES", "MENU", "PRÊT", "ANALYSE", "EN PAUSE", "TERMINÉ",
        "SCAN A -> B", "SCAN B -> A", "VITESSE TEST", "PANNEAU VIRTUEL",
        "PAS-À-PAS", "RESET / ANNULER", "SON: OUI", "SON: NON", "PROFIL",
        "CÂBLE SUIVANT", "MODE", "VEILLE", "DIAGNOSTIC", "TEST TACTILE",
        "Vérifie les coordonnées GT911 sur cinq points.", "FERMER",
        "ATTENTE", "ANALYSE", "OK", "OUVERT", "COURT-CIRCUIT",
        "MAUVAISE CONNEXION", "RÉSISTANCE ÉLEVÉE",
    },
    {
        "MG SMART TEST - MENÚ PRINCIPAL",
        "Seleccione la operación deseada",
        "PRUEBA DE CABLE NORMAL",
        "PE + pines ajustables + dS | 1:1 / mapa personalizado",
        "PRUEBA DE CABLE SUB-D",
        "1..N + dS | Perfil Sub-D",
        "MAPA PERSONALIZADO", "Cableado personalizado, ej. A1->B3, A2->B1",
        "PRUEBA DUMMY", "DIAGNÓSTICO TÁCTIL",
        "AJUSTES", "Ajustes de dispositivo, red y diagnóstico",
        "AJUSTES", "Seleccione la configuración",
        "Wi-Fi", "Ethernet", "Aún no configurado",
        "APRENDER CABLE", "PRUEBA QR / CÓDIGO", "INFORMES",
        "PRUEBA MULTICONECTOR", "Todavía no disponible",
        "JC1060 listo | Escáner externo de 128 canales no conectado",
        "Idioma", "SELECCIONAR IDIOMA", "VOLVER", "INICIAR", "PAUSA",
        "SCAN / PAUSA", "DIRECCIÓN",
        "PARAR EN ERROR", "NON-STOP",
        "CONTROLES", "MENÚ", "LISTO", "ESCANEANDO", "PAUSADO", "TERMINADO",
        "ESCANEO A -> B", "ESCANEO B -> A", "VELOCIDAD", "PANEL FRONTAL VIRTUAL",
        "PASO ÚNICO", "RESET / ABORTAR", "SONIDO: SÍ", "SONIDO: NO", "PERFIL",
        "SIGUIENTE CABLE", "MODO", "ESPERA", "DIAGNÓSTICO", "PRUEBA TÁCTIL",
        "Verifica coordenadas GT911 en cinco puntos.", "CERRAR",
        "ESPERA", "ESCANEO", "OK", "CIRCUITO ABIERTO", "CORTOCIRCUITO",
        "CONEXIÓN INCORRECTA", "ALTA RESISTENCIA",
    },
    {
        "MG SMART TEST - MENU GŁÓWNE",
        "Wybierz żądaną operację",
        "NORMALNY TEST KABLA",
        "PE + regulowane piny + dS | 1:1 / mapa niestandardowa",
        "TEST KABLA SUB-D",
        "1..N + dS | Profil Sub-D",
        "MAPA NIESTANDARDOWA", "Niestandardowe połączenia, np. A1->B3, A2->B1",
        "TEST DUMMY", "DIAGNOSTYKA DOTYKU",
        "USTAWIENIA", "Ustawienia urządzenia, sieci i diagnostyki",
        "USTAWIENIA", "Wybierz konfigurację urządzenia",
        "Wi-Fi", "Ethernet", "Jeszcze nieskonfigurowane",
        "UCZENIE KABLA", "TEST QR / KODU", "RAPORTY", "TEST WIELOZŁĄCZOWY",
        "Jeszcze niedostępne",
        "JC1060 gotowy | Zewnętrzny skaner 128 kanałów odłączony",
        "Język", "WYBIERZ JĘZYK", "WSTECZ", "START", "PAUZA",
        "SKAN / PAUZA", "KIERUNEK",
        "STOP PRZY BŁĘDZIE", "NON-STOP",
        "STEROWANIE", "MENU", "GOTOWY", "SKANOWANIE", "PAUZA", "ZAKOŃCZONO",
        "SKAN A -> B", "SKAN B -> A", "PRĘDKOŚĆ TESTU", "WIRTUALNY PANEL",
        "JEDEN KROK", "RESET / PRZERWIJ", "DŹWIĘK: TAK", "DŹWIĘK: NIE", "PROFIL",
        "NASTĘPNY KABEL", "TRYB", "CZUWANIE", "DIAGNOSTYKA", "TEST DOTYKU",
        "Sprawdza współrzędne GT911 w pięciu punktach.", "ZAMKNIJ",
        "OCZEKUJE", "SKAN", "OK", "PRZERWA", "ZWARCIE",
        "BŁĘDNE POŁĄCZENIE", "WYSOKI OPÓR",
    },
};

static_assert(sizeof(kTexts) / sizeof(kTexts[0]) ==
                  static_cast<uint8_t>(P4Language::Count),
              "Language table and P4Language enum differ");

const char* const kLanguageNames[] = {
    "Türkçe", "English", "Nederlands", "Deutsch",
    "Français", "Español", "Polski"};

}  // namespace

void setP4Language(P4Language language) {
    if (isValidP4Language(static_cast<uint8_t>(language))) {
        activeLanguage = language;
    }
}

P4Language currentP4Language() {
    return activeLanguage;
}

const P4UiTexts& p4Texts() {
    return kTexts[static_cast<uint8_t>(activeLanguage)];
}

const char* p4SelectText(const char* turkish,
                         const char* english,
                         const char* dutch,
                         const char* german,
                         const char* french,
                         const char* spanish,
                         const char* polish) {
    switch (activeLanguage) {
        case P4Language::English: return english;
        case P4Language::Dutch: return dutch;
        case P4Language::German: return german;
        case P4Language::French: return french;
        case P4Language::Spanish: return spanish;
        case P4Language::Polish: return polish;
        case P4Language::Turkish:
        case P4Language::Count:
        default: return turkish;
    }
}

const char* p4LanguageName(P4Language language) {
    const uint8_t index = static_cast<uint8_t>(language);
    return isValidP4Language(index) ? kLanguageNames[index] : "--";
}

bool isValidP4Language(uint8_t value) {
    return value < static_cast<uint8_t>(P4Language::Count);
}

}  // namespace mg::p4
