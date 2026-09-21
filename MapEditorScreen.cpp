#include "MapEditorScreen.h"

#include <stdio.h>
#include <string.h>

#include "ConfigP4.h"
#include "P4Fonts.h"
#include "P4Localization.h"

namespace mg::p4 {
namespace {

struct EditorText {
    const char* title;
    const char* subtitle;
    const char* source;
    const char* target;
    const char* cross;
    const char* sameA;
    const char* sameB;
    const char* selected;
    const char* targets;
    const char* instructionCross;
    const char* instructionSame;
    const char* oneToOne;
    const char* clear;
    const char* graph;
    const char* save;
    const char* test;
    const char* back;
    const char* saved;
    const char* unsaved;
};

const EditorText& editorText() {
    static const EditorText tr{
        "ÖZEL HARİTA EDİTÖRÜ", "B->A otomatik oluşturulur",
        "KAYNAK", "HEDEF", "A -> B", "A <-> A", "B <-> B",
        "Seçilen", "Bağlantılar",
        "Soldan kaynak pini seçin, sağdan bir veya birden fazla hedefe dokunun.",
        "Aynı taraftaki ortak bağlantıları seçin. Kaynak pin kendiliğinden dahildir.",
        "1:1 DOLDUR", "TEMİZLE", "GRAFİKLE GÖSTER", "KAYDET", "KAYDET + TEST", "GERİ",
        "KAYDEDİLDİ", "KAYDEDİLMEDİ"};
    static const EditorText en{
        "CUSTOM MAP EDITOR", "B->A is generated automatically",
        "SOURCE", "TARGET", "A -> B", "A <-> A", "B <-> B",
        "Selected", "Connections",
        "Select a source pin on the left, then tap one or more targets on the right.",
        "Select expected same-side connections. The source pin itself is implicit.",
        "FILL 1:1", "CLEAR", "SHOW GRAPH", "SAVE", "SAVE + TEST", "BACK", "SAVED", "UNSAVED"};
    static const EditorText nl{
        "AANGEPASTE KAART EDITOR", "B->A wordt automatisch gemaakt",
        "BRON", "DOEL", "A -> B", "A <-> A", "B <-> B", "Gekozen", "Verbindingen",
        "Kies links een bronpin en tik rechts op een of meer doelen.",
        "Kies verwachte verbindingen aan dezelfde zijde. De bronpin is impliciet.",
        "VUL 1:1", "WISSEN", "TOON GRAFIEK", "OPSLAAN", "OPSLAAN + TEST", "TERUG", "OPGESLAGEN", "NIET OPGESLAGEN"};
    static const EditorText de{
        "SONDERBELEGUNG EDITOR", "B->A wird automatisch erzeugt",
        "QUELLE", "ZIEL", "A -> B", "A <-> A", "B <-> B", "Ausgewählt", "Verbindungen",
        "Links Quellpin wählen, rechts ein oder mehrere Ziele antippen.",
        "Erwartete Verbindungen auf derselben Seite wählen. Quellpin ist implizit.",
        "1:1 FÜLLEN", "LÖSCHEN", "GRAFIK ZEIGEN", "SPEICHERN", "SPEICHERN + TEST", "ZURÜCK", "GESPEICHERT", "NICHT GESPEICHERT"};
    static const EditorText fr{
        "ÉDITEUR CARTE PERSONNALISÉE", "B->A généré automatiquement",
        "SOURCE", "CIBLE", "A -> B", "A <-> A", "B <-> B", "Sélection", "Connexions",
        "Choisissez la source à gauche puis une ou plusieurs cibles à droite.",
        "Choisissez les connexions attendues du même côté. La source est implicite.",
        "REMPLIR 1:1", "EFFACER", "VOIR GRAPHE", "ENREGISTRER", "ENREG. + TEST", "RETOUR", "ENREGISTRÉ", "NON ENREGISTRÉ"};
    static const EditorText es{
        "EDITOR DE MAPA PERSONALIZADO", "B->A se genera automáticamente",
        "ORIGEN", "DESTINO", "A -> B", "A <-> A", "B <-> B", "Seleccionado", "Conexiones",
        "Seleccione el pin de origen a la izquierda y uno o más destinos a la derecha.",
        "Seleccione conexiones esperadas del mismo lado. El pin origen es implícito.",
        "LLENAR 1:1", "BORRAR", "VER GRÁFICO", "GUARDAR", "GUARDAR + TEST", "VOLVER", "GUARDADO", "SIN GUARDAR"};
    static const EditorText pl{
        "EDYTOR MAPY NIESTANDARDOWEJ", "B->A tworzone automatycznie",
        "ŹRÓDŁO", "CEL", "A -> B", "A <-> A", "B <-> B", "Wybrany", "Połączenia",
        "Wybierz pin źródłowy po lewej, potem jeden lub więcej celów po prawej.",
        "Wybierz oczekiwane połączenia po tej samej stronie. Pin źródłowy jest domyślny.",
        "WYPEŁNIJ 1:1", "WYCZYŚĆ", "POKAŻ GRAF", "ZAPISZ", "ZAPISZ + TEST", "WSTECZ", "ZAPISANO", "NIEZAPISANO"};

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

lv_obj_t* makeButton(lv_obj_t* parent,
                     lv_coord_t x,
                     lv_coord_t y,
                     lv_coord_t width,
                     lv_coord_t height,
                     lv_color_t color,
                     const char* text) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(0x557183), LV_PART_MAIN);
    lv_obj_set_style_radius(button, 7, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_t* label = makeLabel(button, text, p4Font14(), lv_color_hex(0xFFFFFF));
    lv_obj_center(label);
    return button;
}

}  // namespace

void MapEditorScreen::begin(const CableMap& source, const CableProfile& profile) {
    begin(source, profile, MapEditorContext::Normal);
}

void MapEditorScreen::begin(const CableMap& source, const CableProfile& profile,
                            MapEditorContext context) {
    working_ = source;
    profile_ = profile;
    context_ = context;
    dirty_ = false;
    editUnlocked_ = context_ != MapEditorContext::DocumentReview;
    pendingAction_ = MapEditorAction::None;
    mode_ = MapEditMode::AtoB;
    rebuildVisiblePoints();
    sourceIndex_ = isVisiblePoint(1U)
                       ? 1U
                       : (visibleCount_ > 0U ? visibleIndices_[0] : 0U);
    build();
    refresh();
}

void MapEditorScreen::activate() {
    pendingAction_ = MapEditorAction::None;
    if (screen_ != nullptr && lv_scr_act() != screen_) {
        lv_scr_load(screen_);
    }
    refresh();
}

void MapEditorScreen::reloadFrom(const CableMap& source) {
    if (dirty_) {
        return;
    }
    working_ = source;
    refresh();
}

void MapEditorScreen::markSaved() {
    dirty_ = false;
    refresh();
}

void MapEditorScreen::focusPoint(MapEditMode mode, uint8_t sourceIndex) {
    mode_ = mode;
    if (isVisiblePoint(sourceIndex) && pointEnabledForSide(sourceSide(), sourceIndex)) {
        sourceIndex_ = sourceIndex;
    } else {
        const uint8_t replacement = firstEnabledVisiblePoint(sourceSide());
        if (replacement != 0xFFU) sourceIndex_ = replacement;
    }
    refresh();
}

void MapEditorScreen::build() {
    if (screen_ == nullptr) {
        screen_ = lv_obj_create(nullptr);
    } else {
        lv_obj_clean(screen_);
    }
    lv_scr_load(screen_);
    flat(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x071018), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

    const EditorText& t = editorText();
    const bool documentReview = context_ == MapEditorContext::DocumentReview;
    const bool turkish = currentP4Language() == P4Language::Turkish;
    lv_obj_t* header = lv_obj_create(screen_);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, kDisplayWidth, 72);
    flat(header);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x142B3A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);

    const char* titleText = documentReview
        ? (turkish ? "BELGE HARİTA KONTROLÜ" : "DOCUMENT MAP REVIEW")
        : t.title;
    lv_obj_t* title = makeLabel(header, titleText, p4Font20(), lv_color_hex(0xEAF6FF));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);
    char subtitleText[144] = {};
    snprintf(subtitleText,
             sizeof(subtitleText),
             "%u PIN | A:PE %s dS %s | B:PE %s dS %s | %s",
             static_cast<unsigned>(profile_.signalPinCount),
             profile_.includePeA ? "ON" : "OFF",
             profile_.includeDrainShieldA ? "ON" : "OFF",
             profile_.includePeB ? "ON" : "OFF",
             profile_.includeDrainShieldB ? "ON" : "OFF",
             documentReview
                 ? (turkish ? "START son profil onayına kadar kilitli"
                            : "START stays locked until final profile approval")
                 : t.subtitle);
    subtitleLabel_ = makeLabel(header, subtitleText, p4Font14(), lv_color_hex(0x9CC6DC));
    lv_obj_align(subtitleLabel_, LV_ALIGN_BOTTOM_MID, 0, -7);
    dirtyLabel_ = makeLabel(
        header,
        (documentReview && !editUnlocked_)
            ? p4SelectText("KİLİTLİ", "LOCKED", "VERGRENDELD", "GESPERRT", "VERROUILLÉ", "BLOQUEADO", "ZABLOKOWANE")
            : t.saved,
        p4Font14(),
        (documentReview && !editUnlocked_) ? lv_color_hex(0x6EDBFF) : lv_color_hex(0x75E39A));
    lv_obj_align(dirtyLabel_, LV_ALIGN_LEFT_MID, 16, 0);
    soundButton_.create(header, 970, 15, 42);

    sourcePaneTitle_ = makeLabel(screen_, "A - KAYNAK", p4Font14(), lv_color_hex(0x6EDBFF));
    lv_obj_set_width(sourcePaneTitle_, 350);
    lv_obj_set_style_text_align(sourcePaneTitle_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(sourcePaneTitle_, 16, 82);

    targetPaneTitle_ = makeLabel(screen_, "B - HEDEF", p4Font14(), lv_color_hex(0x8EF0A7));
    lv_obj_set_width(targetPaneTitle_, 350);
    lv_obj_set_style_text_align(targetPaneTitle_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(targetPaneTitle_, 658, 82);

    constexpr lv_coord_t kLeftX = 16;
    constexpr lv_coord_t kRightX = 658;
    constexpr lv_coord_t kGridY = 108;
    constexpr lv_coord_t kPaneWidth = 350;
    constexpr lv_coord_t kGridHeight = 392;
    constexpr lv_coord_t kGap = 4;

    memset(sourceButtons_, 0, sizeof(sourceButtons_));
    memset(targetButtons_, 0, sizeof(targetButtons_));

    // The 7-inch panel is used adaptively: common 9/15/25-pin harnesses get
    // larger touch targets, while 52/62-pin maps keep the exact same layout
    // concept with smaller cells. Only points active in the NORMAL profile
    // are created; unused 26..62 points do not clutter a 9/15/25-pin map.
    const uint8_t columns = visibleCount_ <= 12U ? 4U
                           : visibleCount_ <= 30U ? 6U
                                                  : 8U;
    const uint8_t rows = visibleCount_ == 0U
                             ? 1U
                             : static_cast<uint8_t>((visibleCount_ + columns - 1U) / columns);
    const lv_coord_t cellW = static_cast<lv_coord_t>(
        (kPaneWidth - static_cast<lv_coord_t>(columns - 1U) * kGap) / columns);
    const lv_coord_t cellHFromHeight = static_cast<lv_coord_t>(
        (kGridHeight - static_cast<lv_coord_t>(rows - 1U) * kGap) / rows);
    const lv_coord_t cellH = cellHFromHeight < cellW ? cellHFromHeight : cellW;

    for (uint8_t visibleSlot = 0; visibleSlot < visibleCount_; ++visibleSlot) {
        const uint8_t index = visibleIndices_[visibleSlot];
        const uint8_t row = visibleSlot / columns;
        const uint8_t col = visibleSlot % columns;
        const lv_coord_t xLeft = static_cast<lv_coord_t>(
            kLeftX + col * (cellW + kGap));
        const lv_coord_t xRight = static_cast<lv_coord_t>(
            kRightX + col * (cellW + kGap));
        const lv_coord_t y = static_cast<lv_coord_t>(
            kGridY + row * (cellH + kGap));
        char pin[8]{};
        formatPin(index, pin, sizeof(pin));

        sourceButtons_[index] = makeButton(screen_, xLeft, y, cellW, cellH,
                                           lv_color_hex(0x1B3544), pin);
        sourceBindings_[index] = {this, false, index};
        lv_obj_add_event_cb(sourceButtons_[index], pinCallback, LV_EVENT_CLICKED,
                            &sourceBindings_[index]);

        targetButtons_[index] = makeButton(screen_, xRight, y, cellW, cellH,
                                           lv_color_hex(0x1B3544), pin);
        targetBindings_[index] = {this, true, index};
        lv_obj_add_event_cb(targetButtons_[index], pinCallback, LV_EVENT_CLICKED,
                            &targetBindings_[index]);
    }

    lv_obj_t* center = lv_obj_create(screen_);
    lv_obj_set_pos(center, 384, 92);
    lv_obj_set_size(center, 256, 368);
    flat(center);
    lv_obj_set_style_bg_color(center, lv_color_hex(0x101F29), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(center, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(center, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(center, lv_color_hex(0x365565), LV_PART_MAIN);
    lv_obj_set_style_radius(center, 10, LV_PART_MAIN);

    const char* modeNames[] = {t.cross, t.sameA, t.sameB};
    const MapEditMode modes[] = {MapEditMode::AtoB, MapEditMode::SameA, MapEditMode::SameB};
    for (uint8_t i = 0; i < 3; ++i) {
        modeButtons_[i] = makeButton(center, 16, 16 + i * 50, 224, 42,
                                     lv_color_hex(0x24485A), modeNames[i]);
        modeBindings_[i] = {this, modes[i]};
        lv_obj_add_event_cb(modeButtons_[i], modeCallback, LV_EVENT_CLICKED,
                            &modeBindings_[i]);
    }

    selectedLabel_ = makeLabel(center, "", p4Font20(), lv_color_hex(0xFFFFFF));
    lv_obj_set_pos(selectedLabel_, 16, 178);
    lv_obj_set_width(selectedLabel_, 224);
    lv_obj_set_style_text_align(selectedLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    targetsLabel_ = makeLabel(center, "", p4Font14(), lv_color_hex(0xAEE8C0));
    lv_obj_set_pos(targetsLabel_, 12, 220);
    lv_obj_set_width(targetsLabel_, 232);
    lv_obj_set_style_text_align(targetsLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(targetsLabel_, LV_LABEL_LONG_WRAP);

    instructionLabel_ = makeLabel(center, "", p4Font14(), lv_color_hex(0xAABBC5));
    lv_obj_set_pos(instructionLabel_, 16, 282);
    lv_obj_set_width(instructionLabel_, 224);
    lv_obj_set_style_text_align(instructionLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(instructionLabel_, LV_LABEL_LONG_WRAP);

    if (documentReview && !editUnlocked_) {
        const char* names[] = {
            p4SelectText("ANALİZE DÖN", "BACK TO PROFILE", "TERUG NAAR PROFIEL", "ZURÜCK ZUM PROFIL",
                         "RETOUR AU PROFIL", "VOLVER AL PERFIL", "WRÓĆ DO PROFILU"),
            p4SelectText("DÜZENLE", "EDIT", "BEWERKEN", "BEARBEITEN", "MODIFIER", "EDITAR", "EDYTUJ"),
            p4SelectText("GRAFİKLE GÖSTER", "SHOW GRAPH", "TOON GRAFIEK", "GRAFIK ZEIGEN",
                         "VOIR GRAPHE", "VER GRÁFICO", "POKAŻ GRAF")};
        const LocalAction actions[] = {LocalAction::Back, LocalAction::UnlockEdit, LocalAction::Graph};
        const lv_coord_t xs[] = {24, 304, 584};
        const lv_coord_t widths[] = {260, 260, 416};
        const lv_color_t colors[] = {lv_color_hex(0x5A496F), lv_color_hex(0xB07024), lv_color_hex(0x355F78)};
        for (uint8_t i = 0; i < 3; ++i) {
            lv_obj_t* button = makeButton(screen_, xs[i], 516, widths[i], 64, colors[i], names[i]);
            actionBindings_[i] = {this, actions[i]};
            lv_obj_add_event_cb(button, actionCallback, LV_EVENT_CLICKED, &actionBindings_[i]);
        }
    } else {
        const char* actionNamesNormal[] = {t.back, t.oneToOne, t.clear, t.graph, t.save, t.test};
        const char* actionNamesDocument[] = {
            p4SelectText("ANALİZE DÖN", "BACK TO PROFILE", "TERUG NAAR PROFIEL", "ZURÜCK ZUM PROFIL",
                         "RETOUR AU PROFIL", "VOLVER AL PERFIL", "WRÓĆ DO PROFILU"),
            t.oneToOne, t.clear, t.graph,
            p4SelectText("DOĞRULA", "VALIDATE", "VALIDEREN", "PRÜFEN", "VALIDER", "VALIDAR", "SPRAWDŹ"),
            p4SelectText("DOĞRULA + DÖN", "VALIDATE + RETURN", "VALIDEREN + TERUG", "PRÜFEN + ZURÜCK",
                         "VALIDER + RETOUR", "VALIDAR + VOLVER", "SPRAWDŹ + WRÓĆ")};
        const char** actionNames = documentReview ? actionNamesDocument : actionNamesNormal;
        const LocalAction actions[] = {LocalAction::Back, LocalAction::OneToOne,
                                       LocalAction::Clear, LocalAction::Graph,
                                       LocalAction::Save, LocalAction::Test};
        const lv_coord_t xs[] = {16, 152, 288, 424, 600, 736};
        const lv_coord_t widths[] = {130, 130, 130, 170, 130, 272};
        const lv_color_t colors[] = {lv_color_hex(0x5A496F), lv_color_hex(0x26607B),
                                     lv_color_hex(0x7A3D3D), lv_color_hex(0x355F78),
                                     lv_color_hex(0x267047), lv_color_hex(0x1B7F9E)};
        for (uint8_t i = 0; i < 6; ++i) {
            lv_obj_t* button = makeButton(screen_, xs[i], 516, widths[i], 64,
                                          colors[i], actionNames[i]);
            actionBindings_[i] = {this, actions[i]};
            lv_obj_add_event_cb(button, actionCallback, LV_EVENT_CLICKED,
                                &actionBindings_[i]);
        }
    }
}

void MapEditorScreen::formatPin(uint8_t index, char* output, size_t outputSize) const {
    if (output == nullptr || outputSize == 0U) {
        return;
    }
    if (index == kPeTestIndex) {
        snprintf(output, outputSize, "PE");
    } else if (index == kDrainShieldTestIndex) {
        snprintf(output, outputSize, "dS");
    } else {
        snprintf(output, outputSize, "%u", index);
    }
}

char MapEditorScreen::sourceSide() const {
    return mode_ == MapEditMode::SameB ? 'B' : 'A';
}

char MapEditorScreen::targetSide() const {
    return mode_ == MapEditMode::AtoB ? 'B' : sourceSide();
}

uint64_t MapEditorScreen::currentTargetMask() const {
    uint64_t mask = 0ULL;
    switch (mode_) {
        case MapEditMode::SameA:
            mask = working_.sameSideMaskA(sourceIndex_);
            break;
        case MapEditMode::SameB:
            mask = working_.sameSideMaskB(sourceIndex_);
            break;
        case MapEditMode::AtoB:
        default:
            mask = working_.receiverMaskAtoB(sourceIndex_);
            break;
    }
    const uint64_t sideMask = profile_.sidePointMask(targetSide() == 'A');
    return mask & visibleMask_ & sideMask;
}

void MapEditorScreen::rebuildVisiblePoints() {
    visibleCount_ = 0U;
    visibleMask_ = 0ULL;
    const uint8_t count = profile_.activePointCount();
    for (uint8_t slot = 0; slot < count && visibleCount_ < kTestPointsPerSide; ++slot) {
        const uint8_t index = profile_.slotToTestIndex(slot);
        if (index >= kTestPointsPerSide) {
            continue;
        }
        visibleIndices_[visibleCount_++] = index;
        visibleMask_ |= (1ULL << index);
    }
}

bool MapEditorScreen::isVisiblePoint(uint8_t index) const {
    return index < kTestPointsPerSide &&
           (visibleMask_ & (1ULL << index)) != 0ULL;
}

bool MapEditorScreen::pointEnabledForSide(char side, uint8_t index) const {
    return profile_.pointEnabledOnSide(side == 'A', index);
}

uint8_t MapEditorScreen::firstEnabledVisiblePoint(char side) const {
    for (uint8_t slot = 0; slot < visibleCount_; ++slot) {
        const uint8_t index = visibleIndices_[slot];
        if (pointEnabledForSide(side, index)) {
            return index;
        }
    }
    return 0xFFU;
}

void MapEditorScreen::formatTargets(char* output, size_t outputSize) const {
    if (output == nullptr || outputSize == 0U) {
        return;
    }
    output[0] = '\0';
    const uint64_t mask = currentTargetMask();
    uint8_t shown = 0U;
    uint8_t total = 0U;
    for (uint8_t visibleSlot = 0; visibleSlot < visibleCount_; ++visibleSlot) {
        const uint8_t i = visibleIndices_[visibleSlot];
        if ((mask & (1ULL << i)) == 0ULL) {
            continue;
        }
        ++total;
        if (shown >= 8U) {
            continue;
        }
        char pin[8]{};
        formatPin(i, pin, sizeof(pin));
        char item[14]{};
        snprintf(item, sizeof(item), "%c%s", targetSide(), pin);
        if (shown > 0U) {
            strncat(output, ", ", outputSize - strlen(output) - 1U);
        }
        strncat(output, item, outputSize - strlen(output) - 1U);
        ++shown;
    }
    if (total == 0U) {
        snprintf(output, outputSize, "-");
    } else if (total > shown) {
        char more[16]{};
        snprintf(more, sizeof(more), " +%u", static_cast<unsigned>(total - shown));
        strncat(output, more, outputSize - strlen(output) - 1U);
    }
}

void MapEditorScreen::refresh() {
    if (screen_ == nullptr) {
        return;
    }
    const EditorText& t = editorText();
    char pane[40]{};
    snprintf(pane, sizeof(pane), "%c - %s", sourceSide(), t.source);
    lv_label_set_text(sourcePaneTitle_, pane);
    snprintf(pane, sizeof(pane), "%c - %s", targetSide(), t.target);
    lv_label_set_text(targetPaneTitle_, pane);

    char pin[8]{};
    formatPin(sourceIndex_, pin, sizeof(pin));
    char selected[72]{};
    snprintf(selected, sizeof(selected), "%s: %c%s", t.selected, sourceSide(), pin);
    lv_label_set_text(selectedLabel_, selected);

    char targets[190]{};
    char list[150]{};
    formatTargets(list, sizeof(list));
    snprintf(targets, sizeof(targets), "%s:\n%s", t.targets, list);
    lv_label_set_text(targetsLabel_, targets);
    const bool lockedReview = context_ == MapEditorContext::DocumentReview && !editUnlocked_;
    lv_label_set_text(
        instructionLabel_,
        lockedReview
            ? p4SelectText("Haritayı inceleyin. Değişiklik için DÜZENLE'ye basın.",
                           "Review the map. Press EDIT to make changes.",
                           "Controleer de kaart. Druk op BEWERKEN om te wijzigen.",
                           "Karte prüfen. Zum Ändern BEARBEITEN drücken.",
                           "Vérifiez la carte. Appuyez sur MODIFIER pour changer.",
                           "Revise el mapa. Pulse EDITAR para cambiarlo.",
                           "Sprawdź mapę. Naciśnij EDYTUJ, aby wprowadzić zmiany.")
            : (mode_ == MapEditMode::AtoB ? t.instructionCross : t.instructionSame));
    lv_label_set_text(
        dirtyLabel_,
        lockedReview
            ? p4SelectText("KİLİTLİ", "LOCKED", "VERGRENDELD", "GESPERRT", "VERROUILLÉ", "BLOQUEADO", "ZABLOKOWANE")
            : (dirty_ ? t.unsaved : t.saved));
    lv_obj_set_style_text_color(
        dirtyLabel_,
        lockedReview ? lv_color_hex(0x6EDBFF)
                     : (dirty_ ? lv_color_hex(0xFFD06B) : lv_color_hex(0x75E39A)),
        LV_PART_MAIN);

    for (uint8_t i = 0; i < 3; ++i) {
        const bool active = (i == 0U && mode_ == MapEditMode::AtoB) ||
                            (i == 1U && mode_ == MapEditMode::SameA) ||
                            (i == 2U && mode_ == MapEditMode::SameB);
        lv_obj_set_style_bg_color(modeButtons_[i],
                                  active ? lv_color_hex(0x1683A6) : lv_color_hex(0x24485A),
                                  LV_PART_MAIN);
        lv_obj_set_style_border_color(modeButtons_[i],
                                      active ? lv_color_hex(0x8DE8FF) : lv_color_hex(0x557183),
                                      LV_PART_MAIN);
    }

    const uint64_t targetMask = currentTargetMask();
    for (uint8_t visibleSlot = 0; visibleSlot < visibleCount_; ++visibleSlot) {
        const uint8_t i = visibleIndices_[visibleSlot];
        const bool sourceEnabled = pointEnabledForSide(sourceSide(), i);
        const bool targetEnabled = pointEnabledForSide(targetSide(), i);
        const bool sourceSelected = sourceEnabled && i == sourceIndex_;
        if (sourceButtons_[i] != nullptr) {
            if (sourceEnabled) {
                lv_obj_clear_state(sourceButtons_[i], LV_STATE_DISABLED);
            } else {
                lv_obj_add_state(sourceButtons_[i], LV_STATE_DISABLED);
            }
            lv_obj_set_style_bg_color(sourceButtons_[i],
                                      !sourceEnabled ? lv_color_hex(0x182129) :
                                      sourceSelected ? lv_color_hex(0x1683A6) : lv_color_hex(0x1B3544),
                                      LV_PART_MAIN);
            lv_obj_set_style_border_color(sourceButtons_[i],
                                          !sourceEnabled ? lv_color_hex(0x34434C) :
                                          sourceSelected ? lv_color_hex(0x9BEFFF) : lv_color_hex(0x557183),
                                          LV_PART_MAIN);
        }

        const bool connected = targetEnabled && (targetMask & (1ULL << i)) != 0ULL;
        const bool implicitSelf = targetEnabled && mode_ != MapEditMode::AtoB && i == sourceIndex_;
        lv_color_t targetColor = targetEnabled ? lv_color_hex(0x1B3544) : lv_color_hex(0x182129);
        if (connected) {
            targetColor = lv_color_hex(0x267047);
        } else if (implicitSelf) {
            targetColor = lv_color_hex(0x665A28);
        }
        if (targetButtons_[i] != nullptr) {
            if (targetEnabled) {
                lv_obj_clear_state(targetButtons_[i], LV_STATE_DISABLED);
            } else {
                lv_obj_add_state(targetButtons_[i], LV_STATE_DISABLED);
            }
            if (lockedReview) lv_obj_clear_flag(targetButtons_[i], LV_OBJ_FLAG_CLICKABLE);
            else lv_obj_add_flag(targetButtons_[i], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(targetButtons_[i], targetColor, LV_PART_MAIN);
            lv_obj_set_style_border_color(targetButtons_[i],
                                          !targetEnabled ? lv_color_hex(0x34434C) :
                                          connected ? lv_color_hex(0x8EF0A7) :
                                          implicitSelf ? lv_color_hex(0xE3CD70) : lv_color_hex(0x557183),
                                          LV_PART_MAIN);
        }
    }
}

void MapEditorScreen::selectSource(uint8_t index) {
    if (!isVisiblePoint(index) || !pointEnabledForSide(sourceSide(), index)) {
        return;
    }
    sourceIndex_ = index;
    refresh();
}

void MapEditorScreen::toggleTarget(uint8_t index) {
    if (context_ == MapEditorContext::DocumentReview && !editUnlocked_) {
        return;
    }
    if (!isVisiblePoint(index) || !pointEnabledForSide(targetSide(), index)) {
        return;
    }
    if (mode_ == MapEditMode::AtoB) {
        const bool connected = (working_.receiverMaskAtoB(sourceIndex_) & (1ULL << index)) != 0ULL;
        working_.connectAtoB(sourceIndex_, index, !connected);
    } else {
        if (index == sourceIndex_) {
            return;  // source self-membership is implicit
        }
        const CableMapSide side = mode_ == MapEditMode::SameA ? CableMapSide::A : CableMapSide::B;
        const uint64_t mask = side == CableMapSide::A
                                  ? working_.sameSideMaskA(sourceIndex_)
                                  : working_.sameSideMaskB(sourceIndex_);
        const bool connected = (mask & (1ULL << index)) != 0ULL;
        working_.setSameSideConnection(side, sourceIndex_, index, !connected);
    }
    dirty_ = true;
    refresh();
}

void MapEditorScreen::setMode(MapEditMode mode) {
    mode_ = mode;
    if (!pointEnabledForSide(sourceSide(), sourceIndex_)) {
        const uint8_t replacement = firstEnabledVisiblePoint(sourceSide());
        if (replacement != 0xFFU) {
            sourceIndex_ = replacement;
        }
    }
    refresh();
}

void MapEditorScreen::handleAction(LocalAction action) {
    switch (action) {
        case LocalAction::OneToOne:
            if (context_ == MapEditorContext::DocumentReview && !editUnlocked_) break;
            if (context_ == MapEditorContext::DocumentReview) {
                // Fill only points that are actually enabled by the imported
                // profile. A raw 64-point 1:1 map would reference disabled
                // pins and must be rejected by the validation contract.
                working_.clear();
                for (uint8_t i = 0U; i < kTestPointsPerSide; ++i) {
                    if (pointEnabledForSide('A', i) && pointEnabledForSide('B', i)) {
                        working_.connectAtoB(i, i, true);
                    }
                }
            } else {
                working_.setOneToOne();
                working_.setName("CUSTOM MAP");
            }
            dirty_ = true;
            refresh();
            break;
        case LocalAction::Clear:
            if (context_ == MapEditorContext::DocumentReview && !editUnlocked_) break;
            working_.clear();
            working_.setName("CUSTOM MAP");
            dirty_ = true;
            refresh();
            break;
        case LocalAction::Graph:
            pendingAction_ = MapEditorAction::Graph;
            break;
        case LocalAction::Save:
            if (context_ != MapEditorContext::DocumentReview || editUnlocked_) {
                pendingAction_ = MapEditorAction::Save;
            }
            break;
        case LocalAction::Test:
            if (context_ != MapEditorContext::DocumentReview || editUnlocked_) {
                pendingAction_ = context_ == MapEditorContext::DocumentReview
                                     ? MapEditorAction::DocumentDone
                                     : MapEditorAction::Test;
            }
            break;
        case LocalAction::Back:
            pendingAction_ = MapEditorAction::Back;
            break;
        case LocalAction::UnlockEdit:
            if (context_ == MapEditorContext::DocumentReview && !editUnlocked_) {
                editUnlocked_ = true;
                build();
                refresh();
            }
            break;
    }
}

void MapEditorScreen::pinCallback(lv_event_t* event) {
    auto* binding = static_cast<PinBinding*>(lv_event_get_user_data(event));
    if (binding == nullptr || binding->owner == nullptr) {
        return;
    }
    if (binding->targetPane) {
        binding->owner->toggleTarget(binding->index);
    } else {
        binding->owner->selectSource(binding->index);
    }
}

void MapEditorScreen::modeCallback(lv_event_t* event) {
    auto* binding = static_cast<ModeBinding*>(lv_event_get_user_data(event));
    if (binding != nullptr && binding->owner != nullptr) {
        binding->owner->setMode(binding->mode);
    }
}

void MapEditorScreen::actionCallback(lv_event_t* event) {
    auto* binding = static_cast<ActionBinding*>(lv_event_get_user_data(event));
    if (binding != nullptr && binding->owner != nullptr) {
        binding->owner->handleAction(binding->action);
    }
}

MapEditorAction MapEditorScreen::consumeAction() {
    const MapEditorAction action = pendingAction_;
    pendingAction_ = MapEditorAction::None;
    return action;
}

}  // namespace mg::p4
