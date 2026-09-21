#pragma once
#include "NetworkClient.h"
class NetworkClientSecure : public NetworkClient { public: void setCACert(const char*) {} };
