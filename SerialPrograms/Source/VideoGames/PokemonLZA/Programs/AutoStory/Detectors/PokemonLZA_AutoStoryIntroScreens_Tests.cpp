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
#include "NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.h"
#include "Tests/ImageUnitTest.h"
#include "PokemonLZA_AutoStoryConfirmChoicesReader.h"
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
        //  The counter's limit; nullopt: not checked (the field holds text).
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
    //  The console's frames of 2026-11-15, one launch per language.
    {.name = "2026-11-15: the language list opens on English", .input = {.file = "AutoStoryNewGame/2026-11-15/en_langlist.png"}, .expectation = {.language_entry = 0}},
    {.name = "2026-11-15: the language list on Espanol (Espana)", .input = {.file = "AutoStoryNewGame/2026-11-15/esES_langsel.png"}, .expectation = {.language_entry = 1}},
    {.name = "2026-11-15: the language list on Espanol (Latinoamerica)", .input = {.file = "AutoStoryNewGame/2026-11-15/esLA_langsel.png"}, .expectation = {.language_entry = 2}},
    {.name = "2026-11-15: the language list on Francais", .input = {.file = "AutoStoryNewGame/2026-11-15/fr_langsel.png"}, .expectation = {.language_entry = 3}},
    {.name = "2026-11-15: the language list on Deutsch", .input = {.file = "AutoStoryNewGame/2026-11-15/de_langsel.png"}, .expectation = {.language_entry = 4}},
    {.name = "2026-11-15: the language list on Italiano", .input = {.file = "AutoStoryNewGame/2026-11-15/it_langsel.png"}, .expectation = {.language_entry = 5}},
    {.name = "2026-11-15: the language list on Japanese", .input = {.file = "AutoStoryNewGame/2026-11-15/ja_langsel.png"}, .expectation = {.language_entry = 6}},
    {.name = "2026-11-15: the language list on Korean", .input = {.file = "AutoStoryNewGame/2026-11-15/ko_langsel.png"}, .expectation = {.language_entry = 7}},
    {.name = "2026-11-15: the language list on Simplified Chinese", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHans_langsel.png"}, .expectation = {.language_entry = 8}},
    {.name = "2026-11-15: the language list on Traditional Chinese", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHant_langsel.png"}, .expectation = {.language_entry = 9}},
    {.name = "2026-11-15: the look picker before the stick moves", .input = {.file = "AutoStoryNewGame/2026-11-15/en_look.png"}, .expectation = {.look_picker = true}},
    {.name = "2026-11-15: the look picker after the first tilt in English", .input = {.file = "AutoStoryNewGame/2026-11-15/en_look_hl.png"}, .expectation = {.look_picker = true, .look = 6}},
    {.name = "2026-11-15: the look picker after the first tilt in Espanol (Espana)", .input = {.file = "AutoStoryNewGame/2026-11-15/esES_look_hl.png"}, .expectation = {.look_picker = true, .look = 5}},
    {.name = "2026-11-15: the look picker after the first tilt in Espanol (Latinoamerica)", .input = {.file = "AutoStoryNewGame/2026-11-15/esLA_look_hl.png"}, .expectation = {.look_picker = true, .look = 6}},
    {.name = "2026-11-15: the look picker after the first tilt in Francais", .input = {.file = "AutoStoryNewGame/2026-11-15/fr_look_hl.png"}, .expectation = {.look_picker = true, .look = 1}},
    {.name = "2026-11-15: the look picker after the first tilt in Deutsch", .input = {.file = "AutoStoryNewGame/2026-11-15/de_look_hl.png"}, .expectation = {.look_picker = true, .look = 5}},
    {.name = "2026-11-15: the look picker after the first tilt in Italiano", .input = {.file = "AutoStoryNewGame/2026-11-15/it_look_hl.png"}, .expectation = {.look_picker = true, .look = 1}},
    {.name = "2026-11-15: the look picker after the first tilt in Japanese", .input = {.file = "AutoStoryNewGame/2026-11-15/ja_look_hl.png"}, .expectation = {.look_picker = true, .look = 7}},
    {.name = "2026-11-15: the look picker after the first tilt in Korean", .input = {.file = "AutoStoryNewGame/2026-11-15/ko_look_hl.png"}, .expectation = {.look_picker = true, .look = 2}},
    {.name = "2026-11-15: the look picker after the first tilt in Simplified Chinese", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHans_look_hl.png"}, .expectation = {.look_picker = true, .look = 1}},
    {.name = "2026-11-15: the look picker after the first tilt in Traditional Chinese", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHant_look_hl.png"}, .expectation = {.look_picker = true, .look = 8}},
    {.name = "2026-11-15: the keyboard in English", .input = {.file = "AutoStoryNewGame/2026-11-15/en_kb.png"}, .expectation = {.keyboard = true, .name_limit = 12}},
    {.name = "2026-11-15: the keyboard in Espanol (Espana)", .input = {.file = "AutoStoryNewGame/2026-11-15/esES_kb.png"}, .expectation = {.keyboard = true, .name_limit = 12}},
    {.name = "2026-11-15: the keyboard in Espanol (Latinoamerica)", .input = {.file = "AutoStoryNewGame/2026-11-15/esLA_kb.png"}, .expectation = {.keyboard = true, .name_limit = 12}},
    {.name = "2026-11-15: the keyboard in Francais", .input = {.file = "AutoStoryNewGame/2026-11-15/fr_kb.png"}, .expectation = {.keyboard = true, .name_limit = 12}},
    {.name = "2026-11-15: the keyboard in Deutsch", .input = {.file = "AutoStoryNewGame/2026-11-15/de_kb.png"}, .expectation = {.keyboard = true, .name_limit = 12}},
    {.name = "2026-11-15: the keyboard in Italiano", .input = {.file = "AutoStoryNewGame/2026-11-15/it_kb.png"}, .expectation = {.keyboard = true, .name_limit = 12}},
    {.name = "2026-11-15: the keyboard in Japanese", .input = {.file = "AutoStoryNewGame/2026-11-15/ja_kb.png"}, .expectation = {.keyboard = true, .name_limit = 6}},
    {.name = "2026-11-15: the keyboard in Korean", .input = {.file = "AutoStoryNewGame/2026-11-15/ko_kb.png"}, .expectation = {.keyboard = true, .name_limit = 6}},
    {.name = "2026-11-15: the keyboard in Simplified Chinese", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHans_kb.png"}, .expectation = {.keyboard = true, .name_limit = 6}},
    {.name = "2026-11-15: the keyboard in Traditional Chinese", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHant_kb.png"}, .expectation = {.keyboard = true, .name_limit = 6}},
    {.name = "2026-11-15: Simplified Chinese keeps the keyboard after the first +", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHans_plus0.png"}, .expectation = {.keyboard = true, .name_limit = std::nullopt}},
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
        if (expected.keyboard && expected.name_limit){
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


//  The name keyboard as the Switch's keyboard typer (type_on_system_keyboard())
//  reads it when it opens: the cursor on "1". Every language the program
//  types in (Korean is not one).
struct SystemKeyboardCase{
    std::string name;
    struct Input{
        std::string file;   //  Under the test images' PokemonLZA/.
    };
    Input input;
};

const SystemKeyboardCase SYSTEM_KEYBOARD_CASES[] = {
    {.name = "English", .input = {.file = "AutoStoryNewGame/2026-11-15/en_kb.png"}},
    {.name = "Espanol (Espana)", .input = {.file = "AutoStoryNewGame/2026-11-15/esES_kb.png"}},
    {.name = "Espanol (Latinoamerica)", .input = {.file = "AutoStoryNewGame/2026-11-15/esLA_kb.png"}},
    {.name = "Francais", .input = {.file = "AutoStoryNewGame/2026-11-15/fr_kb.png"}},
    {.name = "Deutsch", .input = {.file = "AutoStoryNewGame/2026-11-15/de_kb.png"}},
    {.name = "Italiano", .input = {.file = "AutoStoryNewGame/2026-11-15/it_kb.png"}},
    {.name = "Japanese", .input = {.file = "AutoStoryNewGame/2026-11-15/ja_kb.png"}},
    {.name = "Simplified Chinese", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHans_kb.png"}},
    {.name = "Traditional Chinese", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHant_kb.png"}},
};

class Test_SystemKeyboardOnNameKeyboard : public ImageUnitTest{
public:
    explicit Test_SystemKeyboardOnNameKeyboard(const SystemKeyboardCase& test)
        : ImageUnitTest(
            std::format("PokemonLZA::AutoStory - intro screens: the system keyboard reader in {}", test.name),
            {std::format("PokemonLZA/{}", test.input.file)}
        )
    {}
    virtual UnitTestResult run_on_images(Logger&, CancellableScope&, const std::vector<ImageRGB32>& images) const override{
        const ImageRGB32& frame = images[0];
        if (!SystemKeyboardDetector().detect(frame)){
            return std::string("The system keyboard was not detected.");
        }
        const std::optional<SystemKeyboardKey> cursor = read_system_keyboard_cursor(frame);
        if (cursor != SystemKeyboardKey{.row = 0, .col = 0}){
            return std::format(
                "Expected the cursor on \"1\" (0, 0), read {}.",
                cursor ? std::format("({}, {})", cursor->row, cursor->col) : std::string("none")
            );
        }
        return true;
    }
};


//  Confirm Choices after the name "1" was typed and submitted (the console's
//  frames of 2026-11-15): its highlight, the name, and for the languages
//  English OCR reads, a part of the language's name.
struct ConfirmCase{
    std::string name;
    struct Input{
        std::string file;   //  Under the test images' PokemonLZA/.
    };
    Input input;
    struct Expectation{
        std::string name;
        std::optional<std::string> language;
    };
    Expectation expectation;
};

const ConfirmCase CONFIRM_CASES[] = {
    {.name = "English", .input = {.file = "AutoStoryNewGame/2026-11-15/en_confirm.png"}, .expectation = {.name = "1", .language = "English"}},
    {.name = "Espanol (Espana)", .input = {.file = "AutoStoryNewGame/2026-11-15/esES_confirm.png"}, .expectation = {.name = "1", .language = "Espa"}},
    {.name = "Espanol (Latinoamerica)", .input = {.file = "AutoStoryNewGame/2026-11-15/esLA_confirm.png"}, .expectation = {.name = "1", .language = "Latino"}},
    {.name = "Francais", .input = {.file = "AutoStoryNewGame/2026-11-15/fr_confirm.png"}, .expectation = {.name = "1", .language = "Fran"}},
    {.name = "Deutsch", .input = {.file = "AutoStoryNewGame/2026-11-15/de_confirm.png"}, .expectation = {.name = "1", .language = "Deutsch"}},
    {.name = "Italiano", .input = {.file = "AutoStoryNewGame/2026-11-15/it_confirm.png"}, .expectation = {.name = "1", .language = "Italiano"}},
    {.name = "Japanese", .input = {.file = "AutoStoryNewGame/2026-11-15/ja_confirm.png"}, .expectation = {.name = "1", .language = std::nullopt}},
    {.name = "Korean", .input = {.file = "AutoStoryNewGame/2026-11-15/ko_confirm.png"}, .expectation = {.name = "1", .language = std::nullopt}},
    {.name = "Simplified Chinese", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHans_confirm.png"}, .expectation = {.name = "1", .language = std::nullopt}},
    {.name = "Traditional Chinese", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHant_confirm.png"}, .expectation = {.name = "1", .language = std::nullopt}},
    {.name = "German, one + after the name", .input = {.file = "AutoStoryNewGame/2026-11-15/de_plus0.png"}, .expectation = {.name = "1", .language = "Deutsch"}},
    {.name = "Traditional Chinese, two + after the name", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHant_plus1.png"}, .expectation = {.name = "1", .language = std::nullopt}},
};

class Test_ConfirmChoicesFrame : public ImageUnitTest{
public:
    explicit Test_ConfirmChoicesFrame(const ConfirmCase& test)
        : ImageUnitTest(
            std::format("PokemonLZA::AutoStory - intro screens: Confirm Choices in {}", test.name),
            {std::format("PokemonLZA/{}", test.input.file)}
        )
        , m_test(test)
    {}
    virtual UnitTestResult run_on_images(Logger&, CancellableScope&, const std::vector<ImageRGB32>& images) const override{
        const ImageRGB32& frame = images[0];
        if (!read_confirm_choices_cursor(frame)){
            return std::string("Confirm Choices' highlight did not read.");
        }
        if (is_name_keyboard(frame)){
            return std::string("Read as the name keyboard.");
        }
        const std::string name = read_confirm_choices_name(frame);
        if (name != m_test.expectation.name){
            return std::format("Expected the name \"{}\", read \"{}\".", m_test.expectation.name, name);
        }
        if (m_test.expectation.language){
            const std::string language = read_confirm_choices_language(frame, Language::English);
            if (language.find(*m_test.expectation.language) == std::string::npos){
                return std::format("Expected the language to have \"{}\", read \"{}\".", *m_test.expectation.language, language);
            }
        }
        return true;
    }

private:
    const ConfirmCase& m_test;
};

} // namespace


void add_tests_AutoStoryIntroScreens(UnitTestDatabase& database){
    for (const FrameCase& test : FRAME_CASES){
        database.add<Test_IntroScreenFrame>(test);
    }
    database.add<Test_ConfirmChoicesProfile>();
    for (const SystemKeyboardCase& test : SYSTEM_KEYBOARD_CASES){
        database.add<Test_SystemKeyboardOnNameKeyboard>(test);
    }
    for (const ConfirmCase& test : CONFIRM_CASES){
        database.add<Test_ConfirmChoicesFrame>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
