#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "CableMap.h"
#include "CableProfile.h"
#include "DocumentImportValidator.h"
#include "ProductionProfileContract.h"

namespace mg::p4 {

class MapWebEditor;

enum class DocumentAnalysisAction : uint8_t {
    None,
    ReturnToReview,
    BackToExtra,
    EditDraftMap,
    ApplyProfile,
};

class DocumentAnalysisScreen {
public:
    void begin(MapWebEditor& portal, DocumentImportValidator& validator);
    void activate();
    DocumentAnalysisAction consumeAction();

    bool hasDraftProfile() const { return profileReady_; }
    bool hasEditableDraft() const { return profileReady_; }
    bool acceptOperatorEditedMap(const CableMap& map);
    bool restoreOriginalMap();
    bool operatorEdited() const { return operatorEdited_; }
    bool operatorEditConfirmed() const { return operatorEditConfirmed_; }
    const CableMap& draftMap() const { return draftMap_; }
    const CableProfile& draftProfile() const { return draftProfile_; }
    const ProductionProfileRecord& profileRecord() const { return record_; }

    static void actionCallback(lv_event_t* event);

private:
    enum class UiAction : uint8_t {
        ImportProfile,
        EditDraftMap,
        ReturnToReview,
        BackToExtra,
        ApplyProfile,
        RestoreOriginal,
    };

    struct ActionContext {
        DocumentAnalysisScreen* self = nullptr;
        UiAction action = UiAction::ReturnToReview;
    };

    void buildUi();
    void refresh();
    void importProfile();
    void handleUiAction(UiAction action);

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* statusLabel_ = nullptr;
    lv_obj_t* detailLabel_ = nullptr;
    lv_obj_t* importButton_ = nullptr;
    lv_obj_t* editButton_ = nullptr;
    lv_obj_t* restoreButton_ = nullptr;
    lv_obj_t* applyButton_ = nullptr;

    MapWebEditor* portal_ = nullptr;
    DocumentImportValidator* validator_ = nullptr;
    DocumentValidationReport validationReport_;
    ProductionProfileRecord record_{};
    ProductionProfileRecord originalRecord_{};
    CableMap draftMap_;
    CableMap originalMap_;
    CableProfile draftProfile_ = makeNormalProfile();
    bool importAttempted_ = false;
    bool profileReady_ = false;
    bool operatorEdited_ = false;
    bool operatorEditConfirmed_ = false;
    String message_;
    String editValidationError_;
    DocumentAnalysisAction action_ = DocumentAnalysisAction::None;
    ActionContext actionContexts_[6];
};

}  // namespace mg::p4
