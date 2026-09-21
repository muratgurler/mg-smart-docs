#include "AdvancedAnalysisScreen.h"

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

lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* font, lv_color_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, color, LV_PART_MAIN);
    return l;
}

const char* pageTitle() {
    return p4SelectText("KABLO ANALİZİ", "CABLE ANALYSIS", "KABELANALYSE",
                        "KABELANALYSE", "ANALYSE DU CÂBLE", "ANÁLISIS DE CABLE",
                        "ANALIZA KABLA");
}

const char* pageSubtitle() {
    return p4SelectText("Yapmak istediğiniz testi seçin",
                        "Select the test you want to perform",
                        "Kies de test die u wilt uitvoeren",
                        "Gewünschten Test auswählen",
                        "Sélectionnez le test à effectuer",
                        "Seleccione la prueba que desea realizar",
                        "Wybierz test do wykonania");
}





}  // namespace

void AdvancedAnalysisScreen::begin(v33::V33HardwareManager& hardware) {
    backRequested_ = false;
    connectionScanRequested_ = false;
    backboneRequested_ = false;
    requestedWorkflowTitle_ = nullptr;
    dialogOverlay_ = nullptr;

    if (!screen_) screen_ = lv_obj_create(nullptr); else lv_obj_clean(screen_);
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

    lv_obj_t* t = label(header, pageTitle(), p4Font20(), lv_color_hex(0xEAF6FF));
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_t* s = label(header, pageSubtitle(), p4Font14(), lv_color_hex(0x9CC6DC));
    lv_obj_align(s, LV_ALIGN_BOTTOM_MID, 0, -9);

    constexpr lv_coord_t buttonW = 308;
    constexpr lv_coord_t buttonH = 128;
    constexpr lv_coord_t firstX = 22;
    constexpr lv_coord_t firstY = 92;
    constexpr lv_coord_t colPitch = 334;
    constexpr lv_coord_t rowPitch = 145;

    addActionButton(screen_, firstX, firstY, buttonW, buttonH,
                    actionTitle(OperatorAction::FullCableTest),
                    actionSubtitle(OperatorAction::FullCableTest),
                    lv_color_hex(0x147A4B), OperatorAction::FullCableTest);

    addActionButton(screen_, firstX + colPitch, firstY, buttonW, buttonH,
                    actionTitle(OperatorAction::ConnectionFaults),
                    actionSubtitle(OperatorAction::ConnectionFaults),
                    lv_color_hex(0x315B8A), OperatorAction::ConnectionFaults);

    addActionButton(screen_, firstX + 2 * colPitch, firstY, buttonW, buttonH,
                    actionTitle(OperatorAction::Resistance),
                    actionSubtitle(OperatorAction::Resistance),
                    lv_color_hex(0x7A5B17), OperatorAction::Resistance);

    addActionButton(screen_, firstX, firstY + rowPitch, buttonW, buttonH,
                    actionTitle(OperatorAction::CrimpContact),
                    actionSubtitle(OperatorAction::CrimpContact),
                    lv_color_hex(0x8A4F17), OperatorAction::CrimpContact);

    addActionButton(screen_, firstX + colPitch, firstY + rowPitch, buttonW, buttonH,
                    actionTitle(OperatorAction::IntermittentContact),
                    actionSubtitle(OperatorAction::IntermittentContact),
                    lv_color_hex(0x75508C), OperatorAction::IntermittentContact);

    addActionButton(screen_, firstX + 2 * colPitch, firstY + rowPitch, buttonW, buttonH,
                    actionTitle(OperatorAction::FindOpenLocation),
                    actionSubtitle(OperatorAction::FindOpenLocation),
                    lv_color_hex(0x9A3D35), OperatorAction::FindOpenLocation);

    addActionButton(screen_, firstX, firstY + 2 * rowPitch, buttonW, buttonH,
                    actionTitle(OperatorAction::TwistedPair),
                    actionSubtitle(OperatorAction::TwistedPair),
                    lv_color_hex(0x166E7A), OperatorAction::TwistedPair);

    addActionButton(screen_, firstX + colPitch, firstY + 2 * rowPitch, buttonW, buttonH,
                    actionTitle(OperatorAction::Calibration),
                    actionSubtitle(OperatorAction::Calibration),
                    lv_color_hex(0x4E6170), OperatorAction::Calibration);

    lv_obj_t* back = lv_btn_create(screen_);
    lv_obj_set_pos(back, firstX + 2 * colPitch, firstY + 2 * rowPitch);
    lv_obj_set_size(back, buttonW, buttonH);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x5E426F), LV_PART_MAIN);
    lv_obj_set_style_radius(back, 12, LV_PART_MAIN);
    lv_obj_add_event_cb(back, backCallback, LV_EVENT_CLICKED, this);
    lv_obj_t* bt = label(back, p4Texts().back, p4Font20(), lv_color_hex(0xFFFFFF));
    lv_obj_center(bt);

    Serial.println("[V33][STAGE02][UI] operator Cable Analysis menu ready");
    Serial.printf("[V33][STAGE02] external hardware=%s\n", hardware.hardwareEnabled() ? "ENABLED" : "DISABLED");
    Serial.flush();
}

