#include "MgBleProfileServer.h"

#include <LittleFS.h>
#include <BLEAdvertising.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEService.h>
#include <esp_system.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <new>

#include "CableMap.h"
#include "CableProfile.h"
#include "DocumentImportValidator.h"
#include "MapWebEditor.h"
#include "PreparedProfileService.h"
#include "ProductionProfileContract.h"

namespace mg::p4 {
namespace {

constexpr const char* kServiceUuid = "6d675000-7465-7374-6572-000000000001";
constexpr const char* kDeviceInfoUuid = "6d675000-7465-7374-6572-000000000002";
constexpr const char* kSessionUuid = "6d675000-7465-7374-6572-000000000003";
constexpr const char* kControlUuid = "6d675000-7465-7374-6572-000000000004";
constexpr const char* kDataUuid = "6d675000-7465-7374-6572-000000000005";
constexpr const char* kStatusUuid = "6d675000-7465-7374-6572-000000000006";

class ControlCallbacks final : public BLECharacteristicCallbacks {
public:
    explicit ControlCallbacks(MgBleProfileServer& owner) : owner_(owner) {}

    void onWrite(BLECharacteristic* characteristic) override {
        owner_.enqueueControlWrite(characteristic->getData(), characteristic->getLength());
    }
#if defined(CONFIG_NIMBLE_ENABLED)
    void onWrite(BLECharacteristic* characteristic, ble_gap_conn_desc*) override {
        onWrite(characteristic);
    }
#endif
private:
    MgBleProfileServer& owner_;
};

class DataCallbacks final : public BLECharacteristicCallbacks {
public:
    explicit DataCallbacks(MgBleProfileServer& owner) : owner_(owner) {}

    void onWrite(BLECharacteristic* characteristic) override {
        owner_.enqueueDataWrite(characteristic->getData(), characteristic->getLength());
    }
#if defined(CONFIG_NIMBLE_ENABLED)
    void onWrite(BLECharacteristic* characteristic, ble_gap_conn_desc*) override {
        onWrite(characteristic);
    }
#endif
private:
    MgBleProfileServer& owner_;
};

class ServerCallbacks final : public BLEServerCallbacks {
public:
    explicit ServerCallbacks(MgBleProfileServer& owner) : owner_(owner) {}

