#include "CompletionReportScreen.h"

#include <stdio.h>
#include <string.h>

#include "ConfigP4.h"
#include "P4Fonts.h"
#include "P4Localization.h"
#include "ReportFormatter.h"

namespace mg::p4 {
namespace {

lv_obj_t* makeLabel(lv_obj_t* parent,
                    const char* text,
                    const lv_font_t* font,
                    lv_color_t color) {
    lv_obj_t* item = lv_label_create(parent);
    lv_label_set_text(item, text != nullptr ? text : "");
    lv_obj_set_style_text_font(item, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(item, color, LV_PART_MAIN);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_CLICKABLE);
    return item;
}

void flat(lv_obj_t* object) {
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

static constexpr lv_coord_t kCompletionColumnWidths[7] = {
    // Sum = 976 px: exactly the visible table width on JC1060.
    42, 118, 150, 250, 108, 128, 180
};

lv_color_t verdictColor(TestWorkflowVerdict verdict) {
    switch (verdict) {
        case TestWorkflowVerdict::Pass: return lv_color_hex(0x77E08A);
        case TestWorkflowVerdict::Warning: return lv_color_hex(0xFFD36A);
        case TestWorkflowVerdict::Fail: return lv_color_hex(0xFF7E86);
        case TestWorkflowVerdict::None: default: return lv_color_hex(0xDDEBF2);
    }
}

void appendText(char* output, size_t outputSize, const char* text) {
    if (output == nullptr || outputSize == 0U || text == nullptr) return;
    const size_t used = strlen(output);
    if (used + 1U >= outputSize) return;
    snprintf(output + used, outputSize - used, "%s", text);
}

void formatIndex(uint8_t index, char side, char* output, size_t outputSize) {
    if (index == kPeTestIndex) snprintf(output, outputSize, "%cPE", side);
    else if (index == kDrainShieldTestIndex) snprintf(output, outputSize, "%cdS", side);
    else if (index <= kLastSignalTestIndex) snprintf(output, outputSize, "%c%u", side, static_cast<unsigned>(index));
    else snprintf(output, outputSize, "-");
}

void formatBareIndex(uint8_t index, char* output, size_t outputSize) {
    if (index == kPeTestIndex) snprintf(output, outputSize, "PE");
    else if (index == kDrainShieldTestIndex) snprintf(output, outputSize, "dS");
    else if (index <= kLastSignalTestIndex) snprintf(output, outputSize, "%u", static_cast<unsigned>(index));
    else snprintf(output, outputSize, "-");
}

void appendFaultSeparator(char* output, size_t outputSize) {
    if (output == nullptr || output[0] == '\0') return;
    appendText(output, outputSize, " | ");
}

void appendBareIndexList(const uint8_t* indices,
                         uint16_t count,
                         char* output,
                         size_t outputSize) {
    const uint16_t shown = count < kOperatorFaultSummaryMaxItems
        ? count
        : static_cast<uint16_t>(kOperatorFaultSummaryMaxItems);
    for (uint16_t i = 0U; i < shown; ++i) {
        if (i != 0U) appendText(output, outputSize, ",");
        char item[8]{};
        formatBareIndex(indices[i], item, sizeof(item));
        appendText(output, outputSize, item);
    }
    if (count > shown) {
        char more[16]{};
        snprintf(more, sizeof(more), "+%u", static_cast<unsigned>(count - shown));
        appendText(output, outputSize, more);
    }
}

void appendMaskIndexList(uint64_t mask, char* output, size_t outputSize) {
    bool first = true;
    for (uint8_t index = 0U; index < 64U; ++index) {
        if ((mask & (1ULL << index)) == 0ULL) continue;
        if (!first) appendText(output, outputSize, "/");
        char item[8]{};
        formatBareIndex(index, item, sizeof(item));
        appendText(output, outputSize, item);
        first = false;
    }
    if (first) appendText(output, outputSize, "-");
}

void formatOperatorFaultSummaryText(const OperatorPhysicalFaultSummary& summary,
                                    char* output,
                                    size_t outputSize) {
    if (output == nullptr || outputSize == 0U) return;
    output[0] = '\0';
    appendText(output, outputSize,
               p4SelectText("Hata özeti: ", "Fault summary: ", "Foutoverzicht: ",
                            "Fehlerübersicht: ", "Résumé défauts : ",
                            "Resumen de fallos: ", "Podsumowanie usterek: "));

    if (summary.physicalIssueCount() == 0U) {
        appendText(output, outputSize,
                   p4SelectText("YOK", "NONE", "GEEN", "KEINE", "AUCUN", "NINGUNO", "BRAK"));
        return;
    }

    bool haveItem = false;
    if (summary.crossedPairCount != 0U) {
        appendText(output, outputSize,
                   p4SelectText("ÇAPRAZ ", "CROSSED ", "GEKRUIST ", "GEKREUZT ",
                                "CROISÉ ", "CRUZADO ", "SKRZYŻOWANE "));
        const uint16_t shown = summary.crossedPairCount < kOperatorFaultSummaryMaxItems
            ? summary.crossedPairCount
            : static_cast<uint16_t>(kOperatorFaultSummaryMaxItems);
        for (uint16_t i = 0U; i < shown; ++i) {
            if (i != 0U) appendText(output, outputSize, ",");
            char first[8]{};
            char second[8]{};
            formatBareIndex(summary.crossedPairFirst[i], first, sizeof(first));
            formatBareIndex(summary.crossedPairSecond[i], second, sizeof(second));
            char pair[20]{};
            snprintf(pair, sizeof(pair), "%s/%s", first, second);
            appendText(output, outputSize, pair);
        }
        if (summary.crossedPairCount > shown) {
            char more[16]{};
            snprintf(more, sizeof(more), "+%u",
                     static_cast<unsigned>(summary.crossedPairCount - shown));
            appendText(output, outputSize, more);
        }
        haveItem = true;
    }

    if (summary.openLineCount != 0U) {
        if (haveItem) appendFaultSeparator(output, outputSize);
        appendText(output, outputSize,
                   p4SelectText("AÇIK ", "OPEN ", "OPEN ", "OFFEN ",
                                "OUVERT ", "ABIERTO ", "PRZERWA "));
        appendBareIndexList(summary.openLine, summary.openLineCount, output, outputSize);
        haveItem = true;
    }

    if (summary.shortGroupCount != 0U) {
        if (haveItem) appendFaultSeparator(output, outputSize);
        appendText(output, outputSize,
                   p4SelectText("KISA ", "SHORT ", "KORTSLUITING ", "KURZSCHLUSS ",
                                "COURT-CIRCUIT ", "CORTO ", "ZWARCIE "));
        const uint16_t shown = summary.shortGroupCount < kOperatorFaultSummaryMaxItems
            ? summary.shortGroupCount
            : static_cast<uint16_t>(kOperatorFaultSummaryMaxItems);
        for (uint16_t i = 0U; i < shown; ++i) {
            if (i != 0U) appendText(output, outputSize, ",");
            appendMaskIndexList(summary.shortGroupMask[i], output, outputSize);
        }
        if (summary.shortGroupCount > shown) {
            char more[16]{};
            snprintf(more, sizeof(more), "+%u",
                     static_cast<unsigned>(summary.shortGroupCount - shown));
            appendText(output, outputSize, more);
        }
        haveItem = true;
    }

    if (summary.highResistanceLineCount != 0U) {
        if (haveItem) appendFaultSeparator(output, outputSize);
        appendText(output, outputSize,
                   p4SelectText("YÜKSEK-R ", "HIGH-R ", "HOGE-R ", "HOHER-R ",
                                "R-ÉLEVÉE ", "R-ALTA ", "WYSOKIE-R "));
        appendBareIndexList(summary.highResistanceLine, summary.highResistanceLineCount,
                            output, outputSize);
        haveItem = true;
    }

    if (summary.wrongLineCount != 0U) {
        if (haveItem) appendFaultSeparator(output, outputSize);
        appendText(output, outputSize,
                   p4SelectText("YANLIŞ ", "WRONG ", "FOUT ", "FALSCH ",
                                "ERREUR ", "INCORRECTO ", "BŁĘDNE "));
        const uint16_t shown = summary.wrongLineCount < kOperatorFaultSummaryMaxItems
            ? summary.wrongLineCount
            : static_cast<uint16_t>(kOperatorFaultSummaryMaxItems);
        for (uint16_t i = 0U; i < shown; ++i) {
            if (i != 0U) appendText(output, outputSize, ",");
            char source[8]{};
            char actual[8]{};
            formatBareIndex(summary.wrongSource[i], source, sizeof(source));
            formatBareIndex(summary.wrongActual[i], actual, sizeof(actual));
            char route[24]{};
            snprintf(route, sizeof(route), "A%s->B%s", source, actual);
            appendText(output, outputSize, route);
        }
        if (summary.wrongLineCount > shown) {
            char more[16]{};
            snprintf(more, sizeof(more), "+%u",
                     static_cast<unsigned>(summary.wrongLineCount - shown));
            appendText(output, outputSize, more);
        }
    }
}

uint8_t bitCount64(uint64_t value) {
#if defined(__GNUC__)
    return static_cast<uint8_t>(__builtin_popcountll(value));
#else
    uint8_t count = 0U;
    while (value != 0ULL) {
        value &= value - 1ULL;
        ++count;
    }
    return count;
#endif
}

const char* displayElectricalResult(ElectricalResult result) {
    const P4UiTexts& t = p4Texts();
    switch (result) {
        case ElectricalResult::Ok: return t.legendOk;
        case ElectricalResult::Open: return t.legendOpen;
        case ElectricalResult::ShortCircuit: return t.legendShort;
        case ElectricalResult::WrongConnection: return t.legendWrong;
        case ElectricalResult::HighResistance: return t.legendResistance;
        case ElectricalResult::NotMeasured:
        default:
            return p4SelectText("ÖLÇÜLMEDİ", "NOT MEASURED", "NIET GEMETEN",
                                "NICHT GEMESSEN", "NON MESURÉ", "NO MEDIDO",
                                "NIE ZMIERZONO");
    }
}

}  // namespace

MgResultStatus CompletionReportScreen::resultRowStatus(ElectricalResult result) {
    switch (result) {
        case ElectricalResult::Ok:              return MgResultStatus::Pass;
        case ElectricalResult::Open:            return MgResultStatus::Open;
        case ElectricalResult::ShortCircuit:    return MgResultStatus::ShortCircuit;
        case ElectricalResult::WrongConnection: return MgResultStatus::WrongConnection;
        case ElectricalResult::HighResistance:  return MgResultStatus::HighResistance;
        case ElectricalResult::NotMeasured:
        default:                                return MgResultStatus::NotMeasured;
    }
}

void CompletionReportScreen::copyText(char* dst, size_t dstSize, const char* src) {
    if (dst == nullptr || dstSize == 0U) return;
    snprintf(dst, dstSize, "%s", src != nullptr ? src : "");
}

void CompletionReportScreen::beginTest(TestWorkflowController& workflow,
                                       const ScanSession& session) {
    kind_ = CompletionReportKind::Test;
    workflow_ = &workflow;
    session_ = &session;
    pendingAction_ = CompletionReportAction::None;
    rebuildTestEntries();
    buildUi();
    refresh();
}

void CompletionReportScreen::beginCableLearn(const CableProfile& profile,
                                             const CableMap& learnedMap,
                                             size_t learnedNetCount,
                                             bool demoMode) {
    kind_ = CompletionReportKind::CableLearn;
    workflow_ = nullptr;
    session_ = nullptr;
    pendingAction_ = CompletionReportAction::None;
    learnedProfile_ = profile;
    copyText(learnedProfileName_, sizeof(learnedProfileName_), profile.name);
    copyText(learnedConnectorA_, sizeof(learnedConnectorA_), profile.sideA.displayName);
    copyText(learnedConnectorB_, sizeof(learnedConnectorB_), profile.sideB.displayName);
    learnedProfile_.name = learnedProfileName_;
    learnedProfile_.sideA.displayName = learnedConnectorA_;
    learnedProfile_.sideB.displayName = learnedConnectorB_;
    learnedMap_ = learnedMap;
    learnedNetCount_ = learnedNetCount;
    learnedDemoMode_ = demoMode;
    learnedProfileSaved_ = false;
    rebuildLearnedNets();
    buildUi();
    refresh();
}

void CompletionReportScreen::rebuildTestEntries() {
    testEntryCount_ = 0U;
    if (session_ == nullptr) return;

    // Preserve the natural electrical scan order. Result class only changes
    // the row color; OK/OPEN/SHORT/WRONG/HIGH-R must never reorder a wire.
    for (uint8_t directionPass = 0U; directionPass < 2U; ++directionPass) {
        const ScanDirection direction = directionPass == 0U ? ScanDirection::AtoB : ScanDirection::BtoA;
        for (uint8_t slot = 0U; slot < session_->profile().activePointCount(); ++slot) {
            const Measurement m = session_->reportMeasurement(direction, slot);
            if (m.result == ElectricalResult::NotMeasured) continue;
            if (testEntryCount_ < (kTestPointsPerSide * 2U)) {
                testEntries_[testEntryCount_++] = {direction, slot};
            }
        }
    }
}

void CompletionReportScreen::rebuildLearnedNets() {
    const size_t built = CableNetGraph::build(learnedMap_,
                                              learnedProfile_.sidePointMask(true),
                                              learnedProfile_.sidePointMask(false),
                                              learnedNets_,
                                              kTestPointsPerSide);
    if (built != 0U || learnedNetCount_ == 0U) learnedNetCount_ = built;
}

void CompletionReportScreen::configureReportTables() {
    resultTable_.setHeader(0U, "#");
    if (kind_ == CompletionReportKind::Test) {
        resultTable_.setHeader(1U, p4SelectText("KAYNAK", "SOURCE", "BRON", "QUELLE", "SOURCE", "ORIGEN", "ŹRÓDŁO"));
        resultTable_.setHeader(2U, p4SelectText("BEKLENEN", "EXPECTED", "VERWACHT", "ERWARTET", "ATTENDU", "ESPERADO", "OCZEKIWANE"));
        resultTable_.setHeader(3U, p4SelectText("BULUNAN", "MEASURED", "GEVONDEN", "GEMESSEN", "MESURÉ", "MEDIDO", "ZMIERZONE"));
        resultTable_.setHeader(4U, (session_ != nullptr && session_->isDemo()) ? "R (mOhm) [SIM]" : "R (mOhm)");
        resultTable_.setHeader(5U, p4SelectText("R DURUM", "R STATE", "R STATUS", "R STATUS", "ÉTAT R", "ESTADO R", "STAN R"));
        resultTable_.setHeader(6U, p4SelectText("SONUÇ", "RESULT", "RESULTAAT", "ERGEBNIS", "RÉSULTAT", "RESULTADO", "WYNIK"));
    } else {
        resultTable_.setHeader(1U, p4SelectText("A TARAFI", "SIDE A", "ZIJDE A", "SEITE A", "CÔTÉ A", "LADO A", "STRONA A"));
        resultTable_.setHeader(2U, p4SelectText("B TARAFI", "SIDE B", "ZIJDE B", "SEITE B", "CÔTÉ B", "LADO B", "STRONA B"));
        resultTable_.setHeader(3U, p4SelectText("NET TİPİ", "NET TYPE", "NETTYPE", "NETZTYP", "TYPE DE NET", "TIPO DE RED", "TYP SIECI"));
        resultTable_.setHeader(4U, "R (mOhm)");
        resultTable_.setHeader(5U, p4SelectText("R DURUM", "R STATE", "R STATUS", "R STATUS", "ÉTAT R", "ESTADO R", "STAN R"));
        resultTable_.setHeader(6U, p4SelectText("DURUM", "STATE", "STATUS", "STATUS", "ÉTAT", "ESTADO", "STAN"));
    }
}

void CompletionReportScreen::buildUi() {
    if (screen_ == nullptr) screen_ = lv_obj_create(nullptr); else lv_obj_clean(screen_);
    lv_scr_load(screen_);
    flat(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x071019), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* header = lv_obj_create(screen_);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, kDisplayWidth, 70);
    flat(header);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x173345), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);

    titleLabel_ = makeLabel(header, "", p4Font20(), lv_color_hex(0xF1FAFF));
    lv_obj_set_pos(titleLabel_, 28, 19);
    verdictLabel_ = makeLabel(header, "", p4Font20(), lv_color_hex(0xDDEBF2));
    lv_obj_align(verdictLabel_, LV_ALIGN_TOP_RIGHT, -30, 19);

    identityLabel_ = makeLabel(screen_, "", p4Font14(), lv_color_hex(0xB8CBD5));
    lv_obj_set_pos(identityLabel_, 24, 79);
    lv_obj_set_width(identityLabel_, 976);
    lv_label_set_long_mode(identityLabel_, LV_LABEL_LONG_DOT);

    countsLabel_ = makeLabel(screen_, "", p4Font14(), lv_color_hex(0xDDEBF2));
    lv_obj_set_pos(countsLabel_, 24, 109);
    lv_obj_set_width(countsLabel_, 976);
    lv_label_set_long_mode(countsLabel_, LV_LABEL_LONG_DOT);

    detailsTitleLabel_ = makeLabel(screen_, "", p4Font14(), lv_color_hex(0x8FE4FF));
    lv_obj_set_pos(detailsTitleLabel_, 24, 139);
    lv_obj_set_width(detailsTitleLabel_, 976);
    lv_label_set_long_mode(detailsTitleLabel_, LV_LABEL_LONG_DOT);

    resultTable_.create(screen_, 24, 168, 976, 38, 207, 286,
                        kTableColumnCount, kCompletionColumnWidths, p4Font14());
    configureReportTables();

    // Fixed bottom action bar. Scrolling never moves these controls.
    // EXTRA31 adds the same GRAPH view used by the map editor to finite test
    // results and Cable Learn results.
    const lv_coord_t y = 510;
    if (kind_ == CompletionReportKind::Test) {
        actionButtons_[0] = makeButton(screen_, 18, y, 200,
            p4SelectText("ANA MENÜ", "MAIN MENU", "HOOFDMENU", "HAUPTMENÜ", "MENU PRINCIPAL", "MENÚ PRINCIPAL", "MENU GŁÓWNE"), lv_color_hex(0x4C5962), 0U,
            CompletionReportAction::MainMenu);
        actionButtons_[1] = makeButton(screen_, 228, y, 200,
            p4SelectText("TEKRAR TEST", "RETEST", "OPNIEUW TESTEN", "ERNEUT TESTEN", "RETESTER", "REPETIR PRUEBA", "TEST PONOWNIE"), lv_color_hex(0x1B8C63), 1U,
            CompletionReportAction::Retest);
        actionButtons_[2] = makeButton(screen_, 438, y, 260,
            p4SelectText("GRAFİKLE GÖSTER", "SHOW GRAPH", "TOON GRAFIEK", "GRAFIK ZEIGEN", "AFFICHER GRAPHE", "MOSTRAR GRÁFICO", "POKAŻ WYKRES"), lv_color_hex(0x176D91), 2U,
            CompletionReportAction::Graph);
        actionButtons_[3] = makeButton(screen_, 708, y, 298,
            p4SelectText("KAYITLAR", "RECORDS", "RECORDS", "AUFZEICHNUNGEN", "ENREGISTREMENTS", "REGISTROS", "REJESTRY"), lv_color_hex(0x166E7A), 3U,
            CompletionReportAction::Records);
    } else {
        actionButtons_[0] = makeButton(screen_, 18, y, 190,
            p4SelectText("ANA MENÜ", "MAIN MENU", "HOOFDMENU", "HAUPTMENÜ", "MENU PRINCIPAL", "MENÚ PRINCIPAL", "MENU GŁÓWNE"), lv_color_hex(0x4C5962), 0U,
            CompletionReportAction::MainMenu);
        actionButtons_[1] = makeButton(screen_, 218, y, 240,
            p4SelectText("ÖĞRENMEYE DÖN", "RETURN TO LEARN", "TERUG NAAR LEREN", "ZUM LERNEN ZURÜCK", "RETOUR À L'APPRENTISSAGE", "VOLVER A APRENDER", "WRÓĆ DO UCZENIA"), lv_color_hex(0x1B8C63), 1U,
            CompletionReportAction::ReturnToLearning);
        actionButtons_[2] = makeButton(screen_, 468, y, 220,
            p4SelectText("GRAFİKLE GÖSTER", "SHOW GRAPH", "TOON GRAFIEK", "GRAFIK ZEIGEN", "AFFICHER GRAPHE", "MOSTRAR GRÁFICO", "POKAŻ WYKRES"), lv_color_hex(0x176D91), 2U,
            CompletionReportAction::Graph);
        actionButtons_[4] = makeButton(screen_, 698, y, 308,
            p4SelectText("PROFİLİ KAYDET", "SAVE PROFILE", "PROFIEL OPSLAAN", "PROFIL SPEICHERN", "ENREGISTRER LE PROFIL", "GUARDAR PERFIL", "ZAPISZ PROFIL"), lv_color_hex(0x166E7A), 4U,
            CompletionReportAction::SaveLearnedProfile);
    }
}

