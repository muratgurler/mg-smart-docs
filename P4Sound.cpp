#include "P4Sound.h"

namespace mg::p4 {

namespace {
bool muted = false;
}

bool p4SoundMuted() {
    return muted;
}

void setP4SoundMuted(bool value) {
    muted = value;
}

void toggleP4SoundMuted() {
    muted = !muted;
}

}  // namespace mg::p4