    void onConnect(BLEServer*) override { owner_.noteConnectedFromCallback(); }
    void onDisconnect(BLEServer*) override { owner_.noteDisconnectedFromCallback(); }
#if defined(CONFIG_NIMBLE_ENABLED)
    void onConnect(BLEServer*, ble_gap_conn_desc*) override { owner_.noteConnectedFromCallback(); }
    void onDisconnect(BLEServer*, ble_gap_conn_desc*) override { owner_.noteDisconnectedFromCallback(); }
#endif
private:
    MgBleProfileServer& owner_;
};

ControlCallbacks* gControlCallbacks = nullptr;
DataCallbacks* gDataCallbacks = nullptr;
ServerCallbacks* gServerCallbacks = nullptr;

}  // namespace

bool MgBleProfileServer::begin(MapWebEditor& portal, DocumentImportValidator& validator) {
    portal_ = &portal;
    validator_ = &validator;
    if (initialized_) return true;

    eventQueue_ = xQueueCreate(6U, sizeof(Event));
    if (!eventQueue_) {
        stateText_ = "BLE event queue allocation failed";
        return false;
    }

    const uint64_t chip = ESP.getEfuseMac();
    char name[24] = {};
    snprintf(name, sizeof(name), "MG-TEST-%04X", static_cast<unsigned>(chip & 0xFFFFU));
    deviceName_ = name;

    Serial.printf("[BLE-PROFILE] initializing ESP-Hosted/NimBLE name=%s\n", deviceName_.c_str());
    Serial.flush();
    if (!BLEDevice::init(deviceName_)) {
        stateText_ = "ESP-Hosted BLE initialization failed";
        Serial.println("[BLE-PROFILE] BLEDevice::init FAILED; Wi-Fi fallback remains available");
        Serial.flush();
        return false;
    }
    (void)BLEDevice::setMTU(247U);

    if (!initGatt()) {
        stateText_ = "BLE GATT initialization failed";
        return false;
    }

    initialized_ = true;
    stateText_ = "BLE ready; open profile import to advertise";
    Serial.printf("[BLE-PROFILE] GATT ready address=%s\n", BLEDevice::getAddress().toString().c_str());
    Serial.flush();
    return true;
}

bool MgBleProfileServer::initGatt() {
    BLEServer* server = BLEDevice::createServer();
    if (!server) return false;
    server_ = server;
    if (!gServerCallbacks) gServerCallbacks = new ServerCallbacks(*this);
    server->setCallbacks(gServerCallbacks);

    BLEService* service = server->createService(kServiceUuid);
    if (!service) return false;
    service_ = service;

    BLECharacteristic* info = service->createCharacteristic(kDeviceInfoUuid, BLECharacteristic::PROPERTY_READ);
    BLECharacteristic* session = service->createCharacteristic(kSessionUuid, BLECharacteristic::PROPERTY_READ);
    BLECharacteristic* control = service->createCharacteristic(kControlUuid, BLECharacteristic::PROPERTY_WRITE);
    BLECharacteristic* data = service->createCharacteristic(kDataUuid, BLECharacteristic::PROPERTY_WRITE);
    BLECharacteristic* status = service->createCharacteristic(kStatusUuid, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    if (!info || !session || !control || !data || !status) return false;

    deviceInfoCharacteristic_ = info;
    sessionCharacteristic_ = session;
    controlCharacteristic_ = control;
    dataCharacteristic_ = data;
    statusCharacteristic_ = status;

    info->setValue("MG Smart Tester|BLE1|MG_PROFILE_JSON_V1");
    session->setValue("MG1|CLOSED|262144|180");
    status->setValue("IDLE");

    if (!gControlCallbacks) gControlCallbacks = new ControlCallbacks(*this);
    if (!gDataCallbacks) gDataCallbacks = new DataCallbacks(*this);
    control->setCallbacks(gControlCallbacks);
    data->setCallbacks(gDataCallbacks);

    service->start();

    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    if (!advertising) return false;
    advertisingObject_ = advertising;
    advertising->addServiceUUID(kServiceUuid);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMaxPreferred(0x12);
    BLEDevice::stopAdvertising();
    advertising_ = false;
    return true;
}

bool MgBleProfileServer::openSession() {
    if (!initialized_ || !portal_ || !validator_) {
        stateText_ = "BLE service unavailable";
        return false;
    }

    abortTransfer(true);
    profileReady_ = false;
    queueOverflow_ = false;
    disconnectPending_ = false;
    if (eventQueue_) xQueueReset(eventQueue_);

    char id[12] = {};
    snprintf(id, sizeof(id), "%08lX", static_cast<unsigned long>(esp_random()));
    sessionId_ = id;
    sessionOpen_ = true;
    refreshSessionCharacteristic();

    BLEDevice::stopAdvertising();
    BLEAdvertising* advertising = static_cast<BLEAdvertising*>(advertisingObject_);
    if (advertising && advertising->start()) {
        advertising_ = true;
        stateText_ = String("Advertising ") + deviceName_;
        Serial.printf("[BLE-PROFILE] session OPEN id=%s max=%lu chunk=%u\n",
                      sessionId_.c_str(),
                      static_cast<unsigned long>(kMaxProfileBytes),
                      static_cast<unsigned>(kMaxDataPayload));
        Serial.flush();
        return true;
    }

    advertising_ = false;
    sessionOpen_ = false;
    sessionId_ = String();
    refreshSessionCharacteristic();
    stateText_ = "BLE advertising could not start";
    return false;
}

void MgBleProfileServer::closeSession() {
    if (!initialized_) return;
    sessionOpen_ = false;
    BLEDevice::stopAdvertising();
    advertising_ = false;
    abortTransfer(true);
    sessionId_ = String();
    refreshSessionCharacteristic();
    stateText_ = profileReady_ ? "Profile received; BLE session closed" : "BLE profile session closed";
    Serial.println("[BLE-PROFILE] session CLOSED");
    Serial.flush();
}

void MgBleProfileServer::refreshSessionCharacteristic() {
    BLECharacteristic* characteristic = static_cast<BLECharacteristic*>(sessionCharacteristic_);
    if (!characteristic) return;
    if (!sessionOpen_ || sessionId_.isEmpty()) {
        characteristic->setValue("MG1|CLOSED|262144|180");
        return;
    }
    String value = String("MG1|") + sessionId_ + "|" + String(kMaxProfileBytes) + "|" + String(kMaxDataPayload);
    characteristic->setValue(value);
}

void MgBleProfileServer::enqueueControlWrite(const uint8_t* data, size_t length) {
    if (!eventQueue_ || !data || length == 0U) return;
    Event event;
    if (length > sizeof(event.data)) {
        queueOverflow_ = true;
        return;
    }
    event.type = EventType::Control;
    event.length = static_cast<uint16_t>(length);
    memcpy(event.data, data, event.length);
    if (xQueueSend(eventQueue_, &event, 0) != pdTRUE) queueOverflow_ = true;
}

void MgBleProfileServer::enqueueDataWrite(const uint8_t* data, size_t length) {
    if (!eventQueue_ || !data || length == 0U) return;
    Event event;
    if (length > sizeof(event.data)) {
        queueOverflow_ = true;
        return;
    }
    event.type = EventType::Data;
    event.length = static_cast<uint16_t>(length);
    memcpy(event.data, data, event.length);
    if (xQueueSend(eventQueue_, &event, 0) != pdTRUE) queueOverflow_ = true;
}

void MgBleProfileServer::noteConnectedFromCallback() {
    connected_ = true;
    advertising_ = false;
    disconnectPending_ = false;
}

void MgBleProfileServer::noteDisconnectedFromCallback() {
    connected_ = false;
    advertising_ = false;
    disconnectPending_ = true;
}

void MgBleProfileServer::tick() {
    if (!initialized_) return;

    if (queueOverflow_) {
        queueOverflow_ = false;
        fail("QUEUE", "BLE receive queue overflow", true);
    }

    if (disconnectPending_) {
        disconnectPending_ = false;
        if (transferActive_) {
            abortTransfer(true);
            stateText_ = "Phone disconnected; incomplete BLE profile discarded";
            if (portal_) portal_->noteDocumentTransportError(stateText_);
        } else if (sessionOpen_ && !profileReady_) {
            stateText_ = String("Waiting for phone on ") + deviceName_;
        }
        restartAdvertisingIfNeeded();
    }

    Event event;
    uint8_t processed = 0U;
    while (eventQueue_ && processed < 4U && xQueueReceive(eventQueue_, &event, 0) == pdTRUE) {
        if (event.type == EventType::Control) processControl(event.data, event.length);
        else processData(event.data, event.length);
        ++processed;
    }
}

void MgBleProfileServer::restartAdvertisingIfNeeded() {
    if (!sessionOpen_ || connected_ || advertising_) return;
    BLEAdvertising* advertising = static_cast<BLEAdvertising*>(advertisingObject_);
    if (advertising && advertising->start()) advertising_ = true;
}

void MgBleProfileServer::processControl(const uint8_t* data, size_t length) {
    if (!data || length == 0U) return;
    String command;
    command.reserve(length);
    for (size_t i = 0; i < length; ++i) command += static_cast<char>(data[i]);
    command.trim();

    if (command == "A") {
        abortTransfer(true);
        stateText_ = "BLE transfer aborted by phone";
        notifyStatus("ERR|ABORTED|Phone aborted transfer");
        return;
    }
    if (command.startsWith("S|")) {
        startTransfer(command);
        return;
    }
    if (command.startsWith("E|")) {
        finishTransfer(command);
        return;
    }
    fail("CONTROL", "Unsupported BLE control command", false);
}

void MgBleProfileServer::startTransfer(const String& command) {
    if (!sessionOpen_) {
        fail("SESSION_CLOSED", "Open profile import on MG Smart Tester", false);
        return;
    }
    if (profileReady_ || (portal_ && portal_->documentItemCount() > 0U)) {
        fail("PROFILE_EXISTS", "A profile is already stored for this session", false);
        return;
    }

    int p1 = command.indexOf('|');
    int p2 = command.indexOf('|', p1 + 1);
    int p3 = command.indexOf('|', p2 + 1);
    int p4 = command.indexOf('|', p3 + 1);
    if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0 || command.indexOf('|', p4 + 1) >= 0) {
        fail("START_FORMAT", "Expected S|session|bytes|crc32|parts", true);
        return;
    }

    const String session = command.substring(p1 + 1, p2);
    const String sizeText = command.substring(p2 + 1, p3);
    const String crcText = command.substring(p3 + 1, p4);
    const String partsText = command.substring(p4 + 1);
    if (session != sessionId_) {
        fail("SESSION", "BLE session id mismatch", true);
        return;
    }

    const unsigned long sizeValue = strtoul(sizeText.c_str(), nullptr, 10);
    const unsigned long partsValue = strtoul(partsText.c_str(), nullptr, 10);
    uint32_t crc = 0U;
    if (sizeValue == 0UL || sizeValue > kMaxProfileBytes || partsValue == 0UL || partsValue > 0xFFFFUL ||
        !parseHex32(crcText, crc)) {
        fail("START_VALUE", "Invalid profile size, CRC32 or part count", true);
        return;
    }

    abortTransfer(true);
    String reason;
    if (!portal_->prepareExternalProfileStorage(tempPath_, reason)) {
        fail("STORAGE", reason, false);
        return;
    }
    transferFile_ = LittleFS.open(tempPath_, FILE_WRITE);
    if (!transferFile_) {
        fail("STORAGE", "Cannot create BLE profile temporary file", true);
        return;
    }

    expectedBytes_ = static_cast<size_t>(sizeValue);
    receivedBytes_ = 0U;
    expectedCrc32_ = crc;
    runningCrc32_ = 0xFFFFFFFFUL;
    expectedParts_ = static_cast<uint16_t>(partsValue);
    nextPart_ = 0U;
    transferActive_ = true;
    profileReady_ = false;
    stateText_ = String("BLE receiving 0/") + String(expectedParts_);
    portal_->noteDocumentTransportError(String());
    notifyStatus(String("READY|") + sessionId_);
    Serial.printf("[BLE-PROFILE] START id=%s bytes=%lu crc=%08lX parts=%u\n",
                  sessionId_.c_str(), static_cast<unsigned long>(expectedBytes_),
                  static_cast<unsigned long>(expectedCrc32_), static_cast<unsigned>(expectedParts_));
    Serial.flush();
}

void MgBleProfileServer::processData(const uint8_t* data, size_t length) {
    if (!transferActive_) {
        fail("NO_START", "START must be accepted before profile data", false);
        return;
    }
    if (length < 3U || length > static_cast<size_t>(kMaxDataPayload) + 2U) {
        fail("CHUNK_SIZE", "BLE data chunk length is invalid", true);
        return;
    }

    const uint16_t part = static_cast<uint16_t>(data[0]) |
                          static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8U);
    if (part < nextPart_) {
        // Idempotent duplicate support: do not append twice; simply re-ACK a
        // chunk the phone may have retried after a lost notification.
        notifyStatus(String("ACK|") + String(part));
        return;
    }
    if (part != nextPart_ || part >= expectedParts_) {
        fail("CHUNK_ORDER", String("Expected part ") + String(nextPart_) + ", received " + String(part), true);
        return;
    }