lv_obj_t* CompletionReportScreen::makeButton(lv_obj_t* parent,
                                             lv_coord_t x,
                                             lv_coord_t y,
                                             lv_coord_t w,
                                             const char* text,
                                             lv_color_t color,
                                             uint8_t bindingIndex,
                                             CompletionReportAction action) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, 72);
    lv_obj_set_style_bg_color(button, color, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 12, LV_PART_MAIN);
    bindings_[bindingIndex] = {this, action};
    lv_obj_add_event_cb(button, buttonCallback, LV_EVENT_CLICKED, &bindings_[bindingIndex]);
    actionLabels_[bindingIndex] = makeLabel(button, text, p4Font14(), lv_color_hex(0xFFFFFF));
    lv_obj_set_width(actionLabels_[bindingIndex], w - 12);
    lv_obj_set_style_text_align(actionLabels_[bindingIndex], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(actionLabels_[bindingIndex]);
    return button;
}

void CompletionReportScreen::buttonCallback(lv_event_t* event) {
    auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
    if (binding == nullptr || binding->owner == nullptr) return;
    binding->owner->pendingAction_ = binding->action;
}

void CompletionReportScreen::formatNetMask(uint64_t mask,
                                           char side,
                                           char* output,
                                           size_t outputSize) {
    if (output == nullptr || outputSize == 0U) return;
    output[0] = '\0';
    for (uint8_t i = 0U; i < kTestPointsPerSide; ++i) {
        if ((mask & (1ULL << i)) == 0ULL) continue;
        char item[12]{};
        formatIndex(i, side, item, sizeof(item));
        if (output[0] != '\0') appendText(output, outputSize, "+");
        appendText(output, outputSize, item);
    }
    if (output[0] == '\0') snprintf(output, outputSize, "-");
}

