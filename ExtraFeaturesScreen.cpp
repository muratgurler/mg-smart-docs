#include "ExtraFeaturesScreen.h"

#include <Arduino.h>

#include "ConfigP4.h"
#include "P4Fonts.h"
#include "P4Localization.h"

namespace mg::p4 {
namespace {

void flat(lv_obj_t* o) {
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
}

lv_obj_t* label(lv_obj_t* parent,
                const char* text,
                const lv_font_t* font,
                lv_color_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, color, LV_PART_MAIN);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}

const char* extraPageTitle() {
    return p4SelectText("EKSTRA ÖZELLİKLER", "EXTRA FEATURES", "EXTRA FUNCTIES",
                        "EXTRA-FUNKTIONEN", "FONCTIONS SUPPLÉMENTAIRES",
                        "FUNCIONES EXTRA", "FUNKCJE DODATKOWE");
}

const char* extraPageSubtitle() {
    return p4SelectText("V3.3 için yardımcı üretim ve servis fonksiyonları",
                        "V3.3 production and service helper functions",
                        "Hulpfuncties voor productie en service van V3.3",
                        "V3.3 Hilfsfunktionen für Produktion und Service",
                        "Fonctions d'aide production et service V3.3",
                        "Funciones auxiliares de producción y servicio V3.3",
                        "Funkcje pomocnicze produkcji i serwisu V3.3");
}





}  // namespace

void ExtraFeaturesScreen::begin() {
    backRequested_ = false;
    documentImportRequested_ = false;
    backboneRequested_ = false;
    requestedWorkflowTitle_ = nullptr;
    dialogOverlay_ = nullptr;

    if (!screen_) {
        screen_ = lv_obj_create(nullptr);
    } else {
        lv_obj_clean(screen_);
    }
    lv_scr_load(screen_);
    flat(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x081119), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* header = lv_obj_create(screen_);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, kDisplayWidth, 78);
    flat(header);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x142B3A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);

    lv_obj_t* title = label(header,
                            extraPageTitle(),
                            p4Font20(),
                            lv_color_hex(0xEAF6FF));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_t* subtitle = label(
        header,
        extraPageSubtitle(),
        p4Font14(), lv_color_hex(0x9CC6DC));
    lv_obj_align(subtitle, LV_ALIGN_BOTTOM_MID, 0, -9);

    constexpr lv_coord_t buttonW = 308;
    constexpr lv_coord_t buttonH = 128;
    constexpr lv_coord_t firstX = 22;
    constexpr lv_coord_t firstY = 92;
    constexpr lv_coord_t colPitch = 334;
    constexpr lv_coord_t rowPitch = 145;

    addActionButton(screen_, firstX, firstY, buttonW, buttonH,
                    actionTitle(ExtraAction::SelfTest),
                    actionSubtitle(ExtraAction::SelfTest),
                    lv_color_hex(0x147A4B), ExtraAction::SelfTest);

    addActionButton(screen_, firstX + colPitch, firstY, buttonW, buttonH,
                    actionTitle(ExtraAction::DocumentImport),
                    actionSubtitle(ExtraAction::DocumentImport),
                    lv_color_hex(0x315B8A), ExtraAction::DocumentImport);

    addActionButton(screen_, firstX + 2 * colPitch, firstY, buttonW, buttonH,
                    actionTitle(ExtraAction::SmartProbe),
                    actionSubtitle(ExtraAction::SmartProbe),
                    lv_color_hex(0x166E7A), ExtraAction::SmartProbe);

    addActionButton(screen_, firstX, firstY + rowPitch, buttonW, buttonH,
                    actionTitle(ExtraAction::ShieldQuality),
                    actionSubtitle(ExtraAction::ShieldQuality),
                    lv_color_hex(0x7A5B17), ExtraAction::ShieldQuality);

    addActionButton(screen_, firstX + colPitch, firstY + rowPitch, buttonW, buttonH,
                    actionTitle(ExtraAction::SpcTrend),
                    actionSubtitle(ExtraAction::SpcTrend),
                    lv_color_hex(0x8A4F17), ExtraAction::SpcTrend);

    addActionButton(screen_, firstX + 2 * colPitch, firstY + rowPitch, buttonW, buttonH,
                    actionTitle(ExtraAction::VoiceSound),
                    actionSubtitle(ExtraAction::VoiceSound),
                    lv_color_hex(0x75508C), ExtraAction::VoiceSound);

    addActionButton(screen_, firstX, firstY + 2 * rowPitch, buttonW, buttonH,
                    actionTitle(ExtraAction::EnvironmentAwg),
                    actionSubtitle(ExtraAction::EnvironmentAwg),
                    lv_color_hex(0x4E6170), ExtraAction::EnvironmentAwg);

    addActionButton(screen_, firstX + colPitch, firstY + 2 * rowPitch, buttonW, buttonH,
                    actionTitle(ExtraAction::UsbComponent),
                    actionSubtitle(ExtraAction::UsbComponent),
                    lv_color_hex(0x9A3D35), ExtraAction::UsbComponent);

    lv_obj_t* back = lv_btn_create(screen_);
    lv_obj_set_pos(back, firstX + 2 * colPitch, firstY + 2 * rowPitch);
    lv_obj_set_size(back, buttonW, buttonH);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x5E426F), LV_PART_MAIN);
    lv_obj_set_style_radius(back, 12, LV_PART_MAIN);
    lv_obj_add_event_cb(back, backCallback, LV_EVENT_CLICKED, this);
    lv_obj_t* bt = label(back, p4Texts().back, p4Font20(), lv_color_hex(0xFFFFFF));
    lv_obj_center(bt);

    Serial.println("[V33][EXTRA] Extra features menu ready");
    Serial.flush();
}

