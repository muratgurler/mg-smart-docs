#include <cassert>
#include <cstdint>
#include <cstring>

#include "FeatureBackbone.h"

FakeSerialClass Serial;
static uint32_t fakeNow = 0;
uint32_t millis() { return fakeNow; }

using namespace mg::p4;

int main() {
    mg::p4::v33::V33HardwareManager hardware;
    FeatureBackbone backbone;
    backbone.begin(hardware);

    const BackboneModuleId modules[] = {
        BackboneModuleId::Scan128,
        BackboneModuleId::Kelvin,
        BackboneModuleId::SelfTestCalibration,
        BackboneModuleId::Tdr,
        BackboneModuleId::PairIntegrity,
        BackboneModuleId::UsbC,
        BackboneModuleId::PeDs,
        BackboneModuleId::FixtureSpc,
        BackboneModuleId::SmartProbe,
        BackboneModuleId::Network,
        BackboneModuleId::Voice,
        BackboneModuleId::EnvironmentAwg,
        BackboneModuleId::BarcodeQr,
        BackboneModuleId::ComponentTest,
        BackboneModuleId::HardwareValidation,
        BackboneModuleId::CableLearn,
        BackboneModuleId::MultiConnector,
        BackboneModuleId::FlexGlitch,
    };

    for (const BackboneModuleId module : modules) {
        backbone.select(module, "TEST WORKFLOW");
        auto snap = backbone.snapshot();
        assert(snap.state == BackboneRunState::Idle);
        assert(snap.usingDemo);
        assert(snap.progressPercent == 0);
        assert(std::strlen(FeatureBackbone::moduleTitle(module, false)) > 0);
        assert(std::strlen(FeatureBackbone::moduleDescription(module, false)) > 0);
        assert(std::strlen(FeatureBackbone::phaseText(module, 0, false)) > 0);

        backbone.start();
        assert(backbone.snapshot().state == BackboneRunState::Running);
        for (int i = 0; i < 10 && backbone.snapshot().state != BackboneRunState::Complete; ++i) {
            fakeNow += 800;
            backbone.tick(fakeNow);
        }
        snap = backbone.snapshot();
        assert(snap.state == BackboneRunState::Complete);
        assert(snap.progressPercent == 100);
        assert(std::strlen(snap.metric1) > 0);
    }

    backbone.select(BackboneModuleId::Kelvin, "KELVIN");
    backbone.step();
    auto snap = backbone.snapshot();
    assert(snap.state == BackboneRunState::Paused);
    assert(snap.phaseIndex == 1);
    backbone.pauseResume();
    assert(backbone.snapshot().state == BackboneRunState::Running);
    backbone.reset();
    assert(backbone.snapshot().state == BackboneRunState::Idle);

    return 0;
}
