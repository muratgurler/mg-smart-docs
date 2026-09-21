#pragma once

#include <Arduino.h>
#include <FS.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace mg::p4 {

class MapWebEditor;
class DocumentImportValidator;

class MgBleProfileServer {
public:
    static constexpr size_t kMaxProfileBytes = 256U * 1024U;
    static constexpr uint16_t kMaxDataPayload = 180U;

    bool begin(MapWebEditor& portal, DocumentImportValidator& validator);
    void tick();

    bool openSession();
    void closeSession();

    bool available() const { return initialized_; }
    bool sessionOpen() const { return sessionOpen_; }
    bool connected() const { return connected_; }
    bool transferActive() const { return transferActive_; }
    bool profileReady() const { return profileReady_; }
    String deviceName() const { return deviceName_; }
    String sessionId() const { return sessionId_; }
    String stateText() const { return stateText_; }
    size_t receivedBytes() const { return receivedBytes_; }
    size_t expectedBytes() const { return expectedBytes_; }

    // Fast BLE-callback entry points. They only enqueue/copy data; flash I/O,
    // JSON parsing and electrical validation run later from tick().
    void enqueueControlWrite(const uint8_t* data, size_t length);
    void enqueueDataWrite(const uint8_t* data, size_t length);
    void noteConnectedFromCallback();
    void noteDisconnectedFromCallback();

private:
    enum class EventType : uint8_t { Control, Data };
    struct Event {
        EventType type = EventType::Control;
        uint16_t length = 0U;
        uint8_t data[196] = {};
    };

    bool initGatt();
    void processControl(const uint8_t* data, size_t length);
    void processData(const uint8_t* data, size_t length);
    void startTransfer(const String& command);
    void finishTransfer(const String& command);
    void abortTransfer(bool deletePartial = true);
    void fail(const char* code, const String& detail, bool abortActive = true);
    void notifyStatus(const String& status);
    void refreshSessionCharacteristic();
    void restartAdvertisingIfNeeded();
    static uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t length);
    static bool parseHex32(const String& value, uint32_t& output);

    MapWebEditor* portal_ = nullptr;
    DocumentImportValidator* validator_ = nullptr;
    bool initialized_ = false;
    bool sessionOpen_ = false;
    volatile bool connected_ = false;
    volatile bool disconnectPending_ = false;
    volatile bool queueOverflow_ = false;
    bool transferActive_ = false;
    bool profileReady_ = false;
    volatile bool advertising_ = false;

    String deviceName_;
    String sessionId_;
    String stateText_ = "BLE unavailable";
    String tempPath_;
    File transferFile_;
    size_t expectedBytes_ = 0U;
    size_t receivedBytes_ = 0U;
    uint32_t expectedCrc32_ = 0U;
    uint32_t runningCrc32_ = 0xFFFFFFFFUL;
    uint16_t expectedParts_ = 0U;
    uint16_t nextPart_ = 0U;

    QueueHandle_t eventQueue_ = nullptr;

    // Kept opaque in the header to avoid exposing Arduino BLE implementation
    // details throughout the firmware.
    void* server_ = nullptr;
    void* service_ = nullptr;
    void* deviceInfoCharacteristic_ = nullptr;
    void* sessionCharacteristic_ = nullptr;
    void* controlCharacteristic_ = nullptr;
    void* dataCharacteristic_ = nullptr;
    void* statusCharacteristic_ = nullptr;
    void* advertisingObject_ = nullptr;
};

}  // namespace mg::p4