lv_obj_t* AdvancedAnalysisScreen::addActionButton(lv_obj_t* parent,
                                                   lv_coord_t x,
                                                   lv_coord_t y,
                                                   lv_coord_t w,
                                                   lv_coord_t h,
                                                   const char* title,
                                                   const char* subtitle,
                                                   lv_color_t color,
                                                   OperatorAction action) {
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

const char* AdvancedAnalysisScreen::actionTitle(OperatorAction action) const {
    switch (action) {
        case OperatorAction::FullCableTest:
            return p4SelectText("TAM KABLO TESTİ", "FULL CABLE TEST", "VOLLEDIGE KABELTEST",
                                "VOLLSTÄNDIGER KABELTEST", "TEST COMPLET DU CÂBLE",
                                "PRUEBA COMPLETA DEL CABLE", "PEŁNY TEST KABLA");
        case OperatorAction::ConnectionFaults:
            return p4SelectText("BAĞLANTI HATALARI", "CONNECTION FAULTS", "VERBINDINGSFOUTEN",
                                "VERBINDUNGSFEHLER", "DÉFAUTS DE CONNEXION",
                                "FALLOS DE CONEXIÓN", "BŁĘDY POŁĄCZEŃ");
        case OperatorAction::Resistance:
            return p4SelectText("DİRENÇ ÖLÇÜMÜ", "RESISTANCE TEST", "WEERSTANDSMETING",
                                "WIDERSTANDSMESSUNG", "MESURE DE RÉSISTANCE",
                                "MEDICIÓN DE RESISTENCIA", "POMIAR REZYSTANCJI");
        case OperatorAction::CrimpContact:
            return p4SelectText("KRİMP / TEMAS KALİTESİ", "CRIMP / CONTACT QUALITY",
                                "KRIMP-/CONTACTKWALITEIT", "CRIMP-/KONTAKTQUALITÄT",
                                "QUALITÉ SERTISSAGE / CONTACT", "CALIDAD CRIMPADO / CONTACTO",
                                "JAKOŚĆ ZACISKU / STYKU");
        case OperatorAction::IntermittentContact:
            return p4SelectText("ANLIK TEMASSIZLIK", "INTERMITTENT CONTACT", "INTERMITTEREND CONTACT",
                                "WACKELKONTAKT", "CONTACT INTERMITTENT",
                                "CONTACTO INTERMITENTE", "PRZERYWANY STYK");
        case OperatorAction::FindOpenLocation:
            return p4SelectText("KOPUK YERİNİ BUL", "FIND OPEN LOCATION", "BREUKLOCATIE VINDEN",
                                "UNTERBRECHUNGSORT FINDEN", "LOCALISER LA COUPURE",
                                "LOCALIZAR EL CORTE", "ZNAJDŹ MIEJSCE PRZERWY");
        case OperatorAction::TwistedPair:
            return p4SelectText("BÜKÜMLÜ ÇİFT KONTROLÜ", "TWISTED-PAIR CHECK",
                                "TWISTED-PAIRCONTROLE", "TWISTED-PAIR-PRÜFUNG",
                                "CONTRÔLE PAIRE TORSADÉE", "COMPROBACIÓN DE PAR TRENZADO",
                                "KONTROLA PARY SKRĘCANEJ");
        case OperatorAction::Calibration:
            return p4SelectText("KALİBRASYON", "CALIBRATION", "KALIBRATIE", "KALIBRIERUNG",
                                "ÉTALONNAGE", "CALIBRACIÓN", "KALIBRACJA");
    }
    return "";
}

const char* AdvancedAnalysisScreen::actionSubtitle(OperatorAction action) const {
    switch (action) {
        case OperatorAction::FullCableTest:
            return p4SelectText("Kabloyu baştan sona otomatik kontrol et",
                                "Run all automatic cable checks",
                                "Voer alle kabelcontroles automatisch uit",
                                "Alle Kabelprüfungen automatisch ausführen",
                                "Exécuter automatiquement tous les contrôles",
                                "Ejecutar automáticamente todas las comprobaciones",
                                "Wykonaj automatycznie wszystkie kontrole kabla");
        case OperatorAction::ConnectionFaults:
            return p4SelectText("Kopuk, kısa devre ve yanlış bağlantıyı bul",
                                "Find opens, shorts and wrong wiring",
                                "Vind onderbrekingen, kortsluitingen en verkeerde bedrading",
                                "Unterbrechungen, Kurzschlüsse und Fehlverdrahtung finden",
                                "Détecter coupures, courts-circuits et mauvais câblage",
                                "Detectar cortes, cortos y cableado incorrecto",
                                "Znajdź przerwy, zwarcia i błędne połączenia");
        case OperatorAction::Resistance:
            return p4SelectText("Hat direncini mOhm olarak ölç", "Measure cable-line resistance in mOhm",
                                "Meet lijnweerstand in mOhm", "Leitungswiderstand in mOhm messen",
                                "Mesurer la résistance de ligne en mOhm", "Medir resistencia de línea en mOhm",
                                "Zmierz rezystancję linii w mOhm");
        case OperatorAction::CrimpContact:
            return p4SelectText("Yüksek direnç ve şüpheli krimpi bul", "Find high resistance and suspect crimps",
                                "Vind hoge weerstand en verdachte krimps", "Hohen Widerstand und verdächtige Crimps finden",
                                "Détecter résistance élevée et sertissages suspects", "Detectar alta resistencia y crimpados sospechosos",
                                "Znajdź wysoką rezystancję i podejrzane zaciski");
        case OperatorAction::IntermittentContact:
            return p4SelectText("Kablo hareket ederken kesilmeleri yakala", "Catch dropouts while moving the cable",
                                "Detecteer uitval terwijl de kabel beweegt", "Aussetzer bei Kabelbewegung erfassen",
                                "Détecter les coupures pendant le mouvement", "Detectar cortes al mover el cable",
                                "Wykryj zaniki podczas poruszania kablem");
        case OperatorAction::FindOpenLocation:
            return p4SelectText("Kopukluğun kablodaki yaklaşık mesafesini bul", "Locate the approximate distance to an open",
                                "Bepaal de geschatte afstand tot de breuk", "Ungefähre Entfernung zur Unterbrechung bestimmen",
                                "Estimer la distance jusqu'à la coupure", "Estimar la distancia hasta el corte",
                                "Określ przybliżoną odległość do przerwy");
        case OperatorAction::TwistedPair:
            return p4SelectText("Yanlış çiftleme / split-pair hatasını ara", "Check pairing and split-pair faults",
                                "Controleer paarindeling en split-pairfouten", "Paarzuordnung und Split-Pair-Fehler prüfen",
                                "Contrôler l'appariement et les split-pairs", "Comprobar emparejado y fallos split-pair",
                                "Sprawdź parowanie i błędy split-pair");
        case OperatorAction::Calibration:
            return p4SelectText("Ölçüm referanslarını kontrol et ve ayarla", "Check and adjust measurement references",
                                "Controleer en stel meetreferenties af", "Messreferenzen prüfen und einstellen",
                                "Contrôler et régler les références de mesure", "Comprobar y ajustar referencias de medida",
                                "Sprawdź i ustaw referencje pomiarowe");
    }
    return "";
}

const char* AdvancedAnalysisScreen::actionInstructions(OperatorAction action) const {
    switch (action) {
        case OperatorAction::FullCableTest:
            return p4SelectText(
                "Kablonun A ve B uçlarını bağlayın. Cihaz bağlantı, direnç, krimp/temas ve profil varsa bükümlü çift kontrollerini sırayla yapar.",
                "Connect cable ends A and B. The tester runs connection, resistance, crimp/contact and configured twisted-pair checks in sequence.",
                "Sluit kabeluiteinden A en B aan. De tester voert achtereenvolgens verbinding, weerstand, krimp/contact en ingestelde twisted-paircontroles uit.",
                "Kabelenden A und B anschließen. Das Gerät prüft nacheinander Verbindung, Widerstand, Crimp/Kontakt und konfigurierte Twisted Pairs.",
                "Branchez les extrémités A et B. Le testeur contrôle successivement connexion, résistance, sertissage/contact et paires torsadées configurées.",
                "Conecte los extremos A y B. El equipo comprueba en secuencia conexión, resistencia, crimpado/contacto y pares trenzados configurados.",
                "Podłącz końce A i B. Tester kolejno sprawdza połączenia, rezystancję, zaciski/styki i skonfigurowane pary skręcane.");
        case OperatorAction::ConnectionFaults:
            return p4SelectText(
                "Kablonun iki ucunu bağlayın. Cihaz kopuk, kısa devre, yanlış pin ve çapraz bağlantıları otomatik tarar.",
                "Connect both cable ends. The tester automatically scans for opens, shorts, wrong pins and crossed wiring.",
                "Sluit beide kabeluiteinden aan. De tester scant automatisch op onderbrekingen, kortsluitingen, verkeerde pinnen en kruisverbindingen.",
                "Beide Kabelenden anschließen. Das Gerät sucht automatisch nach Unterbrechungen, Kurzschlüssen, falschen Pins und Kreuzverdrahtung.",
                "Branchez les deux extrémités. Le testeur recherche automatiquement coupures, courts-circuits, mauvaises broches et croisements.",
                "Conecte ambos extremos. El equipo busca automáticamente cortes, cortos, pines incorrectos y cruces de cableado.",
                "Podłącz oba końce kabla. Tester automatycznie wykrywa przerwy, zwarcia, błędne piny i połączenia krzyżowe.");
        case OperatorAction::Resistance:
            return p4SelectText(
                "Kablonun iki ucunu bağlayın. Doğru bağlı hatların direnci tek tek ölçülür ve mOhm olarak gösterilir.",
                "Connect both cable ends. Resistance of correctly connected lines is measured one by one and displayed in mOhm.",
                "Sluit beide kabeluiteinden aan. De weerstand van correct aangesloten lijnen wordt één voor één gemeten en in mOhm getoond.",
                "Beide Kabelenden anschließen. Der Widerstand korrekt verbundener Leitungen wird einzeln gemessen und in mOhm angezeigt.",
                "Branchez les deux extrémités. La résistance des lignes correctement connectées est mesurée individuellement en mOhm.",
                "Conecte ambos extremos. La resistencia de las líneas correctas se mide una a una y se muestra en mOhm.",
                "Podłącz oba końce. Rezystancja prawidłowo połączonych linii jest mierzona osobno i wyświetlana w mOhm.");
        case OperatorAction::CrimpContact:
            return p4SelectText(
                "Hat dirençleri referansla karşılaştırılır; yüksek dirençli krimp veya kontaklar şüpheli olarak işaretlenir.",
                "Line resistance is compared with the reference; high-resistance crimps or contacts are flagged as suspect.",
                "Lijnweerstand wordt met de referentie vergeleken; krimps of contacten met hoge weerstand worden gemarkeerd.",
                "Leitungswiderstände werden mit der Referenz verglichen; hochohmige Crimps oder Kontakte werden markiert.",
                "La résistance des lignes est comparée à la référence; les sertissages ou contacts trop résistifs sont signalés.",
                "La resistencia de línea se compara con la referencia; los crimpados o contactos de alta resistencia se marcan.",
                "Rezystancja linii jest porównywana z referencją; zaciski lub styki o wysokiej rezystancji są oznaczane.");
        case OperatorAction::IntermittentContact:
            return p4SelectText(
                "Kabloyu ve konnektörleri hafifçe hareket ettirin. Anlık temas kaybı olursa olay ve sorunlu hat kaydedilir.",
                "Move the cable and connectors gently. Any short contact dropout is captured and the affected line is reported.",
                "Beweeg kabel en connectoren voorzichtig. Korte contactuitval wordt vastgelegd met de betreffende lijn.",
                "Kabel und Stecker leicht bewegen. Kurze Kontaktunterbrechungen werden mit der betroffenen Leitung erfasst.",
                "Bougez légèrement le câble et les connecteurs. Toute perte de contact brève est enregistrée avec la ligne concernée.",
                "Mueva suavemente el cable y los conectores. Cualquier pérdida breve de contacto se registra con la línea afectada.",
                "Lekko poruszaj kablem i złączami. Krótkie zaniki styku są rejestrowane wraz z linią.");
        case OperatorAction::FindOpenLocation:
            return p4SelectText(
                "Önce kopuk hat belirlenir. Sonra seçilen hatta otomatik mesafe ölçümü yapılarak kopukluğun A konnektöründen yaklaşık uzaklığı gösterilir.",
                "After an open line is identified, the tester automatically measures the approximate distance from connector A to the fault.",
                "Na identificatie van de onderbroken lijn meet de tester automatisch de geschatte afstand vanaf connector A tot de fout.",
                "Nach Erkennung der unterbrochenen Leitung misst das Gerät automatisch die ungefähre Entfernung von Stecker A zur Fehlerstelle.",
                "Après identification de la ligne coupée, le testeur mesure automatiquement la distance approximative depuis le connecteur A.",
                "Tras identificar la línea cortada, el equipo mide automáticamente la distancia aproximada desde el conector A hasta el fallo.",
                "Po wykryciu przerwanej linii tester automatycznie mierzy przybliżoną odległość od złącza A do uszkodzenia.");
        case OperatorAction::TwistedPair:
            return p4SelectText(
                "Profilde bükümlü çift olarak tanımlanan hatlar kontrol edilir. Pin sürekliliği doğru olsa bile yanlış çiftleme / split-pair aranır.",
                "Checks profile-defined twisted pairs. Wrong pairing / split-pair faults can be found even when pin continuity is correct.",
                "Controleert twisted pairs uit het profiel. Verkeerde paren / split-pairfouten worden gevonden, ook bij correcte pincontinuïteit.",
                "Prüft die im Profil definierten Twisted Pairs. Falsche Paarung / Split Pair wird auch bei korrekter Pin-Durchgängigkeit erkannt.",
                "Contrôle les paires torsadées du profil. Les mauvais appariements / split-pairs sont détectés même si la continuité des broches est correcte.",
                "Comprueba los pares trenzados del perfil. Detecta emparejado incorrecto / split-pair aunque la continuidad de pines sea correcta.",
                "Sprawdza pary skręcane z profilu. Błędne parowanie / split-pair jest wykrywane nawet przy poprawnej ciągłości pinów.");
        case OperatorAction::Calibration:
            return p4SelectText(
                "Servis işlevidir. Bilinen referanslar direnç, TDR ve çift analizi kalibrasyonunu doğrulamak için kullanılır.",
                "Service function. Known references are used to verify resistance, TDR and pair-analysis calibration.",
                "Servicefunctie. Bekende referenties worden gebruikt om weerstand-, TDR- en paaranalysekalibratie te verifiëren.",
                "Servicefunktion. Bekannte Referenzen prüfen die Kalibrierung von Widerstand, TDR und Paaranalyse.",
                "Fonction de service. Des références connues vérifient l'étalonnage résistance, TDR et analyse de paires.",
                "Función de servicio. Referencias conocidas verifican la calibración de resistencia, TDR y análisis de pares.",
                "Funkcja serwisowa. Znane wzorce służą do weryfikacji kalibracji rezystancji, TDR i analizy par.");
    }
    return "";
}

void AdvancedAnalysisScreen::showActionDialog(OperatorAction action) {
    closeActionDialog();

    dialogOverlay_ = lv_obj_create(screen_);
    lv_obj_set_pos(dialogOverlay_, 0, 0);
    lv_obj_set_size(dialogOverlay_, kDisplayWidth, kDisplayHeight);
    flat(dialogOverlay_);
    lv_obj_set_style_bg_color(dialogOverlay_, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dialogOverlay_, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(dialogOverlay_, 0, LV_PART_MAIN);

    lv_obj_t* panel = lv_obj_create(dialogOverlay_);
    lv_obj_set_size(panel, 800, 390);
    lv_obj_center(panel);
    flat(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x102431), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x4C7A91), LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 14, LV_PART_MAIN);

    lv_obj_t* title = label(panel, actionTitle(action), p4Font20(), lv_color_hex(0xFFFFFF));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t* instructions = label(panel, actionInstructions(action), p4Font20(), lv_color_hex(0xD7EAF4));
    lv_obj_set_width(instructions, 710);
    lv_label_set_long_mode(instructions, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(instructions, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(instructions, LV_ALIGN_TOP_MID, 0, 80);

    lv_obj_t* status = label(panel,
        p4SelectText("Test, V3.3 harici ölçüm kartı bağlandığında aktif olur.",
                     "This test becomes active when the V3.3 external measurement board is connected.",
                     "Deze test wordt actief zodra de externe V3.3-meetkaart is aangesloten.",
                     "Dieser Test wird aktiv, sobald die externe V3.3-Messkarte angeschlossen ist.",
                     "Ce test devient actif lorsque la carte de mesure externe V3.3 est connectée.",
                     "Esta prueba se activa cuando se conecta la tarjeta de medida externa V3.3.",
                     "Ten test staje się aktywny po podłączeniu zewnętrznej karty pomiarowej V3.3."),
        p4Font14(), lv_color_hex(0xFFD07A));
    lv_obj_set_width(status, 690);
    lv_label_set_long_mode(status, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -90);

    lv_obj_t* close = lv_btn_create(panel);
    lv_obj_set_size(close, 220, 62);
    lv_obj_align(close, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_set_style_bg_color(close, lv_color_hex(0x315B8A), LV_PART_MAIN);
    lv_obj_set_style_radius(close, 10, LV_PART_MAIN);
    lv_obj_add_event_cb(close, closeDialogCallback, LV_EVENT_CLICKED, this);
    lv_obj_t* cl = label(close, p4SelectText("TAMAM", "OK", "OK", "OK", "OK", "OK", "OK"), p4Font20(), lv_color_hex(0xFFFFFF));
    lv_obj_center(cl);
}

void AdvancedAnalysisScreen::closeActionDialog() {
    if (dialogOverlay_) {
        lv_obj_del(dialogOverlay_);
        dialogOverlay_ = nullptr;
    }
}

void AdvancedAnalysisScreen::activate() {
    backRequested_ = false;
    if (screen_ && lv_scr_act() != screen_) lv_scr_load(screen_);
}

void AdvancedAnalysisScreen::update(v33::V33HardwareManager& hardware) {
    (void)hardware;
}

void AdvancedAnalysisScreen::backCallback(lv_event_t* event) {
    auto* self = static_cast<AdvancedAnalysisScreen*>(lv_event_get_user_data(event));
    if (!self) return;
    if (self->dialogOverlay_) {
        self->closeActionDialog();
        return;
    }
    self->backRequested_ = true;
}

void AdvancedAnalysisScreen::actionCallback(lv_event_t* event) {
    auto* ctx = static_cast<ActionContext*>(lv_event_get_user_data(event));
    if (!ctx || !ctx->owner) return;
    AdvancedAnalysisScreen* self = ctx->owner;
    Serial.printf("[V33][STAGE02][UI] operator action=%u\n", static_cast<unsigned>(ctx->action));

    if (ctx->action == OperatorAction::ConnectionFaults) {
        self->connectionScanRequested_ = true;
        Serial.println("[V33][STAGE02][UI] existing 128-node scan demo requested");
        Serial.flush();
        return;
    }

    self->backboneRequested_ = true;
    self->requestedWorkflowTitle_ = self->actionTitle(ctx->action);
    switch (ctx->action) {
        case OperatorAction::FullCableTest:
            self->requestedModule_ = BackboneModuleId::Scan128;
            break;
        case OperatorAction::Resistance:
        case OperatorAction::CrimpContact:
            self->requestedModule_ = BackboneModuleId::Kelvin;
            break;
        case OperatorAction::IntermittentContact:
            self->requestedModule_ = BackboneModuleId::FlexGlitch;
            break;
        case OperatorAction::FindOpenLocation:
            self->requestedModule_ = BackboneModuleId::Tdr;
            break;
        case OperatorAction::TwistedPair:
            self->requestedModule_ = BackboneModuleId::PairIntegrity;
            break;
        case OperatorAction::Calibration:
            self->requestedModule_ = BackboneModuleId::SelfTestCalibration;
            break;
        case OperatorAction::ConnectionFaults:
            break;
    }
    Serial.printf("[V33][STAGE02][UI] backbone module=%u requested\n",
                  static_cast<unsigned>(self->requestedModule_));
    Serial.flush();
}

void AdvancedAnalysisScreen::closeDialogCallback(lv_event_t* event) {
    auto* self = static_cast<AdvancedAnalysisScreen*>(lv_event_get_user_data(event));
    if (self) self->closeActionDialog();
}

bool AdvancedAnalysisScreen::consumeBackRequest() {
    const bool value = backRequested_;
    backRequested_ = false;
    return value;
}

bool AdvancedAnalysisScreen::consumeConnectionScanRequest() {
    const bool value = connectionScanRequested_;
    connectionScanRequested_ = false;
    return value;
}

bool AdvancedAnalysisScreen::consumeBackboneRequest(BackboneModuleId& module,
                                                     const char*& workflowTitle) {
    if (!backboneRequested_) return false;
    backboneRequested_ = false;
    module = requestedModule_;
    workflowTitle = requestedWorkflowTitle_;
    requestedWorkflowTitle_ = nullptr;
    return true;
}

}  // namespace mg::p4
