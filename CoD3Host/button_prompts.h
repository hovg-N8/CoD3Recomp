#pragma once
#include "settings.h"
#include <string>
namespace ButtonPrompts {
    void KeyboardUsed();
    void ControllerUsed();
    bool KeyboardActive();
    std::string ActionKey(Settings::Action action);
}
