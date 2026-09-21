#pragma once

#include <Arduino.h>

namespace mg::p4 {

// Presentation-only localization for mobile profile import messages.
String localizeDocumentUiMessage(const String& message);
const char* localizeDocumentUiMessage(const char* message);

}  // namespace mg::p4
