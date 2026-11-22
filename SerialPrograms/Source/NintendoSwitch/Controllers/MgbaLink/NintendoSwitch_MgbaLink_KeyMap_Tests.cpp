/*  Nintendo Switch: Pro Controller to GBA keys (MgbaLink) Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Controllers/MgbaLink/MgbaLink_Keys.h"
#include "NintendoSwitch_MgbaLink_KeyMap.h"
#include "NintendoSwitch_MgbaLink_KeyMap_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{

using namespace MgbaLink;


namespace{

struct KeyMapInput{
    Button buttons = BUTTON_NONE;
    DpadPosition dpad = DPAD_NONE;
    JoystickPosition left_joystick = {};
    JoystickPosition right_joystick = {};
};
struct KeyMapExpectation{
    uint16_t keys = 0;
    Button unmapped = BUTTON_NONE;
    bool right_stick_moved = false;
};
struct KeyMapCase{
    std::string name;
    KeyMapInput input;
    KeyMapExpectation expectation;
};

class Test_KeyMap : public UnitTest{
public:
    Test_KeyMap(KeyMapCase test_case)
        : UnitTest(std::format("NintendoSwitch::MgbaLink - key map - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        SwitchControllerState state;
        state.buttons = m_case.input.buttons;
        state.dpad = m_case.input.dpad;
        state.left_joystick = m_case.input.left_joystick;
        state.right_joystick = m_case.input.right_joystick;
        GbaKeysFromSwitch result = gba_keys_from_switch(state);
        const KeyMapExpectation& expected = m_case.expectation;
        if (result.keys != expected.keys
            || result.unmapped != expected.unmapped
            || result.right_stick_moved != expected.right_stick_moved
        ){
            return std::format(
                "keys {}, unmapped <{}>, right stick {}; expected keys {}, unmapped <{}>, right stick {}",
                result.keys, button_to_string(result.unmapped), result.right_stick_moved,
                expected.keys, button_to_string(expected.unmapped), expected.right_stick_moved
            );
        }
        return true;
    }

private:
    KeyMapCase m_case;
};

}  // namespace



void add_tests_MgbaLinkKeyMap(UnitTestDatabase& database){
    const KeyMapCase CASES[] = {
        {.name = "nothing pressed", .input = {}, .expectation = {}},
        {.name = "A", .input = {.buttons = BUTTON_A}, .expectation = {.keys = GBA_KEY_A}},
        {.name = "B", .input = {.buttons = BUTTON_B}, .expectation = {.keys = GBA_KEY_B}},
        {.name = "L and R", .input = {.buttons = BUTTON_L | BUTTON_R}, .expectation = {.keys = GBA_KEY_L | GBA_KEY_R}},
        {.name = "Plus is Start", .input = {.buttons = BUTTON_PLUS}, .expectation = {.keys = GBA_KEY_START}},
        {.name = "Minus is Select", .input = {.buttons = BUTTON_MINUS}, .expectation = {.keys = GBA_KEY_SELECT}},
        {.name = "D-pad up", .input = {.dpad = DPAD_UP}, .expectation = {.keys = GBA_KEY_UP}},
        {.name = "D-pad down-left", .input = {.dpad = DPAD_DOWN_LEFT}, .expectation = {.keys = GBA_KEY_DOWN | GBA_KEY_LEFT}},
        {.name = "split D-pad buttons", .input = {.buttons = BUTTON_RIGHT | BUTTON_DOWN}, .expectation = {.keys = GBA_KEY_RIGHT | GBA_KEY_DOWN}},
        {.name = "stick full up", .input = {.left_joystick = {0, 1}}, .expectation = {.keys = GBA_KEY_UP}},
        {.name = "stick full left", .input = {.left_joystick = {-1, 0}}, .expectation = {.keys = GBA_KEY_LEFT}},
        {.name = "stick at half tilt does nothing", .input = {.left_joystick = {0.5, -0.5}}, .expectation = {}},
        {.name = "stick down-right past half", .input = {.left_joystick = {0.7, -0.7}}, .expectation = {.keys = GBA_KEY_RIGHT | GBA_KEY_DOWN}},
        {
            .name = "X Y ZL ZR Home Capture unmapped",
            .input = {.buttons = BUTTON_A | BUTTON_X | BUTTON_Y | BUTTON_ZL | BUTTON_ZR | BUTTON_HOME | BUTTON_CAPTURE},
            .expectation = {.keys = GBA_KEY_A, .unmapped = BUTTON_X | BUTTON_Y | BUTTON_ZL | BUTTON_ZR | BUTTON_HOME | BUTTON_CAPTURE},
        },
        {.name = "right stick moved", .input = {.right_joystick = {1, 0}}, .expectation = {.right_stick_moved = true}},
    };
    for (const KeyMapCase& test_case : CASES){
        database.add<Test_KeyMap>(test_case);
    }
}


}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