const char* CompletionReportScreen::learnedNetType(const CableNet& net) {
    const uint8_t a = bitCount64(net.aMask);
    const uint8_t b = bitCount64(net.bMask);
    if (a == 1U && b == 1U) return "1:1";
    if (a == 1U && b > 1U) return "1:N";
    if (a > 1U && b == 1U) return "N:1";
    if (a > 1U && b > 1U) return "N:N";
    if (a > 1U && b == 0U) return "A-A";
    if (a == 0U && b > 1U) return "B-B";
    return "NET";
}

void CompletionReportScreen::refreshHeader() {
    char text[512]{};
    if (kind_ == CompletionReportKind::Test && workflow_ != nullptr && session_ != nullptr) {
        const auto& snap = workflow_->snapshot();
        lv_label_set_text(titleLabel_, p4SelectText("TEST SONU RAPORU", "END-OF-TEST REPORT", "TESTEINDRAPPORT", "TESTABSCHLUSSBERICHT", "RAPPORT DE FIN DE TEST", "INFORME FINAL DE PRUEBA", "RAPORT KOŃCOWY TESTU"));
        lv_label_set_text(verdictLabel_, TestWorkflowController::verdictText(snap.verdict));
        lv_obj_set_style_text_color(verdictLabel_, verdictColor(snap.verdict), LV_PART_MAIN);

        const char* productionPr = snap.identity.productionPr[0] ? snap.identity.productionPr : "-";
        snprintf(text, sizeof(text), "PR=%s%s",
                 productionPr,
                 session_->isDemo() ? " | DEMO/SIM" : "");
        lv_label_set_text(identityLabel_, text);

        const TestReportStats stats = buildTestReportStats(*session_);
        const OperatorPhysicalFaultSummary physical = buildOperatorPhysicalFaultSummary(*session_);
        if (physical.identityOneToOne) {
            snprintf(text, sizeof(text),
                     "%s %u | OK %u | %s %u%s",
                     p4SelectText("Ölçüm", "Measured", "Metingen", "Messungen", "Mesures", "Mediciones", "Pomiary"),
                     static_cast<unsigned>(stats.measured),
                     static_cast<unsigned>(stats.ok),
                     p4SelectText("Fiziksel hata", "Physical faults", "Fysieke fouten", "Physische Fehler",
                                  "Défauts physiques", "Fallos físicos", "Usterki fizyczne"),
                     static_cast<unsigned>(physical.physicalIssueCount()),
                     session_->isDemo()
                         ? p4SelectText(" | DEMO: fiziksel ölçüm değil", " | DEMO: not a physical measurement",
                                        " | DEMO: geen fysieke meting", " | DEMO: keine physische Messung",
                                        " | DÉMO : pas une mesure physique", " | DEMO: no es una medición física",
                                        " | DEMO: to nie jest pomiar fizyczny")
                         : "");
            lv_label_set_text(countsLabel_, text);
            char faultSummary[512]{};
            formatOperatorFaultSummaryText(physical, faultSummary, sizeof(faultSummary));
            lv_label_set_text(detailsTitleLabel_, faultSummary);
        } else {
            // Complex/common nets cannot be safely collapsed to one physical
            // count. Keep the truthful directional statistics instead.
            snprintf(text, sizeof(text),
                     "%s %u | OK %u | OPEN %u | SHORT %u | WRONG %u | HIGH-R %u | R %u/%u",
                     p4SelectText("Ölçüm", "Measured", "Metingen", "Messungen", "Mesures", "Mediciones", "Pomiary"), static_cast<unsigned>(stats.measured),
                     static_cast<unsigned>(stats.ok), static_cast<unsigned>(stats.open),
                     static_cast<unsigned>(stats.shortCircuit), static_cast<unsigned>(stats.wrongConnection),
                     static_cast<unsigned>(stats.highResistance),
                     static_cast<unsigned>(stats.resistanceMeasured),
                     static_cast<unsigned>(stats.measured));
            lv_label_set_text(countsLabel_, text);
            lv_label_set_text(detailsTitleLabel_, session_->isDemo()
                ? p4SelectText(
                    "SİMÜLASYON - sonuçlar demo test motorundandır; fiziksel ölçüm değildir. Tabloyu kaydırın.",
                    "SIMULATION - results are from the demo test engine, not physical measurements. Swipe to scroll.",
                    "SIMULATIE - resultaten komen van de demo-testmotor, niet van fysieke metingen. Veeg om te scrollen.",
                    "SIMULATION - Ergebnisse stammen aus der Demo-Testengine, nicht aus realen Messungen. Vertikal wischen.",
                    "SIMULATION - résultats issus du moteur de démonstration, pas de mesures physiques. Faites défiler.",
                    "SIMULACIÓN - resultados del motor de demostración, no mediciones físicas. Deslice para desplazarse.",
                    "SYMULACJA - wyniki pochodzą z silnika demonstracyjnego, nie z pomiarów fizycznych. Przewijaj pionowo.")
                : p4SelectText(
                    "Ayrıntı - teller tarama sırasındadır; renk durumu gösterir; tabloyu aşağı/yukarı kaydırın",
                    "Detail - wires stay in scan order; color shows status; swipe vertically to scroll",
                    "Details - draden blijven in scanvolgorde; kleur toont de status; veeg verticaal",
                    "Details - Leitungen bleiben in Scanreihenfolge; Farbe zeigt den Status; vertikal wischen",
                    "Détail - fils dans l'ordre de scan; la couleur indique l'état; faites défiler verticalement",
                    "Detalle - cables en orden de escaneo; el color indica el estado; deslice verticalmente",
                    "Szczegóły - przewody w kolejności skanowania; kolor oznacza stan; przewijaj pionowo"));
        }
    } else {
        lv_label_set_text(titleLabel_, p4SelectText("KABLO ÖĞRENME RAPORU", "CABLE LEARN REPORT", "KABELLEERRAPPORT", "KABELLERNBERICHT", "RAPPORT D'APPRENTISSAGE CÂBLE", "INFORME DE APRENDIZAJE DE CABLE", "RAPORT UCZENIA KABLA"));
        lv_label_set_text(verdictLabel_, p4SelectText("ÖĞRENME TAMAM", "LEARN COMPLETE", "LEREN VOLTOOID", "LERNEN ABGESCHLOSSEN", "APPRENTISSAGE TERMINÉ", "APRENDIZAJE COMPLETADO", "UCZENIE ZAKOŃCZONE"));
        lv_obj_set_style_text_color(verdictLabel_, lv_color_hex(0x77E08A), LV_PART_MAIN);
        snprintf(text, sizeof(text), "%s | A=%s | B=%s%s",
                 learnedProfile_.name != nullptr ? learnedProfile_.name : "LEARNED",
                 learnedProfile_.sideA.displayName != nullptr ? learnedProfile_.sideA.displayName : "A",
                 learnedProfile_.sideB.displayName != nullptr ? learnedProfile_.sideB.displayName : "B",
                 learnedDemoMode_ ? (p4SelectText(" | DEMO: fiziksel fikstür yazımı yok", " | DEMO: no physical fixture write", " | DEMO: geen fysieke fixture-write", " | DEMO: kein physisches Fixture-Schreiben", " | DEMO: aucune écriture physique du fixture", " | DEMO: sin escritura física del fixture", " | DEMO: bez fizycznego zapisu fixture")) : "");
        lv_label_set_text(identityLabel_, text);
        snprintf(text, sizeof(text), "%s: %u | %s: %u | PE A/B=%u/%u | dS A/B=%u/%u",
                 p4SelectText("Elektriksel net", "Electrical nets", "Elektrische netten", "Elektrische Netze", "Nets électriques", "Redes eléctricas", "Sieci elektryczne"), static_cast<unsigned>(learnedNetCount_),
                 p4SelectText("Sinyal pin", "Signal pins", "Signaalpinnen", "Signalpins", "Broches signal", "Pines de señal", "Piny sygnałowe"), static_cast<unsigned>(learnedProfile_.signalPinCount),
                 learnedProfile_.includePeA ? 1U : 0U, learnedProfile_.includePeB ? 1U : 0U,
                 learnedProfile_.includeDrainShieldA ? 1U : 0U,
                 learnedProfile_.includeDrainShieldB ? 1U : 0U);
        lv_label_set_text(countsLabel_, text);
        lv_label_set_text(detailsTitleLabel_, p4SelectText(
            "Öğrenilen netler - tabloyu aşağı/yukarı kaydırın; R ölçülmediyse N/A",
            "Learned nets - swipe vertically; resistance is N/A when not measured",
            "Geleerde netten - veeg verticaal; weerstand is N/A indien niet gemeten",
            "Gelernte Netze - vertikal wischen; Widerstand ist N/A wenn nicht gemessen",
            "Nets appris - faites défiler verticalement; résistance N/A si non mesurée",
            "Redes aprendidas - deslice verticalmente; resistencia N/A si no se midió",
            "Nauczone sieci - przewijaj pionowo; rezystancja N/A, jeśli nie zmierzono"));
    }
}

