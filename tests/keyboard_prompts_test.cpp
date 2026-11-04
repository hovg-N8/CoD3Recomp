#define BUTTON_PROMPTS_UNIT_TEST
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include "button_prompts.cpp"
#include <stdexcept>
namespace Settings { Values testValues; Values Get() { return testValues; } }
namespace Overlay { void RequestScreenshot() {} }
uint32_t Guest::AllocateKernelObject(uint32_t) { return 0; }
int main() {
    int checks=0;
    auto equal=[&](std::string got,std::string expected) {
        ++checks;
        if(got!=expected) throw std::runtime_error("expected "+expected+", got "+got);
    };
    Settings::Values v;
    v.keys[Settings::Fire]=VK_LBUTTON;v.keys[Settings::Aim]=VK_RBUTTON;
    v.keys[Settings::Reload]='R';v.keys[Settings::Use]='E';
    v.keys[Settings::Jump]=VK_SPACE;v.keys[Settings::Crouch]='C';
    v.keys[Settings::Weapon]='F';v.keys[Settings::Melee]='V';
    v.keys[Settings::Sprint]=VK_SHIFT;v.keys[Settings::Grenade]='G';
    v.keys[Settings::Smoke]='4';v.keys[Settings::Objectives]=VK_TAB;
    v.keys[Settings::Pause]=VK_ESCAPE;v.keys[Settings::BinocularsOut]='B';
    v.keys[Settings::BinocularsAway]='N';
    v.keys[Settings::Forward]='W';v.keys[Settings::Left]='A';v.keys[Settings::Backward]='S';v.keys[Settings::Right]='D';
    Settings::testValues=v;
    equal(ButtonPrompts::ActionKey(Settings::Fire),"[M1]");
    equal(PlainText("PRESS START"),"PRESS ENTER");
    equal(PlainText("Use the right and left sticks to move and look around."),"Use the [W/A/S/D] and the mouse to move and look around.");
    equal(Label(0x21,v,"fire"),"M1");equal(Label(0x26,v,"aim"),"M2");
    equal(Label(0x23,v,"press to reload"),"R");equal(Label(0x23,v,"hold to use"),"E");
    equal(Label(0x22,v,"jump"),"Space");equal(Label(0x25,v,"crouch"),"C");
    equal(Label(0x25,v,"select back"),"Esc");equal(Label(0x24,v,"weapon"),"F");
    equal(Label(0x33,v,"melee"),"V");equal(Label(0x32,v,"sprint"),"Shift");
    equal(Label(0x28,v,"grenade"),"G");equal(Label(0x27,v,"smoke"),"4");
    equal(Label(0x31,v,"objectives"),"Tab");equal(Label(0x2D,v,"continue"),"Enter");
    equal(Label(0x32,v,"binoculars"),"B");equal(Label(0x2C,v,"binoculars away"),"N");
    // Each conversion must read the current bindings, including rebinding
    // Reload independently of Use even though both emulate the Xbox X button.
    v.keys[Settings::Fire]=VK_XBUTTON1;v.keys[Settings::Reload]='T';
    Settings::testValues=v;
    equal(ButtonPrompts::ActionKey(Settings::Fire),"[M4]");
    equal(ButtonPrompts::ActionKey(Settings::Reload),"[T]");
    equal(Label(0x21,v,"fire"),"M4");equal(Label(0x23,v,"reload"),"T");
    equal(Label(0x23,v,"pick up"),"E");
    v.keys[Settings::Aim]=0;equal(Label(0x26,v,"aim"),"-");
    equal(Label(0xFF,v,"unknown"),"");equal(Key(VK_RSHIFT),"RShift");
    equal(Key(VK_F12),"F12");equal(Key(VK_NUMPAD7),"Num7");
    ButtonPrompts::ControllerUsed();if(keyboard.load()) throw std::runtime_error("controller switching failed");++checks;
    ButtonPrompts::KeyboardUsed();if(!keyboard.load()) throw std::runtime_error("keyboard switching failed");++checks;
    printf("PASS: %d keyboard prompt mapping, rebinding and device-switching checks\n",checks);
}