lv_obj_t* ExtraFeaturesScreen::addActionButton(lv_obj_t* parent,
                                                lv_coord_t x,
                                                lv_coord_t y,
                                                lv_coord_t w,
                                                lv_coord_t h,
                                                const char* title,
                                                const char* subtitle,
                                                lv_color_t color,
                                                ExtraAction action) {
    const uint8_t index = static_cast<uint8_t>(action);
    actionContexts_[index] = {this, action};

    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, h);
    lv_obj_set_style_bg_color(button, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(0x7DA0B3), LV_PART_MAIN);
    lv_obj_add_event_cb(button, actionCallback, LV_EVENT_CLICKED, &actionContexts_[index]);

    lv_obj_t* tl = label(button, title, p4Font20(), lv_color_hex(0xFFFFFF));
    lv_obj_set_width(tl, w - 20);
    lv_obj_set_style_text_align(tl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(tl, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t* sl = label(button, subtitle, p4Font14(), lv_color_hex(0xE1EDF4));
    lv_obj_set_width(sl, w - 26);
    lv_label_set_long_mode(sl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(sl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(sl, LV_ALIGN_BOTTOM_MID, 0, -17);
    return button;
}

const char* ExtraFeaturesScreen::actionTitle(ExtraAction action) const {
    switch (action) {
        case ExtraAction::SelfTest: return "SELF TEST";
        case ExtraAction::DocumentImport:
            return p4SelectText("MOBİL PROFİL", "MOBILE PROFILE", "MOBIEL PROFIEL", "MOBILPROFIL", "PROFIL MOBILE", "PERFIL MÓVIL", "PROFIL MOBILNY");
        case ExtraAction::SmartProbe: return "SMART PROBE";
        case ExtraAction::ShieldQuality:
            return p4SelectText("PE / dS KALİTESİ", "PE / dS QUALITY", "PE / dS-KWALITEIT", "PE / dS-QUALITÄT", "QUALITÉ PE / dS", "CALIDAD PE / dS", "JAKOŚĆ PE / dS");
        case ExtraAction::SpcTrend:
            return p4SelectText("SPC / KRİMP TRENDİ", "SPC / CRIMP TREND", "SPC / KRIMPTREND", "SPC / CRIMP-TREND", "SPC / TENDANCE SERTISSAGE", "SPC / TENDENCIA DE CRIMPADO", "SPC / TREND ZACISKU");
        case ExtraAction::VoiceSound:
            return p4SelectText("SES / KOMUT", "VOICE / SOUND", "SPRAAK / GELUID", "SPRACHE / TON", "VOIX / SON", "VOZ / SONIDO", "GŁOS / DŹWIĘK");
        case ExtraAction::EnvironmentAwg:
            return p4SelectText("ORTAM / AWG", "ENVIRONMENT / AWG", "OMGEVING / AWG", "UMGEBUNG / AWG", "ENVIRONNEMENT / AWG", "ENTORNO / AWG", "ŚRODOWISKO / AWG");
        case ExtraAction::UsbComponent:
            return p4SelectText("USB-C / KOMPONENT", "USB-C / COMPONENT", "USB-C / COMPONENT", "USB-C / KOMPONENTE", "USB-C / COMPOSANT", "USB-C / COMPONENTE", "USB-C / ELEMENT");
    }
    return "";
}

const char* ExtraFeaturesScreen::actionSubtitle(ExtraAction action) const {
    switch (action) {
        case ExtraAction::SelfTest:
            return p4SelectText("Hızlı ve tam cihaz sağlık kontrolü", "Fast and full tester health check", "Snelle en volledige apparaattest", "Schneller und vollständiger Gerätetest", "Contrôle rapide et complet de l'appareil", "Comprobación rápida y completa del equipo", "Szybka i pełna kontrola urządzenia");
        case ExtraAction::DocumentImport:
            return p4SelectText("Android/iOS hazır profil aktarımı", "Android/iOS prepared profile import", "Android/iOS voorbereid profiel", "Android/iOS vorbereitetes Profil", "Profil préparé Android/iOS", "Perfil preparado Android/iOS", "Gotowy profil Android/iOS");
        case ExtraAction::SmartProbe:
            return p4SelectText("Kablo ucunun hangi pine ait olduğunu bul", "Find which pin a loose wire belongs to", "Vind bij welke pin een losse draad hoort", "Pin einer losen Leitung finden", "Identifier la broche d'un fil libre", "Identificar el pin de un cable suelto", "Znajdź pin odpowiadający luźnemu przewodowi");
        case ExtraAction::ShieldQuality:
            return p4SelectText("Shield, drain ve PE direnç kalitesi", "Shield, drain and PE resistance quality", "Weerstandskwaliteit van shield, drain en PE", "Widerstandsqualität von Shield, Drain und PE", "Qualité de résistance shield, drain et PE", "Calidad de resistencia shield, drain y PE", "Jakość rezystancji ekranu, drain i PE");
        case ExtraAction::SpcTrend:
            return p4SelectText("Üretimde mOhm eğilimini ve sapmayı izle", "Track mOhm drift across production", "Volg mOhm-trend en afwijking in productie", "mOhm-Trend und Abweichung in der Produktion verfolgen", "Suivre la dérive mOhm en production", "Seguir tendencia y deriva mOhm en producción", "Śledź trend i odchylenie mOhm w produkcji");
        case ExtraAction::VoiceSound:
            return p4SelectText("Yerel sesli komut ve durum tonları", "Local voice commands and status tones", "Lokale spraakcommando's en statustonen", "Lokale Sprachbefehle und Statustöne", "Commandes vocales locales et tonalités d'état", "Comandos de voz locales y tonos de estado", "Lokalne komendy głosowe i tony stanu");
        case ExtraAction::EnvironmentAwg:
            return p4SelectText("Sıcaklık-nem kaydı ve yaklaşık kesit kontrolü", "Temperature/humidity log and AWG plausibility", "Temperatuur/vochtlog en AWG-plausibiliteit", "Temperatur-/Feuchteprotokoll und AWG-Plausibilität", "Journal température/humidité et plausibilité AWG", "Registro temperatura/humedad y plausibilidad AWG", "Rejestr temperatury/wilgotności i kontrola AWG");
        case ExtraAction::UsbComponent:
            return p4SelectText("E-Marker, diyot, LED, R ve C testleri", "E-Marker, diode, LED, R and C tests", "E-Marker-, diode-, LED-, R- en C-tests", "E-Marker-, Dioden-, LED-, R- und C-Tests", "Tests E-Marker, diode, LED, R et C", "Pruebas E-Marker, diodo, LED, R y C", "Testy E-Marker, diody, LED, R i C");
    }
    return "";
}

const char* ExtraFeaturesScreen::actionInstructions(ExtraAction action) const {
    switch (action) {
        case ExtraAction::SelfTest:
            return p4SelectText(
                "FAST SELF TEST haberleşme, güvenli MUX durumu, beslemeler ve Kelvin ZERO kontrolünü yapar. FULL SELF TEST referansları ve daha geniş yol kontrolünü ekler.",
                "FAST SELF TEST checks communications, safe MUX state, supplies and Kelvin ZERO. FULL SELF TEST adds references and broader path checks.",
                "FAST SELF TEST controleert communicatie, veilige MUX-status, voedingen en Kelvin ZERO. FULL SELF TEST voegt referenties en uitgebreidere padcontroles toe.",
                "FAST SELF TEST prüft Kommunikation, sicheren MUX-Zustand, Versorgungen und Kelvin ZERO. FULL SELF TEST ergänzt Referenzen und weitere Pfade.",
                "FAST SELF TEST contrôle communication, état MUX sûr, alimentations et Kelvin ZERO. FULL SELF TEST ajoute les références et des chemins supplémentaires.",
                "FAST SELF TEST comprueba comunicación, estado seguro del MUX, alimentaciones y Kelvin ZERO. FULL SELF TEST añade referencias y rutas adicionales.",
                "FAST SELF TEST sprawdza komunikację, bezpieczny stan MUX, zasilania i Kelvin ZERO. FULL SELF TEST dodaje wzorce i szerszą kontrolę ścieżek.");
        case ExtraAction::DocumentImport:
            return p4SelectText(
                "Cihaz geçici ve izole bir Wi-Fi ağı açar. Telefon fotoğraf, galeri görüntüsü veya PDF'yi Captive Portal üzerinden doğrudan cihaza gönderir; şirket ağı gerekmez.",
                "The tester opens a temporary isolated Wi-Fi network. A phone uploads a camera photo, gallery image or PDF through the Captive Portal; no company network is required.",
                "De tester opent een tijdelijk geïsoleerd Wi-Fi-netwerk. Een telefoon uploadt foto, galerijbeeld of PDF via het Captive Portal; bedrijfsnetwerk is niet nodig.",
                "Das Gerät öffnet ein temporäres isoliertes Wi-Fi-Netz. Ein Telefon lädt Foto, Galeriebild oder PDF über das Captive Portal hoch; Firmennetz ist nicht erforderlich.",
                "Le testeur ouvre un réseau Wi-Fi temporaire isolé. Le téléphone envoie photo, image ou PDF via le portail captif; le réseau d'entreprise n'est pas requis.",
                "El equipo abre una red Wi-Fi temporal aislada. El teléfono carga foto, imagen o PDF mediante el portal cautivo; no se requiere la red de empresa.",
                "Tester otwiera tymczasową izolowaną sieć Wi-Fi. Telefon przesyła zdjęcie, obraz lub PDF przez portal; sieć firmowa nie jest potrzebna.");
        case ExtraAction::SmartProbe:
            return p4SelectText("Operatör açık tel ucuna probla dokunur; cihaz A/B tarafında hangi test pinine ait olduğunu gösterir.",
                                "Touch a loose wire with the probe; the tester identifies the corresponding A/B test pin.",
                                "Raak een losse draad met de probe aan; de tester toont de bijbehorende A/B-testpin.",
                                "Lose Leitung mit der Probe berühren; das Gerät zeigt den zugehörigen A/B-Testpin.",
                                "Touchez le fil libre avec la sonde; le testeur indique la broche A/B correspondante.",
                                "Toque el cable suelto con la sonda; el equipo identifica el pin A/B correspondiente.",
                                "Dotknij luźnego przewodu sondą; tester wskaże odpowiedni pin testowy A/B.");
        case ExtraAction::ShieldQuality:
            return p4SelectText("PE ve dS bağlantının yanında direnç kalitesiyle değerlendirilir. Bağımsız A/B PE ve dS ayarları tek uçlu shield profillerini destekler.",
                                "PE and dS are evaluated for resistance quality as well as continuity. Independent A/B PE and dS settings support single-ended shield profiles.",
                                "PE en dS worden naast continuïteit op weerstandskwaliteit beoordeeld. Onafhankelijke A/B-instellingen ondersteunen enkelzijdige shields.",
                                "PE und dS werden neben Durchgang auch auf Widerstandsqualität geprüft. Unabhängige A/B-Einstellungen unterstützen einseitige Shields.",
                                "PE et dS sont évalués en continuité et qualité de résistance. Les réglages A/B indépendants gèrent les shields à une seule extrémité.",
                                "PE y dS se evalúan por continuidad y calidad de resistencia. Los ajustes A/B independientes admiten shields de un solo extremo.",
                                "PE i dS są oceniane pod kątem ciągłości i jakości rezystancji. Niezależne ustawienia A/B obsługują ekran jednostronny.");
        case ExtraAction::SpcTrend:
            return p4SelectText("Aynı kablo ailesinde hat direnci trendi izlenir; sert limit aşılmadan krimp, kontak veya proses sapması görülebilir.",
                                "Tracks line-resistance trends within a cable family to expose crimp, contact or process drift before hard limits are exceeded.",
                                "Volgt lijnweerstand binnen een kabelfamilie om krimp-, contact- of procesdrift te zien vóór harde limieten worden overschreden.",
                                "Verfolgt Leitungswiderstand einer Kabelfamilie, um Crimp-, Kontakt- oder Prozessdrift vor Grenzwertüberschreitung zu erkennen.",
                                "Suit la tendance de résistance d'une famille de câbles afin de détecter dérive de sertissage, contact ou procédé avant dépassement des limites.",
                                "Sigue la tendencia de resistencia de una familia de cables para detectar deriva de crimpado, contacto o proceso antes de superar límites.",
                                "Śledzi trend rezystancji w rodzinie kabli, aby wykryć dryf zacisku, styku lub procesu przed przekroczeniem limitu.");
        case ExtraAction::VoiceSound:
            return p4SelectText("Yerel sınırlı sesli komutlar ve PASS, FAIL, OPEN, SHORT için farklı durum tonları kullanılır.",
                                "Uses limited local voice commands and distinct status tones for PASS, FAIL, OPEN and SHORT.",
                                "Gebruikt beperkte lokale spraakcommando's en aparte tonen voor PASS, FAIL, OPEN en SHORT.",
                                "Verwendet begrenzte lokale Sprachbefehle und unterschiedliche Töne für PASS, FAIL, OPEN und SHORT.",
                                "Utilise des commandes vocales locales limitées et des tonalités distinctes pour PASS, FAIL, OPEN et SHORT.",
                                "Usa comandos de voz locales limitados y tonos distintos para PASS, FAIL, OPEN y SHORT.",
                                "Używa ograniczonych lokalnych komend głosowych i różnych tonów dla PASS, FAIL, OPEN i SHORT.");
        case ExtraAction::EnvironmentAwg:
            return p4SelectText("Sıcaklık/nem rapora eklenir. Bilinen uzunluk ve Kelvin direnci varsa iletken kesiti/AWG için yaklaşık uygunluk kontrolü yapılabilir.",
                                "Logs temperature/humidity and can check approximate conductor-size/AWG plausibility from known length and Kelvin resistance.",
                                "Logt temperatuur/vocht en kan de plausibiliteit van aderdoorsnede/AWG controleren uit bekende lengte en Kelvinweerstand.",
                                "Protokolliert Temperatur/Feuchte und kann Leiterquerschnitt/AWG aus bekannter Länge und Kelvin-Widerstand plausibilisieren.",
                                "Enregistre température/humidité et peut vérifier la plausibilité de section/AWG à partir de la longueur et résistance Kelvin.",
                                "Registra temperatura/humedad y puede comprobar sección/AWG aproximada usando longitud y resistencia Kelvin conocidas.",
                                "Rejestruje temperaturę/wilgotność i może sprawdzić przybliżony przekrój/AWG z długości i rezystancji Kelvin.");
        case ExtraAction::UsbComponent:
            return p4SelectText("USB-C E-Marker/PD Cable Identity ile diyot, polarite/Vf, LED, direnç ve uygun kapasite testleri için ortak gelişmiş test alanıdır.",
                                "Advanced area for USB-C E-Marker/PD Cable Identity plus diode, polarity/Vf, LED, resistance and practical capacitance tests.",
                                "Geavanceerd gebied voor USB-C E-Marker/PD Cable Identity plus diode, polariteit/Vf, LED, weerstand en praktische capaciteitstests.",
                                "Erweiterter Bereich für USB-C E-Marker/PD Cable Identity sowie Diode, Polarität/Vf, LED, Widerstand und praktische Kapazitätstests.",
                                "Zone avancée pour USB-C E-Marker/PD Cable Identity ainsi que diode, polarité/Vf, LED, résistance et capacité pratique.",
                                "Área avanzada para USB-C E-Marker/PD Cable Identity, diodo, polaridad/Vf, LED, resistencia y capacitancia práctica.",
                                "Obszar zaawansowany dla USB-C E-Marker/PD Cable Identity oraz diody, polaryzacji/Vf, LED, rezystancji i praktycznej pojemności.");
    }
    return "";
}

const char* ExtraFeaturesScreen::actionStatus(ExtraAction action) const {
    switch (action) {
        case ExtraAction::VoiceSound:
            return p4SelectText("Durum tonları mevcut; sesli komutlar mikrofon/SDK doğrulamasından sonra etkinleştirilecek.",
                                "Status tones exist; voice commands enable after microphone/SDK validation.",
                                "Statustonen zijn beschikbaar; spraakcommando's worden geactiveerd na microfoon/SDK-validatie.",
                                "Statustöne sind verfügbar; Sprachbefehle werden nach Mikrofon-/SDK-Validierung aktiviert.",
                                "Les tonalités sont disponibles; les commandes vocales seront activées après validation micro/SDK.",
                                "Los tonos están disponibles; los comandos de voz se activarán tras validar micrófono/SDK.",
                                "Tony stanu są dostępne; komendy głosowe zostaną włączone po walidacji mikrofonu/SDK.");
        case ExtraAction::UsbComponent:
            return p4SelectText("E-Marker donanımı V3.3 planında; komponent test aralıkları PCB doğrulamasından sonra açılacak.",
                                "E-Marker hardware is in the V3.3 plan; component ranges enable after PCB validation.",
                                "E-Marker-hardware staat in het V3.3-plan; componentbereiken worden actief na PCB-validatie.",
                                "E-Marker-Hardware ist im V3.3-Plan; Komponentenbereiche werden nach PCB-Validierung aktiviert.",
                                "Le matériel E-Marker est prévu en V3.3; les plages composants seront activées après validation PCB.",
                                "El hardware E-Marker está en el plan V3.3; los rangos de componentes se activarán tras validar PCB.",
                                "Sprzęt E-Marker jest w planie V3.3; zakresy testów elementów zostaną włączone po walidacji PCB.");
        default:
            return p4SelectText("Menü altyapısı hazır. İlgili V3.3 donanımı/servis verisi bağlandıkça fonksiyon aktif olacaktır.",
                                "Menu infrastructure is ready. The function becomes active as the relevant V3.3 hardware/service data is connected.",
                                "De menu-infrastructuur is gereed. De functie wordt actief zodra relevante V3.3-hardware/servicegegevens beschikbaar zijn.",
                                "Die Menüstruktur ist bereit. Die Funktion wird aktiv, sobald relevante V3.3-Hardware/Servicedaten angebunden sind.",
                                "L'infrastructure du menu est prête. La fonction s'active lorsque le matériel/service V3.3 correspondant est connecté.",
                                "La infraestructura del menú está lista. La función se activa al conectar el hardware/datos de servicio V3.3 correspondientes.",
                                "Infrastruktura menu jest gotowa. Funkcja aktywuje się po podłączeniu odpowiedniego sprzętu/danych serwisowych V3.3.");
    }
}

void ExtraFeaturesScreen::showActionDialog(ExtraAction action) {
    closeActionDialog();

    dialogOverlay_ = lv_obj_create(screen_);
    lv_obj_set_pos(dialogOverlay_, 0, 0);
    lv_obj_set_size(dialogOverlay_, kDisplayWidth, kDisplayHeight);
    flat(dialogOverlay_);
    lv_obj_set_style_bg_color(dialogOverlay_, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dialogOverlay_, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(dialogOverlay_, 0, LV_PART_MAIN);

    lv_obj_t* panel = lv_obj_create(dialogOverlay_);
    lv_obj_set_size(panel, 820, 410);
    lv_obj_center(panel);
    flat(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x102431), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x4C7A91), LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 14, LV_PART_MAIN);

    lv_obj_t* title = label(panel, actionTitle(action), p4Font20(), lv_color_hex(0xFFFFFF));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 22);

    lv_obj_t* instructions = label(panel, actionInstructions(action), p4Font20(), lv_color_hex(0xD7EAF4));
    lv_obj_set_width(instructions, 730);
    lv_label_set_long_mode(instructions, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(instructions, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(instructions, LV_ALIGN_TOP_MID, 0, 72);

    lv_obj_t* status = label(panel, actionStatus(action), p4Font14(), lv_color_hex(0xFFD07A));
    lv_obj_set_width(status, 710);
    lv_label_set_long_mode(status, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -86);

    lv_obj_t* close = lv_btn_create(panel);
    lv_obj_set_size(close, 220, 60);
    lv_obj_align(close, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_bg_color(close, lv_color_hex(0x315B8A), LV_PART_MAIN);
    lv_obj_set_style_radius(close, 10, LV_PART_MAIN);
    lv_obj_add_event_cb(close, closeDialogCallback, LV_EVENT_CLICKED, this);
    lv_obj_t* cl = label(close, p4SelectText("TAMAM", "OK", "OK", "OK", "OK", "OK", "OK"), p4Font20(), lv_color_hex(0xFFFFFF));
    lv_obj_center(cl);
}

void ExtraFeaturesScreen::closeActionDialog() {
    if (dialogOverlay_) {
        lv_obj_del(dialogOverlay_);
        dialogOverlay_ = nullptr;
    }
}

void ExtraFeaturesScreen::activate() {
    backRequested_ = false;
    if (screen_ && lv_scr_act() != screen_) {
        lv_scr_load(screen_);
    }
}

void ExtraFeaturesScreen::backCallback(lv_event_t* event) {
    auto* self = static_cast<ExtraFeaturesScreen*>(lv_event_get_user_data(event));
    if (!self) return;
    if (self->dialogOverlay_) {
        self->closeActionDialog();
        return;
    }
    self->backRequested_ = true;
}

void ExtraFeaturesScreen::actionCallback(lv_event_t* event) {
    auto* context = static_cast<ActionContext*>(lv_event_get_user_data(event));
    if (!context || !context->owner) return;
    ExtraFeaturesScreen* self = context->owner;
    if (context->action == ExtraAction::DocumentImport) {
        self->documentImportRequested_ = true;
        Serial.println("[V33][EXTRA] Document Import requested");
        Serial.flush();
        return;
    }

    self->backboneRequested_ = true;
    self->requestedWorkflowTitle_ = self->actionTitle(context->action);
    switch (context->action) {
        case ExtraAction::SelfTest:
            self->requestedModule_ = BackboneModuleId::SelfTestCalibration;
            break;
        case ExtraAction::SmartProbe:
            self->requestedModule_ = BackboneModuleId::SmartProbe;
            break;
        case ExtraAction::ShieldQuality:
            self->requestedModule_ = BackboneModuleId::PeDs;
            break;
        case ExtraAction::SpcTrend:
            self->requestedModule_ = BackboneModuleId::FixtureSpc;
            break;
        case ExtraAction::VoiceSound:
            self->requestedModule_ = BackboneModuleId::Voice;
            break;
        case ExtraAction::EnvironmentAwg:
            self->requestedModule_ = BackboneModuleId::EnvironmentAwg;
            break;
        case ExtraAction::UsbComponent:
            self->requestedModule_ = BackboneModuleId::UsbC;
            break;
        case ExtraAction::DocumentImport:
            break;
    }
    Serial.printf("[V33][EXTRA] backbone module=%u requested\n",
                  static_cast<unsigned>(self->requestedModule_));
    Serial.flush();
}

void ExtraFeaturesScreen::closeDialogCallback(lv_event_t* event) {
    auto* self = static_cast<ExtraFeaturesScreen*>(lv_event_get_user_data(event));
    if (self) self->closeActionDialog();
}

bool ExtraFeaturesScreen::consumeBackRequest() {
    const bool value = backRequested_;
    backRequested_ = false;
    return value;
}

bool ExtraFeaturesScreen::consumeDocumentImportRequest() {
    const bool value = documentImportRequested_;
    documentImportRequested_ = false;
    return value;
}

bool ExtraFeaturesScreen::consumeBackboneRequest(BackboneModuleId& module,
                                                  const char*& workflowTitle) {
    if (!backboneRequested_) return false;
    backboneRequested_ = false;
    module = requestedModule_;
    workflowTitle = requestedWorkflowTitle_;
    requestedWorkflowTitle_ = nullptr;
    return true;
}

}  // namespace mg::p4
