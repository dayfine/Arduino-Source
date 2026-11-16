/*  AutoStory Lead Out Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Tests/ImageUnitTest.h"
#include "Tests/TestUtils.h"
#include "PokemonFRLG_AutoStoryLeadOut.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The lead at a trainer's battle menu, by the one rule (lead_on_field()).
struct LeadOnFieldCase{
    std::string_view name;
    LeadOnFieldInput input;
    struct Expectation{
        LeadOnField lead;
    } expectation;
};

const LeadOnFieldCase LEAD_ON_FIELD_CASES[] = {
    {
        "Bruno's Onix's Roar (console B, 2026-11-16): Lapras out, box gone, none sent",
        {.box_gone = true, .member_sent = false, .own_out = "lapras", .lead = "blastoise"},
        {LeadOnField::FORCED_OUT},
    },
    {
        "a Roar the box watch missed",
        {.box_gone = false, .member_sent = false, .own_out = "lapras", .lead = "blastoise"},
        {LeadOnField::FORCED_OUT},
    },
    {
        "a real faint: a member sent from the party list",
        {.box_gone = true, .member_sent = true, .own_out = "lapras", .lead = "blastoise"},
        {LeadOnField::FAINTED},
    },
    {
        "the lead read out: its box only hid in an animation",
        {.box_gone = true, .member_sent = false, .own_out = "blastoise", .lead = "blastoise"},
        {LeadOnField::OUT},
    },
    {
        "nothing read, box gone: fainted",
        {.box_gone = true, .member_sent = false, .own_out = "", .lead = "blastoise"},
        {LeadOnField::FAINTED},
    },
};

class Test_LeadOnField : public UnitTest{
public:
    explicit Test_LeadOnField(const LeadOnFieldCase& test)
        : UnitTest(std::format("PokemonFRLG::AutoStoryLeadOut - {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        if (lead_on_field(m_test.input) != m_test.expectation.lead){
            return "Unexpected verdict.";
        }
        return true;
    }

private:
    LeadOnFieldCase m_test;
};

//  read_own_battler() on console B's battle menus named
//  <anything>_<species>.png, in English.
class Test_OwnBattler : public ImageUnitTest{
public:
    Test_OwnBattler(const std::string& image)
        : ImageUnitTest(std::format("PokemonFRLG::AutoStoryBattle own battler - {}", image), {image})
    {
        m_memory += PADDLE_OCR_SINGLE_LINE_MEMORY_ESTIMATE;
    }

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const std::vector<std::string> words = parse_words(Filesystem::Path(image_paths()[0]).stem().string());
        if (words.empty()){
            return "Error: filename must be <anything>_<species>.";
        }
        const std::string read = read_own_battler(logger, Language::English, images[0]);
        return read == words.back() ? UnitTestResult(true) : UnitTestResult(std::format("Read \"{}\", expected {}.", read, words.back()));
    }
};

//  party_slot_of() on a party list: `species` at `slot` (from 0).
class Test_PartySlotOf : public ImageUnitTest{
public:
    Test_PartySlotOf(const std::string& image, std::string species, size_t slot)
        : ImageUnitTest(std::format("PokemonFRLG::AutoStoryBattle party slot of {} - {}", species, image), {image})
        , m_species(std::move(species))
        , m_slot(slot)
    {
        m_memory += PADDLE_OCR_SINGLE_LINE_MEMORY_ESTIMATE;
    }

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const std::optional<size_t> slot = party_slot_of(logger, Language::English, images[0], m_species);
        return slot == std::optional<size_t>(m_slot)
            ? UnitTestResult(true)
            : UnitTestResult(std::format("{}: slot {}, expected {}.", m_species, slot ? std::format("{}", *slot) : "none", m_slot));
    }

private:
    std::string m_species;
    size_t m_slot;
};

} // namespace


void add_tests_AutoStoryLeadOut(UnitTestDatabase& database){
    for (const LeadOnFieldCase& test : LEAD_ON_FIELD_CASES){
        database.add<Test_LeadOnField>(test);
    }
    //  Bruno's Onix's Roar (console B, 2026-11-16, 20.02): Lapras out; the turn before, Blastoise.
    database.add<Test_OwnBattler>("PokemonFRLG/AutoStoryBattle/BrunoOnixRoar_OwnBattler_lapras.png");
    database.add<Test_OwnBattler>("PokemonFRLG/AutoStoryBattle/BrunoOnix_OwnBattler_blastoise.png");
    database.add<Test_PartySlotOf>("PokemonFRLG/Party/Party_6.png", "blastoise", /*slot=*/4);
    database.add<Test_PartySlotOf>("PokemonFRLG/Party/Party_6.png", "raichu", /*slot=*/0);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
