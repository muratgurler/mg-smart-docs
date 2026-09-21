#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <FS.h>

#include "CableMap.h"
#include "CableMapStore.h"
#include "MgNetworkManager.h"

namespace mg::p4 {

enum class DocumentFileKind : uint8_t {
    Json,
    Unknown,
};

struct DocumentItemInfo {
    String name;
    String path;
    DocumentFileKind kind = DocumentFileKind::Unknown;
    size_t size = 0U;
    uint32_t crc32 = 0U;
};

class MapWebEditor {
public:
    MapWebEditor();

    void begin(CableMap& map, CableMapStore& store, MgNetworkManager& network);
    void tick();
    void setEditingEnabled(bool enabled) { editingEnabled_ = enabled; }

    bool available() const;
    String url() const;
    bool consumeChanged();

    bool startDocumentPortal();
    bool resumeDocumentPortal();
    void stopDocumentPortal();
    bool documentPortalActive() const { return documentPortalActive_; }
    String documentPortalUrl() const;
    uint8_t documentUploadCount() const { return documentItemCount_; }
    String lastDocumentName() const { return lastDocumentName_; }
    size_t lastDocumentSize() const { return lastDocumentSize_; }
    String lastDocumentError() const { return lastDocumentError_; }
    bool documentUploadBusy() const { return documentChunkTransferActive_; }
    bool documentPortalAccepted() const { return documentPortalAccepted_; }

    uint8_t documentItemCount() const { return documentItemCount_; }
    bool documentItem(uint8_t index, DocumentItemInfo& output) const;
    size_t documentTotalBytes() const;
    bool removeDocumentItem(uint8_t index);
    bool moveDocumentItem(uint8_t index, int8_t delta);
    bool clearDocumentItems();
    bool finalizeDocumentManifest();

    // Transport-neutral seam used by BLE.  The BLE receiver writes a temporary
    // file, then hands it to MapWebEditor so Wi-Fi and BLE share one committed
    // profile store, manifest format and review/validation pipeline.
    bool prepareExternalProfileStorage(String& tempPath, String& reason);
    bool commitExternalPreparedProfile(const String& tempPath,
                                       const String& displayName,
                                       size_t size,
                                       uint32_t crc32,
                                       String& reason);
    void abortExternalPreparedProfile(const String& tempPath);
    void noteDocumentTransportError(const String& error);

    // Self-heal the in-RAM profile list from a committed /mgdocs/profile_NNN.json file.
    // Recover a prepared digital profile after an interrupted UI flow.
    bool recoverDocumentItemsFromStorage();
    String documentManifestPath() const;
    bool documentManifestExists() const;

private:
    void handleRoot();
    void handleGraph();
    void handleSave();
    void handleOneToOne();
    void handleClear();
    void redirectToMap();
    void redirectToDocumentPortal();
    void handleDocumentPortal();
    void handleProfileApiSession();
    void handleDocumentUploadComplete();
    void handleDocumentUploadChunk();
    void handleDocumentChunkUploadComplete();
    void handleDocumentChunkUploadChunk();
    void resetDocumentChunkTransfer(bool removePartial);
    void handleDocumentDelete();
    void handleCaptiveProbe();
    bool validDocumentSession() const;
    bool ensureDocumentStorage();
    bool writeDocumentManifest();
    void resetDocumentItems(bool deleteFiles);
    static String safeExtension(const String& filename);
    static String safeDocumentName(const String& filename);
    static DocumentFileKind documentKindFromExtension(const String& extension);
    static const char* documentKindText(DocumentFileKind kind);
    static uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t length);

    static int parsePinToken(String token);
    static uint64_t parseMask(const String& value);
    static String formatPin(uint8_t index);
    static String formatMask(uint64_t mask, char prefix);

    WebServer server_;
    CableMap* map_ = nullptr;
    CableMapStore* store_ = nullptr;
    MgNetworkManager* network_ = nullptr;
    bool started_ = false;
    bool changed_ = false;
    bool editingEnabled_ = true;

    static constexpr uint8_t kMaxDocumentItems = 1U;
    bool documentPortalActive_ = false;
    bool documentPortalAccepted_ = false;
    bool documentFsReady_ = false;
    File documentUploadFile_;
    String documentUploadPath_;
    String pendingDocumentName_;
    DocumentFileKind pendingDocumentKind_ = DocumentFileKind::Unknown;
    size_t pendingDocumentSize_ = 0U;
    uint32_t pendingDocumentCrc32_ = 0xFFFFFFFFUL;
    uint16_t documentNextFileId_ = 1U;
    uint8_t documentItemCount_ = 0U;
    DocumentItemInfo documentItems_[kMaxDocumentItems];
    String lastDocumentName_;
    size_t lastDocumentSize_ = 0U;
    String lastDocumentError_;

    // Phone uploads use many short multipart requests instead of one long
    // multi-megabyte request. This keeps the synchronous Arduino WebServer
    // from monopolising loop() long enough to starve LVGL/DNS/Wi-Fi service.
    bool documentChunkTransferActive_ = false;
    bool documentChunkRequestAccepted_ = false;
    bool documentChunkRequestDuplicate_ = false;
    bool documentChunkFinalizedThisRequest_ = false;
    String documentChunkTransferId_;
    String documentLastCompletedTransferId_;
    uint16_t documentLastCompletedParts_ = 0U;
    uint16_t documentChunkExpectedPart_ = 0U;
    uint16_t documentChunkTotalParts_ = 0U;
    size_t documentChunkExpectedTotalBytes_ = 0U;
};

}  // namespace mg::p4
