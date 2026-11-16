/*  PokemonLZA AutoStory - New game intro screens Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Tests/ImageUnitTest.h"
#include "PokemonLZA_AutoStoryIntroScreens.h"
#include "PokemonLZA_AutoStoryIntroScreens_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

std::string describe(std::optional<size_t> value){
    return value ? std::format("{}", *value) : std::string("none");
}

//  One console frame and what each reader makes of it.
struct FrameCase{
    std::string name;
    struct Input{
        std::string file;   //  Under the test images' PokemonLZA/.
    };
    Input input;
    struct Expectation{
        std::optional<size_t> language_entry = std::nullopt;
        bool look_picker = false;
        std::optional<size_t> look = std::nullopt;
        bool keyboard = false;
        std::optional<size_t> name_limit = std::nullopt;
    };
    Expectation expectation;
};

const FrameCase FRAME_CASES[] = {
    {.name = "language list on English", .input = {.file = "AutoStoryLanguageList/0_en.png"}, .expectation = {.language_entry = 0}},
    {.name = "language list on Espanol (Espana)", .input = {.file = "AutoStoryLanguageList/1_es-ES.png"}, .expectation = {.language_entry = 1}},
    {.name = "language list on Espanol (Latinoamerica)", .input = {.file = "AutoStoryLanguageList/2_es-419.png"}, .expectation = {.language_entry = 2}},
    {.name = "language list on Francais", .input = {.file = "AutoStoryLanguageList/3_fr.png"}, .expectation = {.language_entry = 3}},
    {.name = "language list on Deutsch", .input = {.file = "AutoStoryLanguageList/4_de.png"}, .expectation = {.language_entry = 4}},
    {.name = "language list on Italiano", .input = {.file = "AutoStoryLanguageList/5_it.png"}, .expectation = {.language_entry = 5}},
    {.name = "language list on Japanese", .input = {.file = "AutoStoryLanguageList/6_ja.png"}, .expectation = {.language_entry = 6}},
    {.name = "language list on Korean", .input = {.file = "AutoStoryLanguageList/7_ko.png"}, .expectation = {.language_entry = 7}},
    {.name = "language list on Simplified Chinese", .input = {.file = "AutoStoryLanguageList/8_zh-Hans.png"}, .expectation = {.language_entry = 8}},
    {.name = "language list on Traditional Chinese", .input = {.file = "AutoStoryLanguageList/9_zh-Hant.png"}, .expectation = {.language_entry = 9}},
    {.name = "look picker as it opens", .input = {.file = "AutoStoryLookPicker/English_1_0.png"}, .expectation = {.look_picker = true}},
    {.name = "look picker on look 3", .input = {.file = "AutoStoryLookPicker/English_2_3.png"}, .expectation = {.look_picker = true, .look = 3}},
    {.name = "look picker on look 7", .input = {.file = "AutoStoryLookPicker/German_2_7.png"}, .expectation = {.look_picker = true, .look = 7}},
    {.name = "look picker on look 6 in Japanese", .input = {.file = "AutoStoryLookPicker/Japanese_2_6.png"}, .expectation = {.look_picker = true, .look = 6}},
    {.name = "keyboard in German", .input = {.file = "AutoStoryNameKeyboard/de_12.png"}, .expectation = {.keyboard = true, .name_limit = 12}},
    {.name = "keyboard in Spanish (Spain)", .input = {.file = "AutoStoryNameKeyboard/es-ES_12.png"}, .expectation = {.keyboard = true, .name_limit = 12}},
    {.name = "keyboard in Latin American Spanish", .input = {.file = "AutoStoryNameKeyboard/es-419_12.png"}, .expectation = {.keyboard = true, .name_limit = 12}},
    {.name = "keyboard in French", .input = {.file = "AutoStoryNameKeyboard/fr_12.png"}, .expectation = {.keyboard = true, .name_limit = 12}},
    {.name = "keyboard in Italian", .input = {.file = "AutoStoryNameKeyboard/it_12.png"}, .expectation = {.keyboard = true, .name_limit = 12}},
    {.name = "keyboard in Japanese", .input = {.file = "AutoStoryNameKeyboard/ja_6.png"}, .expectation = {.keyboard = true, .name_limit = 6}},
    {.name = "keyboard in Korean", .input = {.file = "AutoStoryNameKeyboard/ko_6.png"}, .expectation = {.keyboard = true, .name_limit = 6}},
    {.name = "keyboard in Simplified Chinese", .input = {.file = "AutoStoryNameKeyboard/zh-Hans_6.png"}, .expectation = {.keyboard = true, .name_limit = 6}},
    {.name = "keyboard in Traditional Chinese", .input = {.file = "AutoStoryNameKeyboard/zh-Hant_6.png"}, .expectation = {.keyboard = true, .name_limit = 6}},
    {.name = "Confirm Choices is none of them", .input = {.file = "AutoStoryConfirmChoices/German_0.png"}, .expectation = {}},
};

class Test_IntroScreenFrame : public ImageUnitTest{
public:
    explicit Test_IntroScreenFrame(const FrameCase& test)
        : ImageUnitTest(
            std::format("PokemonLZA::AutoStory - intro screens: {}", test.name),
            {std::format("PokemonLZA/{}", test.input.file)}
        )
        , m_test(test)
    {}
    virtual UnitTestResult run_on_images(Logger&, CancellableScope&, const std::vector<ImageRGB32>& images) const override{
        const ImageRGB32& frame = images[0];
        const FrameCase::Expectation& expected = m_test.expectation;
        const std::optional<size_t> entry = read_language_list_cursor(frame);
        if (entry != expected.language_entry){
            return std::format("Language list: expected entry {}, read {}.", describe(expected.language_entry), describe(entry));
        }
        if (is_look_picker(frame) != expected.look_picker){
            return std::format("Look picker: expected {}, read {}.", expected.look_picker, !expected.look_picker);
        }
        const std::optional<size_t> look = read_look_picker_cursor(frame);
        if (look != expected.look){
            return std::format("Look picker: expected look {}, read {}.", describe(expected.look), describe(look));
        }
        if (is_name_keyboard(frame) != expected.keyboard){
            return std::format("Name keyboard: expected {}, read {}.", expected.keyboard, !expected.keyboard);
        }
        if (expected.keyboard){
            const std::optional<size_t> limit = read_name_keyboard_limit(frame);
            if (limit != expected.name_limit){
                return std::format("Name keyboard: expected a limit of {}, read {}.", describe(expected.name_limit), describe(limit));
            }
        }
        return true;
    }

private:
    const FrameCase& m_test;
};


//  Confirm Choices' name and language, read with OCR. The console's frames
//  were taken with the name "q".
class Test_ConfirmChoicesProfile : public ImageUnitTest{
public:
    Test_ConfirmChoicesProfile()
        : ImageUnitTest(
            "PokemonLZA::AutoStory - intro screens: Confirm Choices shows the name and language",
            {"PokemonLZA/AutoStoryConfirmChoices/English_0.png"}
        )
    {}
    virtual UnitTestResult run_on_images(Logger&, CancellableScope&, const std::vector<ImageRGB32>& images) const override{
        const std::string name = read_confirm_choices_name(images[0]);
        if (name != "q"){
            return std::format("Expected the name \"q\", read \"{}\".", name);
        }
        const std::string language = read_confirm_choices_language(images[0], Language::English);
        if (language.find("English") == std::string::npos){
            return std::format("Expected the language \"English\", read \"{}\".", language);
        }
        return true;
    }
};

} // namespace


void add_tests_AutoStoryIntroScreens(UnitTestDatabase& database){
    for (const FrameCase& test : FRAME_CASES){
        database.add<Test_IntroScreenFrame>(test);
    }
    database.add<Test_ConfirmChoicesProfile>();
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
