#include "MapGraphScreen.h"

#include <stdio.h>

#include "CableNetGraph.h"
#include "ConfigP4.h"
#include "P4Fonts.h"
#include "P4Localization.h"

namespace mg::p4 {
namespace {

struct GraphText {
    const char* title;
    const char* subtitle;
    const char* aSide;
    const char* bSide;
    const char* oneToOne;
    const char* custom;
    const char* multi;
    const char* back;
    const char* pointsFmt;
};

const GraphText& graphText() {
    static const GraphText tr{
        "KABLO BAĞLANTI HARİTASI",
        "Editördeki mevcut A -> B bağlantılarının grafik önizlemesi",
        "A TARAFI", "B TARAFI",
        "Beyaz: 1:1", "Yeşil: özel bağlantı", "Sarı: ortak / çoklu net",
        "GERİ", "%u pin%s%s"};
    static const GraphText en{
        "CABLE CONNECTION MAP",
        "Graph preview of the current A -> B connections in the editor",
        "SIDE A", "SIDE B",
        "White: 1:1", "Green: custom", "Yellow: common / multi net",
        "BACK", "%u pins%s%s"};
    static const GraphText nl{
        "KABELVERBINDINGSKAART",
        "Grafisch voorbeeld van de huidige A -> B verbindingen",
        "ZIJDE A", "ZIJDE B",
        "Wit: 1:1", "Groen: aangepast", "Geel: gemeenschappelijk / meervoudig",
        "TERUG", "%u pins%s%s"};
    static const GraphText de{
        "KABEL-VERBINDUNGSPLAN",
        "Grafische Vorschau der aktuellen A -> B Verbindungen",
        "SEITE A", "SEITE B",
        "Weiß: 1:1", "Grün: Sonderbelegung", "Gelb: Sammel-/Mehrfachnetz",
        "ZURÜCK", "%u Pins%s%s"};
    static const GraphText fr{
        "CARTE DES CONNEXIONS",
        "Aperçu graphique des connexions A -> B de l'éditeur",
        "CÔTÉ A", "CÔTÉ B",
        "Blanc : 1:1", "Vert : spécial", "Jaune : réseau commun / multiple",
        "RETOUR", "%u broches%s%s"};
    static const GraphText es{
        "MAPA DE CONEXIONES",
        "Vista gráfica de las conexiones A -> B actuales",
        "LADO A", "LADO B",
        "Blanco: 1:1", "Verde: especial", "Amarillo: red común / múltiple",
        "VOLVER", "%u pines%s%s"};
    static const GraphText pl{
        "MAPA POŁĄCZEŃ KABLA",
        "Graficzny podgląd bieżących połączeń A -> B",
        "STRONA A", "STRONA B",
        "Biały: 1:1", "Zielony: niestandardowe", "Żółty: sieć wspólna / wielokrotna",
        "WSTECZ", "%u pinów%s%s"};

    switch (currentP4Language()) {
        case P4Language::English: return en;
        case P4Language::Dutch: return nl;
        case P4Language::German: return de;
        case P4Language::French: return fr;
        case P4Language::Spanish: return es;
        case P4Language::Polish: return pl;
        case P4Language::Turkish:
        default: return tr;
    }
}

void flat(lv_obj_t* object) {
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

lv_obj_t* makeLabel(lv_obj_t* parent,
                    const char* value,
                    const lv_font_t* font,
                    lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, value);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}


}  // namespace

void MapGraphScreen::begin(const CableMap& map,
                           const CableProfile& profile,
                           bool returnToCompletionReport) {
    map_ = map;
    profile_ = profile;
    resultSession_ = nullptr;
    resultMode_ = false;
    returnToCompletionReport_ = returnToCompletionReport;
    resultView_ = ResultGraphView::Combined;
    resultFilter_ = ResultGraphFilter::All;
    filterVisible_ = false;
    backRequested_ = false;
    build();
    refresh();
    presentPreparedScreen();
}

void MapGraphScreen::beginResults(const ScanSession& session) {
    resultSession_ = &session;
    profile_ = session.profile();
    map_ = session.usesCustomMap() && session.customMap() != nullptr
               ? *session.customMap()
               : makeOneToOneCableMap();
    resultMode_ = true;
    returnToCompletionReport_ = true;
    resultView_ = ResultGraphView::Combined;
    resultFilter_ = ResultGraphFilter::All;
    // User rule: the filter bar exists ONLY above 30 visible test points.
    filterVisible_ = profile_.activePointCount() > 30U;
    backRequested_ = false;
    postPresentRefreshPasses_ = 0U;
    build();
    refresh();
    presentPreparedScreen();
}

void MapGraphScreen::activate() {
    backRequested_ = false;
    refresh();
    presentPreparedScreen();
}

void MapGraphScreen::servicePostPresentRefresh() {
    if (postPresentRefreshPasses_ == 0U || screen_ == nullptr ||
        lv_scr_act() != screen_) {
        return;
    }

    // JC1060/P4 uses LVGL full_refresh with two native MIPI-DPI framebuffers.
    // A newly loaded graph screen can otherwise leave the pin-label layer only
    // partially represented until a later button interaction causes more full
    // frame swaps. This affects both editor/profile maps and result graphs.
    // Two post-present passes populate both native buffers deterministically.
    --postPresentRefreshPasses_;
    refresh();
    lv_obj_update_layout(screen_);
    for (uint8_t i = 0U; i < profile_.activePointCount(); ++i) {
        if (leftLabels_[i] != nullptr) lv_obj_invalidate(leftLabels_[i]);
        if (rightLabels_[i] != nullptr) lv_obj_invalidate(rightLabels_[i]);
    }
    lv_obj_invalidate(screen_);
}

lv_obj_t* MapGraphScreen::makeResultButton(lv_obj_t* parent,
                                           lv_coord_t x,
                                           lv_coord_t y,
                                           lv_coord_t w,
                                           lv_coord_t h,
                                           const char* text,
                                           ResultControl control,
                                           uint8_t bindingIndex) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, h);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x33444F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(0x607784), LV_PART_MAIN);
    lv_obj_set_style_radius(button, 7, LV_PART_MAIN);
    resultBindings_[bindingIndex] = {this, control};
    lv_obj_add_event_cb(button,
                        resultControlCallback,
                        LV_EVENT_CLICKED,
                        &resultBindings_[bindingIndex]);
    lv_obj_t* label = makeLabel(button, text, p4Font10(), lv_color_hex(0xFFFFFF));
    lv_obj_set_width(label, w - 8);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(label);
    return button;
}