void CompletionReportScreen::refreshTable() {
    const size_t itemCount = kind_ == CompletionReportKind::Test ? testEntryCount_ : learnedNetCount_;
    const uint16_t rowCount = itemCount == 0U ? 1U : static_cast<uint16_t>(itemCount);
    resultTable_.setRowCount(rowCount);
    resultTable_.resetStatuses(MgResultStatus::Info);

    if (itemCount == 0U) {
        for (uint16_t column = 0U; column < kTableColumnCount; ++column) {
            resultTable_.setCell(0U, column, column == 1U ? p4SelectText("VERİ YOK", "NO DATA", "GEEN GEGEVENS", "KEINE DATEN", "AUCUNE DONNÉE", "SIN DATOS", "BRAK DANYCH") : "-");
        }
        return;
    }

    for (size_t index = 0U; index < itemCount; ++index) {
        const uint16_t rowIndex = static_cast<uint16_t>(index);
        char number[12]{};
        snprintf(number, sizeof(number), "%u", static_cast<unsigned>(index + 1U));
        resultTable_.setCell(rowIndex, 0U, number);

        if (kind_ == CompletionReportKind::Test && session_ != nullptr) {
            const TestEntry& entry = testEntries_[index];
            ReportTableRow row{};
            if (!buildReportTableRow(*session_, entry.direction, entry.slot, row)) {
                for (uint16_t column = 1U; column < kTableColumnCount; ++column) {
                    resultTable_.setCell(rowIndex, column, "-");
                }
                continue;
            }
            resultTable_.setCell(rowIndex, 1U, row.source);
            resultTable_.setCell(rowIndex, 2U, row.expected);
            resultTable_.setCell(rowIndex, 3U, row.actual);
            resultTable_.setCell(rowIndex, 4U, row.resistance);
            resultTable_.setCell(rowIndex, 5U, row.resistanceState);
            const Measurement measurement = session_->reportMeasurement(entry.direction, entry.slot);
            resultTable_.setCell(rowIndex, 6U, displayElectricalResult(measurement.result));
            resultTable_.setRowStatus(rowIndex, resultRowStatus(measurement.result));
        } else {
            char a[128]{};
            char b[128]{};
            formatNetMask(learnedNets_[index].aMask, 'A', a, sizeof(a));
            formatNetMask(learnedNets_[index].bMask, 'B', b, sizeof(b));
            resultTable_.setCell(rowIndex, 1U, a);
            resultTable_.setCell(rowIndex, 2U, b);
            resultTable_.setCell(rowIndex, 3U, learnedNetType(learnedNets_[index]));
            resultTable_.setCell(rowIndex, 4U, "-");
            resultTable_.setCell(rowIndex, 5U, "N/A");
            resultTable_.setCell(rowIndex, 6U, p4SelectText("ÖĞRENİLDİ", "LEARNED", "GELEERD", "GELERNT", "APPRIS", "APRENDIDO", "NAUCZONO"));
            resultTable_.setRowStatus(rowIndex, MgResultStatus::Info);
        }
    }
}

