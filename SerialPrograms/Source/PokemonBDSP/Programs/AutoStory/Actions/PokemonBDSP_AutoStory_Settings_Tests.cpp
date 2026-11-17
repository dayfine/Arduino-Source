/*  BDSP AutoStory - Settings Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonBDSP_AutoStory_Settings.h"
#include "PokemonBDSP_AutoStory_Settings_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using detail::OptionsLayout;
using detail::options_layout;
using detail::options_value_column;


struct OptionsLayoutExpectation{
    size_t rows = 0;
    bool label_images = false;
    size_t autosave_row = 0;
    std::string_view second_label;
    std::string_view second_value;
};
struct OptionsLayoutCase{
    std::string name;
    Language input = Language::None;
    OptionsLayoutExpectation expectation;
};

//  The Options rows for a game language: how many, whether they have label
//  images, where Autosave is and what the second row is.
class Test_OptionsLayout : public UnitTest{
public:
    explicit Test_OptionsLayout(OptionsLayoutCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Options rows: " + test_case.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const OptionsLayout layout = options_layout(m_case.input);
        size_t autosave = layout.rows.size();
        for (size_t row = 0; row < layout.rows.size(); row++){
            if (layout.rows[row].label == "autosave"){
                autosave = row;
            }
        }
        const OptionsLayoutExpectation& e = m_case.expectation;
        if (layout.rows.size() < 2){
            return std::format("{} rows.", layout.rows.size());
        }
        if (layout.rows.size() != e.rows || layout.label_images != e.label_images || autosave != e.autosave_row ||
            layout.rows[1].label != e.second_label || layout.rows[1].value != e.second_value){
            return std::format("{} rows, label images {}, Autosave on row {}, second row {}={}.",
                layout.rows.size(), layout.label_images, autosave, layout.rows[1].label, layout.rows[1].value);
        }
        return true;
    }

private:
    OptionsLayoutCase m_case;
};

//  Japanese has the Kana/Kanji row under Text Speed (console B, 2026-11-15:
//  "Unable to confirm expected row textspeed" on the English label image),
//  which the story leaves as it is; Autosave is one row further down.
const OptionsLayoutCase OPTIONS_LAYOUT_CASES[] = {
    {
        .name = "English: label images",
        .input = Language::English,
        .expectation = {.rows = 11, .label_images = true, .autosave_row = 7, .second_label = "changewindow", .second_value = "type1"},
    },
    {
        .name = "Japanese: the Kana/Kanji row, left as it is",
        .input = Language::Japanese,
        .expectation = {.rows = 12, .label_images = false, .autosave_row = 8, .second_label = "mojimode", .second_value = ""},
    },
    {
        .name = "French: the English rows, no label images",
        .input = Language::French,
        .expectation = {.rows = 11, .label_images = false, .autosave_row = 7, .second_label = "changewindow", .second_value = "type1"},
    },
};


struct ValueColumnInput{
    double x = 0;
    size_t columns = 0;
};
struct ValueColumnCase{
    std::string name;
    ValueColumnInput input;
    std::optional<size_t> expectation;
};

//  The selected value's column from the centre of its red ink.
class Test_OptionsValueColumn : public UnitTest{
public:
    explicit Test_OptionsValueColumn(ValueColumnCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Options value column: " + test_case.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::optional<size_t> column = options_value_column(m_case.input.x, m_case.input.columns);
        if (column != m_case.expectation){
            return std::format("column {}.", column ? std::to_string(*column) : "none");
        }
        return true;
    }

private:
    ValueColumnCase m_case;
};

//  Centres measured on the Japanese failure frame (console B, 2026-11-15)
//  and on the English value images.
const ValueColumnCase OPTIONS_VALUE_COLUMN_CASES[] = {
    {.name = "Japanese Text Speed: Slow", .input = {.x = 0.38, .columns = 3}, .expectation = 0},
    {.name = "Japanese Text Speed: Normal", .input = {.x = 0.56, .columns = 3}, .expectation = 1},
    {.name = "English Text Speed: Fast", .input = {.x = 0.73, .columns = 3}, .expectation = 2},
    {.name = "Japanese Send to Boxes: pick yourself", .input = {.x = 0.42, .columns = 2}, .expectation = 0},
    {.name = "English Send to Boxes: Automatic", .input = {.x = 0.685, .columns = 2}, .expectation = 1},
    {.name = "English Gyroscope: Use", .input = {.x = 0.361, .columns = 2}, .expectation = 0},
    {.name = "left of the values", .input = {.x = 0.20, .columns = 2}, .expectation = std::nullopt},
    {.name = "right of the values", .input = {.x = 0.90, .columns = 3}, .expectation = std::nullopt},
    {.name = "no columns", .input = {.x = 0.50, .columns = 0}, .expectation = std::nullopt},
};

} // namespace


void add_tests_AutoStory_Settings(UnitTestDatabase& database){
    for (const OptionsLayoutCase& test_case : OPTIONS_LAYOUT_CASES){
        database.add<Test_OptionsLayout>(test_case);
    }
    for (const ValueColumnCase& test_case : OPTIONS_VALUE_COLUMN_CASES){
        database.add<Test_OptionsValueColumn>(test_case);
    }
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