void MapGraphScreen::buildEditorFooter(lv_obj_t* footer) {
    const GraphText& t = graphText();
    lv_obj_t* l1 = makeLabel(footer, t.oneToOne, p4Font10(), lv_color_hex(0xF0F4F6));
    lv_obj_set_pos(l1, 18, 12);
    lv_obj_t* l2 = makeLabel(footer, t.custom, p4Font10(), lv_color_hex(0x6FE59B));
    lv_obj_set_pos(l2, 210, 12);
    lv_obj_t* l3 = makeLabel(footer, t.multi, p4Font10(), lv_color_hex(0xFFD05A));
    lv_obj_set_pos(l3, 442, 12);

    lv_obj_t* back = lv_btn_create(footer);
    lv_obj_set_pos(back, 828, 10);
    lv_obj_set_size(back, 170, 68);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x5A496F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(back, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(back, lv_color_hex(0x8B75A7), LV_PART_MAIN);
    lv_obj_set_style_radius(back, 8, LV_PART_MAIN);
    lv_obj_t* backText = makeLabel(back, t.back, p4Font14(), lv_color_hex(0xFFFFFF));
    lv_obj_center(backText);
    lv_obj_add_event_cb(back, backCallback, LV_EVENT_CLICKED, this);
}

void MapGraphScreen::buildResultFooter(lv_obj_t* footer) {
    const char* combined = p4SelectText("BİRLİKTE", "COMBINED", "SAMEN", "ZUSAMMEN",
                                        "ENSEMBLE", "JUNTOS", "RAZEM");
    const char* expected = p4SelectText("BEKLENEN", "EXPECTED", "VERWACHT", "ERWARTET",
                                        "ATTENDU", "ESPERADO", "OCZEKIWANE");
    const char* measured = p4SelectText("ÖLÇÜLEN", "MEASURED", "GEMETEN", "GEMESSEN",
                                        "MESURÉ", "MEDIDO", "ZMIERZONE");
    viewButtons_[0] = makeResultButton(footer, 10, 8, 108, 34, combined,
                                       ResultControl::ViewCombined, 0U);
    viewButtons_[1] = makeResultButton(footer, 124, 8, 108, 34, expected,
                                       ResultControl::ViewExpected, 1U);
    viewButtons_[2] = makeResultButton(footer, 238, 8, 108, 34, measured,
                                       ResultControl::ViewMeasured, 2U);

    // Compact result legend: this is the same color contract as ScanScreen and
    // MgResultTable, not a second status palette.
    lv_obj_t* legend = makeLabel(
        footer,
        p4SelectText("YEŞİL OK  BEYAZ OPEN  SARI SHORT  KIRMIZI WRONG  TURUNCU HIGH-R",
                     "GREEN OK  WHITE OPEN  YELLOW SHORT  RED WRONG  ORANGE HIGH-R",
                     "GROEN OK  WIT OPEN  GEEL SHORT  ROOD WRONG  ORANJE HIGH-R",
                     "GRÜN OK  WEISS OPEN  GELB SHORT  ROT WRONG  ORANGE HIGH-R",
                     "VERT OK  BLANC OPEN  JAUNE SHORT  ROUGE WRONG  ORANGE HIGH-R",
                     "VERDE OK  BLANCO OPEN  AMARILLO SHORT  ROJO WRONG  NARANJA HIGH-R",
                     "ZIELONY OK  BIAŁY OPEN  ŻÓŁTY SHORT  CZERWONY WRONG  POMARAŃCZOWY HIGH-R"),
        p4Font10(),
        lv_color_hex(0xC8D6DE));
    lv_obj_set_pos(legend, 365, 17);

    lv_obj_t* back = lv_btn_create(footer);
    lv_obj_set_pos(back, 866, 7);
    lv_obj_set_size(back, 146, 38);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x5A496F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(back, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(back, lv_color_hex(0x8B75A7), LV_PART_MAIN);
    lv_obj_set_style_radius(back, 8, LV_PART_MAIN);
    lv_obj_t* backText = makeLabel(back, graphText().back, p4Font14(), lv_color_hex(0xFFFFFF));
    lv_obj_center(backText);
    lv_obj_add_event_cb(back, backCallback, LV_EVENT_CLICKED, this);

    if (!filterVisible_) {
        return;
    }

    static constexpr lv_coord_t kFilterWidth = 136;
    static constexpr lv_coord_t kGap = 7;
    const char* filterText[7] = {
        p4SelectText("TÜMÜ", "ALL", "ALLES", "ALLE", "TOUT", "TODO", "WSZYSTKO"),
        p4SelectText("HATALAR", "ERRORS", "FOUTEN", "FEHLER", "ERREURS", "ERRORES", "BŁĘDY"),
        p4SelectText("DOĞRU", "OK", "GOED", "OK", "OK", "OK", "OK"),
        "OPEN", "SHORT", "WRONG", "HIGH-R"
    };
    const ResultControl controls[7] = {
        ResultControl::FilterAll,
        ResultControl::FilterErrors,
        ResultControl::FilterOk,
        ResultControl::FilterOpen,
        ResultControl::FilterShort,
        ResultControl::FilterWrong,
        ResultControl::FilterHighR,
    };
    for (uint8_t i = 0U; i < 7U; ++i) {
        const lv_coord_t x = static_cast<lv_coord_t>(10 + i * (kFilterWidth + kGap));
        filterButtons_[i] = makeResultButton(footer, x, 55, kFilterWidth, 36,
                                             filterText[i], controls[i],
                                             static_cast<uint8_t>(3U + i));
    }
}

void MapGraphScreen::build() {
    if (screen_ == nullptr) {
        screen_ = lv_obj_create(nullptr);
    } else {
        lv_obj_clean(screen_);
    }
    // Do not load the screen while its child labels are still empty.
    // The first result-graph frame must be prepared completely
    // before LVGL can render it; otherwise only the earliest pin label may
    // appear until a view-button forces a second refresh.
    flat(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x071018), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

    for (auto& button : viewButtons_) button = nullptr;
    for (auto& button : filterButtons_) button = nullptr;

    const GraphText& t = graphText();
    lv_obj_t* header = lv_obj_create(screen_);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, kDisplayWidth, 70);
    flat(header);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x142B3A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);

    const char* titleText = resultMode_
        ? p4SelectText("TARAMA SONUÇ GRAFİĞİ", "SCAN RESULT GRAPH", "SCANRESULTAAT GRAFIEK",
                       "SCAN-ERGEBNISGRAFIK", "GRAPHE DU RÉSULTAT", "GRÁFICO DE RESULTADO",
                       "WYKRES WYNIKU SKANU")
        : t.title;
    const char* subtitleText = resultMode_
        ? p4SelectText("Beklenen bağlantı ile gerçek tarama sonucunu karşılaştırır",
                       "Compares expected wiring with the measured scan result",
                       "Vergelijkt verwachte bedrading met het gemeten scanresultaat",
                       "Vergleicht Sollbelegung mit dem gemessenen Scan-Ergebnis",
                       "Compare le câblage attendu au résultat mesuré",
                       "Compara el cableado esperado con el resultado medido",
                       "Porównuje oczekiwane połączenia z wynikiem pomiaru")
        : t.subtitle;

    lv_obj_t* title = makeLabel(header, titleText, p4Font20(), lv_color_hex(0xEAF6FF));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 7);
    subtitleLabel_ = makeLabel(header, subtitleText, p4Font10(), lv_color_hex(0x9CC6DC));
    lv_obj_align(subtitleLabel_, LV_ALIGN_BOTTOM_MID, 0, -8);
    soundButton_.create(header, 970, 14, 42);

    lv_obj_t* leftTitle = makeLabel(screen_, t.aSide, p4Font14(), lv_color_hex(0x6EDBFF));
    lv_obj_set_pos(leftTitle, 12, 74);
    lv_obj_set_width(leftTitle, 100);
    lv_obj_set_style_text_align(leftTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    lv_obj_t* rightTitle = makeLabel(screen_, t.bSide, p4Font14(), lv_color_hex(0x8EF0A7));
    lv_obj_set_pos(rightTitle, 912, 74);
    lv_obj_set_width(rightTitle, 100);
    lv_obj_set_style_text_align(rightTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    countLabel_ = makeLabel(screen_, "", p4Font10(), lv_color_hex(0xA7C4D3));
    lv_obj_set_pos(countLabel_, 390, 76);
    lv_obj_set_width(countLabel_, 244);
    lv_obj_set_style_text_align(countLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    graphY_ = 94;
    graphHeight_ = resultMode_ && filterVisible_ ? 378 : 404;
    graphObject_ = lv_obj_create(screen_);
    lv_obj_set_pos(graphObject_, 62, graphY_);
    lv_obj_set_size(graphObject_, 900, graphHeight_);
    flat(graphObject_);
    lv_obj_set_style_bg_opa(graphObject_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(graphObject_, 0, LV_PART_MAIN);
    lv_obj_clear_flag(graphObject_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(graphObject_, drawCallback, LV_EVENT_DRAW_MAIN, this);

    for (uint8_t i = 0; i < kTestPointsPerSide; ++i) {
        leftLabels_[i] = makeLabel(screen_, "", p4Font10(), lv_color_hex(0xDCECF4));
        lv_obj_set_width(leftLabels_[i], 48);
        lv_obj_set_height(leftLabels_[i], 12);
        lv_obj_set_style_text_align(leftLabels_[i], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

        rightLabels_[i] = makeLabel(screen_, "", p4Font10(), lv_color_hex(0xDCECF4));
        lv_obj_set_width(rightLabels_[i], 48);
        lv_obj_set_height(rightLabels_[i], 12);
        lv_obj_set_style_text_align(rightLabels_[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    }

    const lv_coord_t footerY = resultMode_ && filterVisible_ ? 478 : 506;
    const lv_coord_t footerH = static_cast<lv_coord_t>(600 - footerY);
    lv_obj_t* footer = lv_obj_create(screen_);
    lv_obj_set_pos(footer, 0, footerY);
    lv_obj_set_size(footer, kDisplayWidth, footerH);
    flat(footer);
    lv_obj_set_style_bg_color(footer, lv_color_hex(0x0D1B24), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(footer, 0, LV_PART_MAIN);

    if (resultMode_) buildResultFooter(footer);
    else buildEditorFooter(footer);

    refreshResultControls();
}

void MapGraphScreen::formatPin(uint8_t testIndex,
                               char* output,
                               size_t outputSize) const {
    if (output == nullptr || outputSize == 0U) {
        return;
    }
    if (testIndex == kPeTestIndex) {
        snprintf(output, outputSize, "PE");
    } else if (testIndex == kDrainShieldTestIndex) {
        snprintf(output, outputSize, "dS");
    } else {
        snprintf(output, outputSize, "%u", testIndex);
    }
}

uint8_t MapGraphScreen::visibleTestIndex(uint8_t visibleSlot) const {
    return profile_.slotToTestIndex(visibleSlot);
}

int16_t MapGraphScreen::pointY(uint8_t visibleSlot,
                               uint8_t visibleCount,
                               lv_coord_t height) const {
    if (visibleCount <= 1U) {
        return static_cast<int16_t>(height / 2);
    }
    const int16_t margin = visibleCount <= 25U ? 12 : 4;
    const int16_t usable = static_cast<int16_t>(height - 2 * margin);
    return static_cast<int16_t>(margin +
        (static_cast<int32_t>(visibleSlot) * usable) / (visibleCount - 1U));
}

uint64_t MapGraphScreen::visibleMask() const {
    uint64_t mask = 0ULL;
    const uint8_t count = profile_.activePointCount();
    for (uint8_t slot = 0; slot < count; ++slot) {
        const uint8_t testIndex = visibleTestIndex(slot);
        if (testIndex < kTestPointsPerSide) {
            mask |= (1ULL << testIndex);
        }
    }
    return mask;
}

void MapGraphScreen::refresh() {
    if (screen_ == nullptr || graphObject_ == nullptr) {
        return;
    }

    const uint8_t count = profile_.activePointCount();
    const lv_coord_t graphH = lv_obj_get_height(graphObject_);
    char countText[160]{};
    if (resultMode_) {
        snprintf(countText, sizeof(countText),
                 "%u PIN | %s",
                 static_cast<unsigned>(profile_.signalPinCount),
                 resultView_ == ResultGraphView::Combined
                     ? p4SelectText("BEKLENEN + ÖLÇÜLEN", "EXPECTED + MEASURED", "VERWACHT + GEMETEN",
                                    "SOLL + GEMESSEN", "ATTENDU + MESURÉ", "ESPERADO + MEDIDO", "OCZEKIWANE + ZMIERZONE")
                     : (resultView_ == ResultGraphView::Expected
                            ? p4SelectText("BEKLENEN", "EXPECTED", "VERWACHT", "ERWARTET", "ATTENDU", "ESPERADO", "OCZEKIWANE")
                            : p4SelectText("ÖLÇÜLEN", "MEASURED", "GEMETEN", "GEMESSEN", "MESURÉ", "MEDIDO", "ZMIERZONE")));
    } else {
        snprintf(countText, sizeof(countText),
                 "%u PIN | A: PE %s dS %s | B: PE %s dS %s",
                 static_cast<unsigned>(profile_.signalPinCount),
                 profile_.includePeA ? "ON" : "OFF",
                 profile_.includeDrainShieldA ? "ON" : "OFF",
                 profile_.includePeB ? "ON" : "OFF",
                 profile_.includeDrainShieldB ? "ON" : "OFF");
    }
    lv_label_set_text(countLabel_, countText);

    for (uint8_t i = 0; i < kTestPointsPerSide; ++i) {
        if (i >= count) {
            lv_obj_add_flag(leftLabels_[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(rightLabels_[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const uint8_t testIndex = visibleTestIndex(i);
        const bool leftEnabled = profile_.pointEnabledOnSide(true, testIndex);
        const bool rightEnabled = profile_.pointEnabledOnSide(false, testIndex);
        if (leftEnabled) lv_obj_clear_flag(leftLabels_[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(leftLabels_[i], LV_OBJ_FLAG_HIDDEN);
        if (rightEnabled) lv_obj_clear_flag(rightLabels_[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(rightLabels_[i], LV_OBJ_FLAG_HIDDEN);

        char pin[8]{};
        formatPin(testIndex, pin, sizeof(pin));
        lv_label_set_text(leftLabels_[i], pin);
        lv_label_set_text(rightLabels_[i], pin);

        const int16_t y = pointY(i, count, graphH);
        lv_obj_set_pos(leftLabels_[i], 8, graphY_ + y - 6);
        lv_obj_set_pos(rightLabels_[i], 968, graphY_ + y - 6);

        if (resultMode_ && resultSession_ != nullptr) {
            const ElectricalResult aResult = worstResultForPoint(true, testIndex);
            const ElectricalResult bResult = worstResultForPoint(false, testIndex);
            const bool aVisibleByFilter = resultMatchesFilter(aResult);
            const bool bVisibleByFilter = resultMatchesFilter(bResult);
            lv_obj_set_style_text_color(leftLabels_[i],
                aVisibleByFilter && aResult != ElectricalResult::NotMeasured
                    ? resultColor(aResult) : lv_color_hex(0x62737E), LV_PART_MAIN);
            lv_obj_set_style_text_color(rightLabels_[i],
                bVisibleByFilter && bResult != ElectricalResult::NotMeasured
                    ? resultColor(bResult) : lv_color_hex(0x62737E), LV_PART_MAIN);
        } else {
            lv_obj_set_style_text_color(leftLabels_[i], lv_color_hex(0xDCECF4), LV_PART_MAIN);
            lv_obj_set_style_text_color(rightLabels_[i], lv_color_hex(0xDCECF4), LV_PART_MAIN);
        }
    }

    refreshResultControls();
    lv_obj_invalidate(graphObject_);
}


void MapGraphScreen::presentPreparedScreen() {
    if (screen_ == nullptr) {
        return;
    }

    // Resolve all explicit label positions/text sizes before this screen becomes
    // active.  This makes the initial Combined result view deterministic rather
    // than depending on a later EXPECTED/MEASURED button refresh.
    lv_obj_update_layout(screen_);
    if (lv_scr_act() != screen_) {
        lv_scr_load(screen_);
    }
    // Force a full first-frame redraw after the completed object tree is active.
    lv_obj_invalidate(screen_);

    // On the P4 double-buffer/full-refresh port, one frame is not
    // enough to guarantee that both native buffers contain the complete label
    // layer. Service two more full frames from the main loop for BOTH editor/profile
    // maps and result graphs, after this first presentation has actually flushed.
    postPresentRefreshPasses_ = 2U;
}

void MapGraphScreen::refreshResultControls() {
    if (!resultMode_) {
        return;
    }
    const ResultGraphView views[3] = {
        ResultGraphView::Combined,
        ResultGraphView::Expected,
        ResultGraphView::Measured,
    };
    for (uint8_t i = 0U; i < 3U; ++i) {
        if (viewButtons_[i] == nullptr) continue;
        const bool active = resultView_ == views[i];
        lv_obj_set_style_bg_color(viewButtons_[i],
                                  active ? lv_color_hex(0x147A9D) : lv_color_hex(0x33444F),
                                  LV_PART_MAIN);
        lv_obj_set_style_border_color(viewButtons_[i],
                                      active ? lv_color_hex(0x75D8F5) : lv_color_hex(0x607784),
                                      LV_PART_MAIN);
    }
    if (!filterVisible_) return;
    const ResultGraphFilter filters[7] = {
        ResultGraphFilter::All,
        ResultGraphFilter::Errors,
        ResultGraphFilter::Ok,
        ResultGraphFilter::Open,
        ResultGraphFilter::ShortCircuit,
        ResultGraphFilter::WrongConnection,
        ResultGraphFilter::HighResistance,
    };
    for (uint8_t i = 0U; i < 7U; ++i) {
        if (filterButtons_[i] == nullptr) continue;
        const bool active = resultFilter_ == filters[i];
        lv_obj_set_style_bg_color(filterButtons_[i],
                                  active ? lv_color_hex(0x147A9D) : lv_color_hex(0x33444F),
                                  LV_PART_MAIN);
        lv_obj_set_style_border_color(filterButtons_[i],
                                      active ? lv_color_hex(0x75D8F5) : lv_color_hex(0x607784),
                                      LV_PART_MAIN);
    }
}

uint8_t MapGraphScreen::resultSeverity(ElectricalResult result) {
    switch (result) {
        case ElectricalResult::WrongConnection: return 5U;
        case ElectricalResult::ShortCircuit:    return 4U;
        case ElectricalResult::HighResistance:  return 3U;
        case ElectricalResult::Open:            return 2U;
        case ElectricalResult::Ok:              return 1U;
        case ElectricalResult::NotMeasured:
        default:                                return 0U;
    }
}

lv_color_t MapGraphScreen::resultColor(ElectricalResult result) {
    switch (result) {
        case ElectricalResult::Ok:              return lv_color_hex(0x35D273);
        case ElectricalResult::Open:            return lv_color_hex(0xE9EEF2);
        case ElectricalResult::ShortCircuit:    return lv_color_hex(0xFFD34D);
        case ElectricalResult::WrongConnection: return lv_color_hex(0xF04444);
        case ElectricalResult::HighResistance:  return lv_color_hex(0xFF922E);
        case ElectricalResult::NotMeasured:
        default:                                return lv_color_hex(0x566873);
    }
}

bool MapGraphScreen::resultMatchesFilter(ElectricalResult result) const {
    if (!resultMode_) return true;
    // Under the 30-pin threshold the filter UI is intentionally absent and
    // the graph always shows the complete result set.
    if (!filterVisible_) return result != ElectricalResult::NotMeasured;
    switch (resultFilter_) {
        case ResultGraphFilter::All:
            return result != ElectricalResult::NotMeasured;
        case ResultGraphFilter::Errors:
            return result != ElectricalResult::NotMeasured && result != ElectricalResult::Ok;
        case ResultGraphFilter::Ok:              return result == ElectricalResult::Ok;
        case ResultGraphFilter::Open:            return result == ElectricalResult::Open;
        case ResultGraphFilter::ShortCircuit:    return result == ElectricalResult::ShortCircuit;
        case ResultGraphFilter::WrongConnection:return result == ElectricalResult::WrongConnection;
        case ResultGraphFilter::HighResistance:  return result == ElectricalResult::HighResistance;
        default:                                 return true;
    }
}

ElectricalResult MapGraphScreen::worstResultForPoint(bool sideA,
                                                      uint8_t testIndex) const {
    if (resultSession_ == nullptr || testIndex >= kTestPointsPerSide) {
        return ElectricalResult::NotMeasured;
    }
    ElectricalResult worst = ElectricalResult::NotMeasured;
    auto consider = [&](ElectricalResult candidate) {
        if (resultSeverity(candidate) > resultSeverity(worst)) worst = candidate;
    };
    const uint64_t bit = 1ULL << testIndex;
    const uint8_t count = resultSession_->profile().activePointCount();
    for (uint8_t slot = 0U; slot < count; ++slot) {
        const uint8_t sourceIndex = resultSession_->profile().slotToTestIndex(slot);
        const Measurement ab = resultSession_->reportMeasurement(ScanDirection::AtoB, slot);
        if (ab.result != ElectricalResult::NotMeasured) {
            if ((sideA && ((ab.senderGroupMask & bit) != 0ULL || sourceIndex == testIndex)) ||
                (!sideA && (ab.actualReceiverMask & bit) != 0ULL)) {
                consider(ab.result);
            }
        }
        const Measurement ba = resultSession_->reportMeasurement(ScanDirection::BtoA, slot);
        if (ba.result != ElectricalResult::NotMeasured) {
            if ((!sideA && ((ba.senderGroupMask & bit) != 0ULL || sourceIndex == testIndex)) ||
                (sideA && (ba.actualReceiverMask & bit) != 0ULL)) {
                consider(ba.result);
            }
        }
    }
    return worst;
}

void MapGraphScreen::drawGraph(lv_draw_ctx_t* drawCtx, lv_obj_t* graphObject) {
    if (drawCtx == nullptr || graphObject == nullptr) {
        return;
    }
    if (resultMode_) {
        drawResultGraph(drawCtx, graphObject);
        return;
    }

    lv_area_t area{};
    lv_obj_get_coords(graphObject, &area);
    const lv_coord_t width = lv_obj_get_width(graphObject);
    const lv_coord_t height = lv_obj_get_height(graphObject);
    const uint8_t count = profile_.activePointCount();
    const uint64_t vMask = visibleMask();
    const uint64_t vMaskA = vMask & profile_.sidePointMask(true);
    const uint64_t vMaskB = vMask & profile_.sidePointMask(false);

    constexpr int16_t kLeftNodeX = 10;
    const int16_t rightNodeX = static_cast<int16_t>(width - 11);
    // Branches are deliberately kept close to the connector sides. The large
    // centre region therefore contains only ONE trunk per electrical net.
    const int16_t branchInset = count <= 25U ? 78 : (count <= 40U ? 62 : 48);
    const int16_t leftJoinX = static_cast<int16_t>(kLeftNodeX + branchInset);
    const int16_t rightJoinX = static_cast<int16_t>(rightNodeX - branchInset);

    // Subtle row guides keep dense maps readable without changing the dark UI.
    lv_draw_line_dsc_t guide{};
    lv_draw_line_dsc_init(&guide);
    guide.width = 1;
    guide.color = lv_color_hex(0x16303D);
    guide.opa = LV_OPA_70;

    for (uint8_t slot = 0; slot < count; ++slot) {
        const int16_t y = pointY(slot, count, height);
        lv_point_t p1{static_cast<lv_coord_t>(area.x1 + kLeftNodeX),
                      static_cast<lv_coord_t>(area.y1 + y)};
        lv_point_t p2{static_cast<lv_coord_t>(area.x1 + rightNodeX),
                      static_cast<lv_coord_t>(area.y1 + y)};
        lv_draw_line(drawCtx, &guide, &p1, &p2);
    }

    auto findSlot = [&](uint8_t testIndex) -> int16_t {
        for (uint8_t slot = 0; slot < count; ++slot) {
            if (visibleTestIndex(slot) == testIndex) {
                return static_cast<int16_t>(slot);
            }
        }
        return -1;
    };

    auto drawSegment = [&](const lv_draw_line_dsc_t& dsc,
                           int16_t x1, int16_t y1,
                           int16_t x2, int16_t y2) {
        lv_point_t p1{static_cast<lv_coord_t>(area.x1 + x1),
                      static_cast<lv_coord_t>(area.y1 + y1)};
        lv_point_t p2{static_cast<lv_coord_t>(area.x1 + x2),
                      static_cast<lv_coord_t>(area.y1 + y2)};
        lv_draw_line(drawCtx, &dsc, &p1, &p2);
    };

    auto drawJunction = [&](int16_t x, int16_t y, lv_color_t color, int16_t radius) {
        lv_draw_rect_dsc_t dot{};
        lv_draw_rect_dsc_init(&dot);
        dot.bg_color = color;
        dot.bg_opa = LV_OPA_COVER;
        dot.radius = LV_RADIUS_CIRCLE;
        dot.border_width = 0;
        lv_area_t r{
            static_cast<lv_coord_t>(area.x1 + x - radius),
            static_cast<lv_coord_t>(area.y1 + y - radius),
            static_cast<lv_coord_t>(area.x1 + x + radius),
            static_cast<lv_coord_t>(area.y1 + y + radius)};
        lv_draw_rect(drawCtx, &dot, &r);
    };

    CableNet nets[kTestPointsPerSide]{};
    const size_t netCount = CableNetGraph::build(
        map_, vMaskA, vMaskB, nets, kTestPointsPerSide);

    for (size_t netIndex = 0; netIndex < netCount; ++netIndex) {
        const CableNet& net = nets[netIndex];
        const uint8_t aCount = CableNetGraph::popcount(net.aMask);
        const uint8_t bCount = CableNetGraph::popcount(net.bMask);
        if (aCount == 0U && bCount == 0U) {
            continue;
        }

        uint8_t aSlots[kTestPointsPerSide]{};
        uint8_t bSlots[kTestPointsPerSide]{};
        uint8_t aSlotCount = 0U;
        uint8_t bSlotCount = 0U;
        uint8_t onlyAIndex = 0xFFU;
        uint8_t onlyBIndex = 0xFFU;

        for (uint8_t index = 0; index < kTestPointsPerSide; ++index) {
            if ((net.aMask & (1ULL << index)) != 0ULL) {
                const int16_t slot = findSlot(index);
                if (slot >= 0) {
                    aSlots[aSlotCount++] = static_cast<uint8_t>(slot);
                    onlyAIndex = index;
                }
            }
            if ((net.bMask & (1ULL << index)) != 0ULL) {
                const int16_t slot = findSlot(index);
                if (slot >= 0) {
                    bSlots[bSlotCount++] = static_cast<uint8_t>(slot);
                    onlyBIndex = index;
                }
            }
        }

        const bool multiNet = aCount > 1U || bCount > 1U;
        const bool oneToOne = aCount == 1U && bCount == 1U &&
                              onlyAIndex == onlyBIndex;

        lv_draw_line_dsc_t line{};
        lv_draw_line_dsc_init(&line);
        line.width = count <= 25U ? 2 : 1;
        line.opa = LV_OPA_COVER;
        line.color = multiNet
                         ? lv_color_hex(0xFFD05A)
                         : (oneToOne ? lv_color_hex(0xF0F4F6)
                                     : lv_color_hex(0x59D98A));

        // Select the A/B root pair with the smallest vertical distance. This
        // tends to keep the one central trunk horizontal and lets the branch
        // bus fan out next to the connector, as in a hand-drawn wiring sketch.
        int16_t rootAY = 0;
        int16_t rootBY = 0;
        bool haveA = aSlotCount > 0U;
        bool haveB = bSlotCount > 0U;
        if (haveA) {
            rootAY = pointY(aSlots[0], count, height);
        }
        if (haveB) {
            rootBY = pointY(bSlots[0], count, height);
        }
        if (haveA && haveB) {
            int32_t bestDistance = 0x7FFFFFFF;
            for (uint8_t ai = 0; ai < aSlotCount; ++ai) {
                const int16_t ay = pointY(aSlots[ai], count, height);
                for (uint8_t bi = 0; bi < bSlotCount; ++bi) {
                    const int16_t by = pointY(bSlots[bi], count, height);
                    const int32_t distance = ay > by ? ay - by : by - ay;
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        rootAY = ay;
                        rootBY = by;
                    }
                }
            }
        }

        // Collapse all A pins belonging to this net onto a short vertical bus
        // close to the A connector. A single-A net has no unnecessary bus.
        if (aSlotCount > 1U) {
            int16_t minY = pointY(aSlots[0], count, height);
            int16_t maxY = minY;
            for (uint8_t i = 0; i < aSlotCount; ++i) {
                const int16_t y = pointY(aSlots[i], count, height);
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
                drawSegment(line, kLeftNodeX, y, leftJoinX, y);
                drawJunction(leftJoinX, y, line.color, count <= 25U ? 2 : 1);
            }
            drawSegment(line, leftJoinX, minY, leftJoinX, maxY);
        }

        // Same operation at B. This is the key visual change for e.g.
        // A2 -> B2+B3+B4: one trunk reaches B, then a local B-side bus fans
        // out to B2/B3/B4 instead of three full-width lines leaving A2.
        if (bSlotCount > 1U) {
            int16_t minY = pointY(bSlots[0], count, height);
            int16_t maxY = minY;
            for (uint8_t i = 0; i < bSlotCount; ++i) {
                const int16_t y = pointY(bSlots[i], count, height);
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
                drawSegment(line, rightJoinX, y, rightNodeX, y);
                drawJunction(rightJoinX, y, line.color, count <= 25U ? 2 : 1);
            }
            drawSegment(line, rightJoinX, minY, rightJoinX, maxY);
        }

        // Exactly one central trunk per electrical net. For a single endpoint
        // the trunk goes directly to the pin. For a multi endpoint it joins the
        // local branch bus. Single-side-only nets intentionally have no centre
        // trunk because they are confined to one connector side.
        if (haveA && haveB) {
            const int16_t trunkX1 = aSlotCount > 1U ? leftJoinX : kLeftNodeX;
            const int16_t trunkX2 = bSlotCount > 1U ? rightJoinX : rightNodeX;
            drawSegment(line, trunkX1, rootAY, trunkX2, rootBY);
            if (aSlotCount > 1U) {
                drawJunction(leftJoinX, rootAY, line.color, count <= 25U ? 2 : 1);
            }
            if (bSlotCount > 1U) {
                drawJunction(rightJoinX, rootBY, line.color, count <= 25U ? 2 : 1);
            }
        }
    }

    // Physical endpoint dots are always drawn last. Crossing trunks therefore
    // never look like electrical junctions; only the small branch-bus dots do.
    lv_draw_rect_dsc_t node{};
    lv_draw_rect_dsc_init(&node);
    node.bg_color = lv_color_hex(0xA8DFF3);
    node.bg_opa = LV_OPA_COVER;
    node.radius = LV_RADIUS_CIRCLE;
    node.border_width = 0;
    const int16_t radius = count <= 25U ? 3 : 2;
    for (uint8_t slot = 0; slot < count; ++slot) {
        const int16_t y = pointY(slot, count, height);
        lv_area_t left{
            static_cast<lv_coord_t>(area.x1 + kLeftNodeX - radius),
            static_cast<lv_coord_t>(area.y1 + y - radius),
            static_cast<lv_coord_t>(area.x1 + kLeftNodeX + radius),
            static_cast<lv_coord_t>(area.y1 + y + radius)};
        lv_area_t right{
            static_cast<lv_coord_t>(area.x1 + rightNodeX - radius),
            static_cast<lv_coord_t>(area.y1 + y - radius),
            static_cast<lv_coord_t>(area.x1 + rightNodeX + radius),
            static_cast<lv_coord_t>(area.y1 + y + radius)};
        const uint8_t testIndex = visibleTestIndex(slot);
        if ((vMaskA & (1ULL << testIndex)) != 0ULL) {
            lv_draw_rect(drawCtx, &node, &left);
        }
        if ((vMaskB & (1ULL << testIndex)) != 0ULL) {
            lv_draw_rect(drawCtx, &node, &right);
        }
    }
}

void MapGraphScreen::drawResultGraph(lv_draw_ctx_t* drawCtx,
                                     lv_obj_t* graphObject) {
    if (resultSession_ == nullptr) return;

    lv_area_t area{};
    lv_obj_get_coords(graphObject, &area);
    const lv_coord_t width = lv_obj_get_width(graphObject);
    const lv_coord_t height = lv_obj_get_height(graphObject);
    const uint8_t count = profile_.activePointCount();
    constexpr int16_t kLeftNodeX = 10;
    const int16_t rightNodeX = static_cast<int16_t>(width - 11);
    const int16_t branchInset = count <= 25U ? 78 : (count <= 40U ? 62 : 48);
    const int16_t leftJoinX = static_cast<int16_t>(kLeftNodeX + branchInset);
    const int16_t rightJoinX = static_cast<int16_t>(rightNodeX - branchInset);

    auto findSlot = [&](uint8_t testIndex) -> int16_t {
        for (uint8_t slot = 0U; slot < count; ++slot) {
            if (visibleTestIndex(slot) == testIndex) return static_cast<int16_t>(slot);
        }
        return -1;
    };
    auto lowestBit = [](uint64_t mask) -> uint8_t {
        for (uint8_t index = 0U; index < kTestPointsPerSide; ++index) {
            if ((mask & (1ULL << index)) != 0ULL) return index;
        }
        return 0xFFU;
    };
    auto popcount = [](uint64_t mask) -> uint8_t {
#if defined(__GNUC__)
        return static_cast<uint8_t>(__builtin_popcountll(mask));
#else
        uint8_t c = 0U;
        while (mask != 0ULL) { mask &= mask - 1ULL; ++c; }
        return c;
#endif
    };
    auto drawSegment = [&](const lv_draw_line_dsc_t& dsc,
                           int16_t x1, int16_t y1,
                           int16_t x2, int16_t y2) {
        lv_point_t p1{static_cast<lv_coord_t>(area.x1 + x1),
                      static_cast<lv_coord_t>(area.y1 + y1)};
        lv_point_t p2{static_cast<lv_coord_t>(area.x1 + x2),
                      static_cast<lv_coord_t>(area.y1 + y2)};
        lv_draw_line(drawCtx, &dsc, &p1, &p2);
    };
    auto drawJunction = [&](int16_t x, int16_t y, lv_color_t color, int16_t radius) {
        lv_draw_rect_dsc_t dot{};
        lv_draw_rect_dsc_init(&dot);
        dot.bg_color = color;
        dot.bg_opa = LV_OPA_COVER;
        dot.radius = LV_RADIUS_CIRCLE;
        dot.border_width = 0;
        lv_area_t r{
            static_cast<lv_coord_t>(area.x1 + x - radius),
            static_cast<lv_coord_t>(area.y1 + y - radius),
            static_cast<lv_coord_t>(area.x1 + x + radius),
            static_cast<lv_coord_t>(area.y1 + y + radius)};
        lv_draw_rect(drawCtx, &dot, &r);
    };

    // Same compact bus/trunk geometry as the editor graph, but with a caller
    // supplied electrical-status color. This avoids one full-width line per
    // branch on 1:N/N:1/common nets.
    auto drawNet = [&](uint64_t aMask,
                       uint64_t bMask,
                       lv_color_t color,
                       uint8_t lineWidth,
                       lv_opa_t opacity) {
        uint8_t aSlots[kTestPointsPerSide]{};
        uint8_t bSlots[kTestPointsPerSide]{};
        uint8_t aCount = 0U;
        uint8_t bCount = 0U;
        for (uint8_t index = 0U; index < kTestPointsPerSide; ++index) {
            if ((aMask & (1ULL << index)) != 0ULL) {
                const int16_t slot = findSlot(index);
                if (slot >= 0 && profile_.pointEnabledOnSide(true, index)) {
                    aSlots[aCount++] = static_cast<uint8_t>(slot);
                }
            }
            if ((bMask & (1ULL << index)) != 0ULL) {
                const int16_t slot = findSlot(index);
                if (slot >= 0 && profile_.pointEnabledOnSide(false, index)) {
                    bSlots[bCount++] = static_cast<uint8_t>(slot);
                }
            }
        }
        if (aCount == 0U && bCount == 0U) return;

        lv_draw_line_dsc_t line{};
        lv_draw_line_dsc_init(&line);
        line.width = lineWidth;
        line.color = color;
        line.opa = opacity;

        int16_t rootAY = aCount > 0U ? pointY(aSlots[0], count, height) : 0;
        int16_t rootBY = bCount > 0U ? pointY(bSlots[0], count, height) : 0;
        if (aCount > 0U && bCount > 0U) {
            int32_t bestDistance = 0x7FFFFFFF;
            for (uint8_t ai = 0U; ai < aCount; ++ai) {
                const int16_t ay = pointY(aSlots[ai], count, height);
                for (uint8_t bi = 0U; bi < bCount; ++bi) {
                    const int16_t by = pointY(bSlots[bi], count, height);
                    const int32_t distance = ay > by ? ay - by : by - ay;
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        rootAY = ay;
                        rootBY = by;
                    }
                }
            }
        }

        if (aCount > 1U) {
            int16_t minY = pointY(aSlots[0], count, height);
            int16_t maxY = minY;
            for (uint8_t i = 0U; i < aCount; ++i) {
                const int16_t y = pointY(aSlots[i], count, height);
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
                drawSegment(line, kLeftNodeX, y, leftJoinX, y);
                drawJunction(leftJoinX, y, color, count <= 25U ? 2 : 1);
            }
            drawSegment(line, leftJoinX, minY, leftJoinX, maxY);
        }
        if (bCount > 1U) {
            int16_t minY = pointY(bSlots[0], count, height);
            int16_t maxY = minY;
            for (uint8_t i = 0U; i < bCount; ++i) {
                const int16_t y = pointY(bSlots[i], count, height);
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
                drawSegment(line, rightJoinX, y, rightNodeX, y);
                drawJunction(rightJoinX, y, color, count <= 25U ? 2 : 1);
            }
            drawSegment(line, rightJoinX, minY, rightJoinX, maxY);
        }
        if (aCount > 0U && bCount > 0U) {
            const int16_t x1 = aCount > 1U ? leftJoinX : kLeftNodeX;
            const int16_t x2 = bCount > 1U ? rightJoinX : rightNodeX;
            drawSegment(line, x1, rootAY, x2, rootBY);
            if (aCount > 1U) drawJunction(leftJoinX, rootAY, color, count <= 25U ? 2 : 1);
            if (bCount > 1U) drawJunction(rightJoinX, rootBY, color, count <= 25U ? 2 : 1);
        }
    };

    auto drawOpenStub = [&](bool sideA, uint8_t testIndex, lv_color_t color) {
        const int16_t slot = findSlot(testIndex);
        if (slot < 0) return;
        const int16_t y = pointY(static_cast<uint8_t>(slot), count, height);
        lv_draw_line_dsc_t line{};
        lv_draw_line_dsc_init(&line);
        line.width = count <= 25U ? 3 : 2;
        line.color = color;
        line.opa = LV_OPA_COVER;
        const int16_t nodeX = sideA ? kLeftNodeX : rightNodeX;
        const int16_t endX = sideA ? static_cast<int16_t>(nodeX + 80)
                                   : static_cast<int16_t>(nodeX - 80);
        drawSegment(line, nodeX, y, endX, y);
        // Small X at the open end.
        drawSegment(line, endX - 5, y - 5, endX + 5, y + 5);
        drawSegment(line, endX - 5, y + 5, endX + 5, y - 5);
    };

    // Row guides.
    lv_draw_line_dsc_t guide{};
    lv_draw_line_dsc_init(&guide);
    guide.width = 1;
    guide.color = lv_color_hex(0x16303D);
    guide.opa = LV_OPA_70;
    for (uint8_t slot = 0U; slot < count; ++slot) {
        const int16_t y = pointY(slot, count, height);
        drawSegment(guide, kLeftNodeX, y, rightNodeX, y);
    }

    // Expected geometry is intentionally subdued in COMBINED mode. The
    // measured line is drawn on top in the status color, so a wrong mapping is
    // visually obvious as grey expected + red actual.
    if (resultView_ != ResultGraphView::Measured) {
        const lv_color_t expectedColor = lv_color_hex(0x7A8992);
        const lv_opa_t expectedOpa = resultView_ == ResultGraphView::Combined
                                         ? LV_OPA_50 : LV_OPA_COVER;
        for (uint8_t slot = 0U; slot < count; ++slot) {
            const uint8_t sourceIndex = profile_.slotToTestIndex(slot);
            if (!profile_.pointEnabledOnSide(true, sourceIndex)) continue;
            const Measurement report = resultSession_->reportMeasurement(ScanDirection::AtoB, slot);
            if (filterVisible_ && !resultMatchesFilter(report.result)) continue;

            const uint64_t aMask = map_.senderMaskA(sourceIndex) & profile_.sidePointMask(true);
            const uint64_t bMask = map_.receiverMaskAtoB(sourceIndex) & profile_.sidePointMask(false);
            if (lowestBit(aMask) != sourceIndex) continue;  // one expected net, one draw
            if (bMask == 0ULL) drawOpenStub(true, sourceIndex, expectedColor);
            else drawNet(aMask, bMask, expectedColor, 1U, expectedOpa);
        }
        // Expected B-only/same-side groups that have no A trunk.
        for (uint8_t slot = 0U; slot < count; ++slot) {
            const uint8_t sourceIndex = profile_.slotToTestIndex(slot);
            const uint64_t bMask = map_.senderMaskB(sourceIndex) & profile_.sidePointMask(false);
            if (popcount(bMask) <= 1U || lowestBit(bMask) != sourceIndex) continue;
            const Measurement report = resultSession_->reportMeasurement(ScanDirection::BtoA, slot);
            if (filterVisible_ && !resultMatchesFilter(report.result)) continue;
            drawNet(0ULL, bMask, expectedColor, 1U, expectedOpa);
        }
    }

    if (resultView_ != ResultGraphView::Expected) {
        for (uint8_t slot = 0U; slot < count; ++slot) {
            const uint8_t sourceIndex = profile_.slotToTestIndex(slot);
            const Measurement m = resultSession_->reportMeasurement(ScanDirection::AtoB, slot);
            if (!resultMatchesFilter(m.result)) continue;
            const lv_color_t color = resultColor(m.result);
            const uint64_t aMask = (m.senderGroupMask != 0ULL
                                        ? m.senderGroupMask
                                        : (1ULL << sourceIndex)) & profile_.sidePointMask(true);
            const uint64_t bMask = m.actualReceiverMask & profile_.sidePointMask(false);
            if (lowestBit(aMask) != sourceIndex) continue;  // de-duplicate common sender groups
            if (m.result == ElectricalResult::Open && bMask == 0ULL) {
                drawOpenStub(true, sourceIndex, color);
            } else {
                drawNet(aMask, bMask, color, count <= 25U ? 3U : 2U, LV_OPA_COVER);
            }
        }

        // A B-side same-side short/common group is best observed in B->A. Draw
        // only the B-local bus here; cross-side wiring already came from A->B
        // and would otherwise be duplicated in the result graph.
        for (uint8_t slot = 0U; slot < count; ++slot) {
            const uint8_t sourceIndex = profile_.slotToTestIndex(slot);
            const Measurement m = resultSession_->reportMeasurement(ScanDirection::BtoA, slot);
            if (!resultMatchesFilter(m.result)) continue;
            const uint64_t bMask = (m.senderGroupMask != 0ULL
                                        ? m.senderGroupMask
                                        : (1ULL << sourceIndex)) & profile_.sidePointMask(false);
            if (popcount(bMask) <= 1U || lowestBit(bMask) != sourceIndex) continue;
            drawNet(0ULL, bMask, resultColor(m.result), count <= 25U ? 3U : 2U, LV_OPA_COVER);
        }
    }

    // Endpoint dots are status colored in result mode. When a filter is
    // active, unrelated pins remain visible as dim context rather than moving
    // or renumbering the graph.
    const int16_t radius = count <= 25U ? 3 : 2;
    for (uint8_t slot = 0U; slot < count; ++slot) {
        const uint8_t testIndex = visibleTestIndex(slot);
        const int16_t y = pointY(slot, count, height);
        const ElectricalResult aResult = worstResultForPoint(true, testIndex);
        const ElectricalResult bResult = worstResultForPoint(false, testIndex);
        const lv_color_t aColor = resultMatchesFilter(aResult)
                                      ? resultColor(aResult) : lv_color_hex(0x40505A);
        const lv_color_t bColor = resultMatchesFilter(bResult)
                                      ? resultColor(bResult) : lv_color_hex(0x40505A);
        lv_draw_rect_dsc_t node{};
        lv_draw_rect_dsc_init(&node);
        node.bg_opa = LV_OPA_COVER;
        node.radius = LV_RADIUS_CIRCLE;
        node.border_width = 0;
        if (profile_.pointEnabledOnSide(true, testIndex)) {
            node.bg_color = aResult == ElectricalResult::NotMeasured
                                ? lv_color_hex(0x40505A) : aColor;
            lv_area_t r{
                static_cast<lv_coord_t>(area.x1 + kLeftNodeX - radius),
                static_cast<lv_coord_t>(area.y1 + y - radius),
                static_cast<lv_coord_t>(area.x1 + kLeftNodeX + radius),
                static_cast<lv_coord_t>(area.y1 + y + radius)};
            lv_draw_rect(drawCtx, &node, &r);
        }
        if (profile_.pointEnabledOnSide(false, testIndex)) {
            node.bg_color = bResult == ElectricalResult::NotMeasured
                                ? lv_color_hex(0x40505A) : bColor;
            lv_area_t r{
                static_cast<lv_coord_t>(area.x1 + rightNodeX - radius),
                static_cast<lv_coord_t>(area.y1 + y - radius),
                static_cast<lv_coord_t>(area.x1 + rightNodeX + radius),
                static_cast<lv_coord_t>(area.y1 + y + radius)};
            lv_draw_rect(drawCtx, &node, &r);
        }
    }
}

void MapGraphScreen::drawCallback(lv_event_t* event) {
    auto* owner = static_cast<MapGraphScreen*>(lv_event_get_user_data(event));
    if (owner == nullptr) {
        return;
    }
    owner->drawGraph(lv_event_get_draw_ctx(event), lv_event_get_target(event));
}

void MapGraphScreen::resultControlCallback(lv_event_t* event) {
    auto* binding = static_cast<ResultBinding*>(lv_event_get_user_data(event));
    if (binding == nullptr || binding->owner == nullptr) return;
    MapGraphScreen* owner = binding->owner;
    switch (binding->control) {
        case ResultControl::ViewCombined: owner->resultView_ = ResultGraphView::Combined; break;
        case ResultControl::ViewExpected: owner->resultView_ = ResultGraphView::Expected; break;
        case ResultControl::ViewMeasured: owner->resultView_ = ResultGraphView::Measured; break;
        case ResultControl::FilterAll:    owner->resultFilter_ = ResultGraphFilter::All; break;
        case ResultControl::FilterErrors: owner->resultFilter_ = ResultGraphFilter::Errors; break;
        case ResultControl::FilterOk:     owner->resultFilter_ = ResultGraphFilter::Ok; break;
        case ResultControl::FilterOpen:   owner->resultFilter_ = ResultGraphFilter::Open; break;
        case ResultControl::FilterShort:  owner->resultFilter_ = ResultGraphFilter::ShortCircuit; break;
        case ResultControl::FilterWrong:  owner->resultFilter_ = ResultGraphFilter::WrongConnection; break;
        case ResultControl::FilterHighR:  owner->resultFilter_ = ResultGraphFilter::HighResistance; break;
    }
    owner->refresh();
}

void MapGraphScreen::backCallback(lv_event_t* event) {
    auto* owner = static_cast<MapGraphScreen*>(lv_event_get_user_data(event));
    if (owner != nullptr) {
        owner->backRequested_ = true;
    }
}

bool MapGraphScreen::consumeBackRequest() {
    const bool requested = backRequested_;
    backRequested_ = false;
    return requested;
}

}  // namespace mg::p4