    const size_t payloadLength = length - 2U;
    if (receivedBytes_ + payloadLength > expectedBytes_) {
        fail("CHUNK_OVERFLOW", "BLE profile exceeds announced size", true);
        return;
    }
    const size_t written = transferFile_.write(data + 2U, payloadLength);
    if (written != payloadLength) {
        fail("FLASH_WRITE", "BLE profile flash write failed", true);
        return;
    }

    runningCrc32_ = crc32Update(runningCrc32_, data + 2U, payloadLength);
    receivedBytes_ += payloadLength;
    ++nextPart_;
    stateText_ = String("BLE receiving ") + String(nextPart_) + "/" + String(expectedParts_);
    notifyStatus(String("ACK|") + String(part));
}

void MgBleProfileServer::finishTransfer(const String& command) {
    if (!transferActive_) {
        fail("NO_START", "No active BLE profile transfer", false);
        return;
    }
    const int separator = command.indexOf('|');
    if (separator < 0 || command.substring(separator + 1) != sessionId_) {
        fail("SESSION", "END session id mismatch", true);
        return;
    }
    if (nextPart_ != expectedParts_ || receivedBytes_ != expectedBytes_) {
        fail("INCOMPLETE", String("Received ") + String(receivedBytes_) + "/" + String(expectedBytes_) + " bytes", true);
        return;
    }

    transferFile_.flush();
    transferFile_.close();
    notifyStatus(String("RECEIVED|") + String(receivedBytes_));

    const uint32_t actualCrc = ~runningCrc32_;
    if (actualCrc != expectedCrc32_) {
        char detail[96] = {};
        snprintf(detail, sizeof(detail), "CRC32 expected %08lX got %08lX",
                 static_cast<unsigned long>(expectedCrc32_), static_cast<unsigned long>(actualCrc));
        fail("CRC", detail, true);
        return;
    }
    notifyStatus("CRC_OK");

    String reason;
    if (!portal_->commitExternalPreparedProfile(tempPath_, "mg_profile_ble.json",
                                                receivedBytes_, actualCrc, reason)) {
        fail("COMMIT", reason, true);
        return;
    }
    tempPath_ = String();
    transferActive_ = false;

    // loopTask has a small internal-RAM stack. ProductionProfileRecord
    // (~1.7 KiB) plus CableMap (~2.1 KiB) used to live in this stack frame; the
    // subsequent validator call added another 1 KiB buffer and overflowed the
    // loopTask stack immediately after the durable profile commit. Keep the
    // large validation scratch objects on the heap instead.
    auto record = std::unique_ptr<ProductionProfileRecord>(
        new (std::nothrow) ProductionProfileRecord{});
    auto profile = std::unique_ptr<CableProfile>(
        new (std::nothrow) CableProfile(makeNormalProfile()));
    auto map = std::unique_ptr<CableMap>(
        new (std::nothrow) CableMap());
    if (!record || !profile || !map) {
        (void)portal_->clearDocumentItems();
        fail("MEMORY", "Insufficient heap for profile validation", false);
        return;
    }

    Serial.printf("[BLE-PROFILE] validation scratch=HEAP free=%lu psram=%lu\n",
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(ESP.getFreePsram()));
    Serial.flush();

    String validationMessage;
    if (!PreparedProfileService::loadAndValidate(*portal_, *validator_,
                                                *record, *profile, *map,
                                                validationMessage)) {
        // A profile that fails the device's independent electrical validation
        // must not remain as a committed candidate.
        (void)portal_->clearDocumentItems();
        portal_->noteDocumentTransportError(validationMessage);
        fail("VALIDATION", validationMessage, false);
        return;
    }

    notifyStatus("VALIDATED");
    profileReady_ = true;
    stateText_ = String("PROFILE_READY: ") + record->profileId;
    portal_->noteDocumentTransportError(String());
    notifyStatus("PROFILE_READY");
    // The phone has its terminal success notification; do not keep exposing a
    // new writable session for a profile that is already committed.
    sessionOpen_ = false;
    advertising_ = false;
    refreshSessionCharacteristic();
    Serial.printf("[BLE-PROFILE] PROFILE_READY id=%s pr=%s pins=%u bytes=%lu crc=%08lX\n",
                  record->profileId, record->productionPr,
                  static_cast<unsigned>(record->signalPinCount),
                  static_cast<unsigned long>(receivedBytes_),
                  static_cast<unsigned long>(actualCrc));
    Serial.flush();
}

