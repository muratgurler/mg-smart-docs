#pragma once

#include <stdint.h>

namespace mg::p4::build {

constexpr uint8_t kVersionMajor = 1;
constexpr uint8_t kVersionMinor = 0;
constexpr bool kIsBeta = true;
constexpr uint8_t kFixNumber = 1;
constexpr char kReleaseName[] = "Beta 1.0";
constexpr char kReleaseId[] = "MG_TESTER_BETA_1_0";
constexpr char kPatchName[] = "Fix1";
constexpr char kBuildLabel[] = "Beta 1.0 Fix1";
constexpr char kProductName[] = "MG Smart Tester";

}  // namespace mg::p4::build