void CompletionReportScreen::refresh() {
    refreshHeader();
    refreshTable();
}

void CompletionReportScreen::markLearnedProfileSaved(bool success) {
    if (kind_ != CompletionReportKind::CableLearn) return;
    learnedProfileSaved_ = success;
    if (actionLabels_[4] != nullptr) {
        lv_label_set_text(actionLabels_[4], success
            ? p4SelectText("PROFİL KAYDEDİLDİ", "PROFILE SAVED", "PROFIEL OPGESLAGEN", "PROFIL GESPEICHERT", "PROFIL ENREGISTRÉ", "PERFIL GUARDADO", "PROFIL ZAPISANY")
            : p4SelectText("KAYIT HATASI", "SAVE FAILED", "OPSLAAN MISLUKT", "SPEICHERN FEHLGESCHLAGEN", "ÉCHEC ENREGISTREMENT", "ERROR AL GUARDAR", "BŁĄD ZAPISU"));
    }
    if (actionButtons_[4] != nullptr) {
        if (success) lv_obj_add_state(actionButtons_[4], LV_STATE_DISABLED);
        else lv_obj_clear_state(actionButtons_[4], LV_STATE_DISABLED);
    }
    if (verdictLabel_ != nullptr) {
        lv_label_set_text(verdictLabel_, success
            ? p4SelectText("KAYDEDİLDİ", "SAVED", "OPGESLAGEN", "GESPEICHERT", "ENREGISTRÉ", "GUARDADO", "ZAPISANO")
            : p4SelectText("KAYIT HATASI", "SAVE FAILED", "OPSLAAN MISLUKT", "SPEICHERN FEHLGESCHLAGEN", "ÉCHEC ENREGISTREMENT", "ERROR AL GUARDAR", "BŁĄD ZAPISU"));
        lv_obj_set_style_text_color(verdictLabel_,
            success ? lv_color_hex(0x77E08A) : lv_color_hex(0xFF7E86), LV_PART_MAIN);
    }
}

void CompletionReportScreen::activate() {
    pendingAction_ = CompletionReportAction::None;
    if (screen_ != nullptr && lv_scr_act() != screen_) lv_scr_load(screen_);
    refresh();
}

CompletionReportAction CompletionReportScreen::consumeAction() {
    const CompletionReportAction action = pendingAction_;
    pendingAction_ = CompletionReportAction::None;
    return action;
}

}  // namespace mg::p4