void MgBleProfileServer::abortTransfer(bool deletePartial) {
    if (transferFile_) {
        transferFile_.flush();
        transferFile_.close();
    }
    if (deletePartial && portal_ && !tempPath_.isEmpty()) {
        portal_->abortExternalPreparedProfile(tempPath_);
    }
    tempPath_ = String();
    transferActive_ = false;
    expectedBytes_ = 0U;
    receivedBytes_ = 0U;
    expectedCrc32_ = 0U;
    runningCrc32_ = 0xFFFFFFFFUL;
    expectedParts_ = 0U;
    nextPart_ = 0U;
}

void MgBleProfileServer::fail(const char* code, const String& detail, bool abortActive) {
    if (abortActive) abortTransfer(true);
    String safeDetail = detail;
    safeDetail.replace('|', '/');
    if (safeDetail.length() > 150U) safeDetail.remove(150U);
    stateText_ = String("BLE error: ") + safeDetail;
    if (portal_) portal_->noteDocumentTransportError(stateText_);
    notifyStatus(String("ERR|") + (code ? code : "ERROR") + "|" + safeDetail);
    Serial.printf("[BLE-PROFILE] ERR code=%s detail=%s\n", code ? code : "ERROR", safeDetail.c_str());
    Serial.flush();
}

void MgBleProfileServer::notifyStatus(const String& status) {
    BLECharacteristic* characteristic = static_cast<BLECharacteristic*>(statusCharacteristic_);
    if (!characteristic) return;
    characteristic->setValue(status);
    if (connected_) characteristic->notify();
}

uint32_t MgBleProfileServer::crc32Update(uint32_t crc, const uint8_t* data, size_t length) {
    if (!data) return crc;
    for (size_t index = 0; index < length; ++index) {
        crc ^= static_cast<uint32_t>(data[index]);
        for (uint8_t bit = 0; bit < 8U; ++bit) {
            const uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
        }
    }
    return crc;
}

bool MgBleProfileServer::parseHex32(const String& value, uint32_t& output) {
    if (value.length() != 8U) return false;
    uint32_t parsed = 0U;
    for (size_t i = 0; i < 8U; ++i) {
        const char c = value.charAt(i);
        uint8_t nibble = 0U;
        if (c >= '0' && c <= '9') nibble = static_cast<uint8_t>(c - '0');
        else if (c >= 'a' && c <= 'f') nibble = static_cast<uint8_t>(10 + c - 'a');
        else if (c >= 'A' && c <= 'F') nibble = static_cast<uint8_t>(10 + c - 'A');
        else return false;
        parsed = (parsed << 4U) | nibble;
    }
    output = parsed;
    return true;
}

}  // namespace mg::p4
