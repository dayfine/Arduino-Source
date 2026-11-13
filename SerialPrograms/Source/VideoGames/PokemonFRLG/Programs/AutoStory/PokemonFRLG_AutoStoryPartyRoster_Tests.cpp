/*  AutoStory Party Roster Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Counts the screens a species lookup goes through, on a fake party.
 *
 */

#include <format>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryPartyRoster.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  A party that counts what is opened and read.
class FakeParty : public PartyScreens{
public:
    explicit FakeParty(std::vector<std::string> species)
        : m_species(std::move(species))
    {}

    virtual bool open_party(size_t& members) override{
        members = m_species.size();
        m_party_opens++;
        return true;
    }
    virtual bool enter_summary() override{
        m_summary_opens++;
        m_shown = 0;
        return true;
    }
    virtual std::string read_species() override{
        m_reads++;
        return m_species[m_shown];
    }
    virtual bool next_member() override{
        //  The game's DOWN does nothing on the last member.
        if (m_shown + 1 < m_species.size()){
            m_shown++;
        }
        return true;
    }
    virtual bool leave_summary() override{
        m_menu_slot = m_shown;
        return true;
    }
    virtual bool open_member_menu(size_t slot) override{
        m_menu_slot = slot;
        return true;
    }
    virtual void close() override{
        m_closes++;
        m_menu_slot = SIZE_MAX;
    }

    std::vector<std::string> m_species;
    size_t m_shown = 0;
    size_t m_menu_slot = SIZE_MAX;
    size_t m_party_opens = 0;
    size_t m_summary_opens = 0;
    size_t m_reads = 0;
    size_t m_closes = 0;
};

UnitTestResult check_counts(
    const std::string& what, const FakeParty& party,
    size_t party_opens, size_t summary_opens, size_t reads, size_t closes
){
    if (party.m_party_opens != party_opens || party.m_summary_opens != summary_opens
        || party.m_reads != reads || party.m_closes != closes
    ){
        return std::format(
            "{}: {} party opens, {} summary entries, {} summary reads, {} closes; expected {}, {}, {}, {}.",
            what, party.m_party_opens, party.m_summary_opens, party.m_reads, party.m_closes,
            party_opens, summary_opens, reads, closes
        );
    }
    return true;
}

//  A Fly naming the Pidgey: one party open, the summaries up to the
//  Pidgey read once, the member's menu left open for FLY.
UnitTestResult test_first_lookup_opens_party_once(Logger& logger){
    FakeParty party({"blastoise", "oddish", "pidgey", "eevee"});
    PartyCache cache;
    const std::optional<size_t> slot = open_member_menu_of(logger, party, cache, "pidgey", true);
    if (slot != size_t(2) || party.m_menu_slot != 2){
        return "the Pidgey isn't found at slot 3 with its menu open.";
    }
    return check_counts("first lookup", party, 1, 1, 3, 0);
}

//  The second Fly in the checkpoint: one party open and no summary read.
UnitTestResult test_second_lookup_reads_nothing(Logger& logger){
    FakeParty party({"blastoise", "oddish", "pidgey", "eevee"});
    PartyCache cache;
    open_member_menu_of(logger, party, cache, "pidgey", true);
    FakeParty again({"blastoise", "oddish", "pidgey", "eevee"});
    const std::optional<size_t> slot = open_member_menu_of(logger, again, cache, "pidgey", true);
    if (slot != size_t(2) || again.m_menu_slot != 2){
        return "the cached Pidgey isn't found at slot 3 with its menu open.";
    }
    return check_counts("second lookup", again, 1, 0, 0, 0);
}

//  A member the scan passed is cached too; one it never reached is read for.
UnitTestResult test_cache_holds_what_was_read(Logger& logger){
    FakeParty party({"blastoise", "oddish", "pidgey", "eevee"});
    PartyCache cache;
    open_member_menu_of(logger, party, cache, "pidgey", true);
    FakeParty second({"blastoise", "oddish", "pidgey", "eevee"});
    if (open_member_menu_of(logger, second, cache, "oddish", true) != size_t(1)){
        return "the Oddish, read on the way to the Pidgey, isn't found.";
    }
    if (UnitTestResult result = check_counts("cached passed-by member", second, 1, 0, 0, 0); result.result != UnitTestResult::PASSED){
        return result;
    }
    FakeParty third({"blastoise", "oddish", "pidgey", "eevee"});
    if (open_member_menu_of(logger, third, cache, "eevee", true) != size_t(3)){
        return "the Eevee, past the cache, isn't found.";
    }
    return check_counts("species past the cache", third, 1, 1, 4, 0);
}

//  A changed party: invalidate() makes the next lookup read again.
UnitTestResult test_invalidate_reads_again(Logger& logger){
    FakeParty party({"blastoise", "oddish", "pidgey"});
    PartyCache cache;
    open_member_menu_of(logger, party, cache, "pidgey", true);
    cache.invalidate();
    FakeParty caught({"blastoise", "pidgey", "oddish"});
    if (open_member_menu_of(logger, caught, cache, "pidgey", true) != size_t(1)){
        return "after invalidate(), the Pidgey isn't found at its new slot 2.";
    }
    return check_counts("after invalidate", caught, 1, 1, 2, 0);
}

//  No such member: the scan stops at the party's size (DOWN doesn't wrap)
//  and the party is closed.
UnitTestResult test_missing_species_closes(Logger& logger){
    FakeParty party({"blastoise", "oddish"});
    PartyCache cache;
    if (open_member_menu_of(logger, party, cache, "pidgey", true).has_value()){
        return "found a Pidgey in a party without one.";
    }
    return check_counts("missing species", party, 1, 1, 2, 1);
}

//  The first of two of a species wins, and the scan stops there.
UnitTestResult test_first_duplicate_wins(Logger& logger){
    FakeParty party({"pidgey", "oddish", "pidgey"});
    PartyCache cache;
    if (open_member_menu_of(logger, party, cache, "pidgey", true) != size_t(0)){
        return "the first Pidgey isn't the one found.";
    }
    return check_counts("duplicates", party, 1, 1, 1, 0);
}

//  Teach and the like want the overworld back: the menu is closed after.
UnitTestResult test_closes_when_not_kept(Logger& logger){
    FakeParty party({"blastoise", "pidgey"});
    PartyCache cache;
    if (open_member_menu_of(logger, party, cache, "pidgey", false) != size_t(1)){
        return "the Pidgey isn't found.";
    }
    return check_counts("closing lookup", party, 1, 1, 2, 1);
}

//  The slot alone, for a TEACH, from the cache: nothing opened at all.
UnitTestResult test_cached_slot_opens_nothing(Logger& logger){
    FakeParty party({"blastoise", "pidgey"});
    PartyCache cache;
    open_member_menu_of(logger, party, cache, "pidgey", true);
    FakeParty again({"blastoise", "pidgey"});
    if (open_member_menu_of(logger, again, cache, "pidgey", false) != size_t(1)){
        return "the cached Pidgey isn't found.";
    }
    return check_counts("cached slot only", again, 0, 0, 0, 0);
}

struct AutoStoryPartyRosterInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct AutoStoryPartyRosterExpectation{
};

struct AutoStoryPartyRosterCase{
    AutoStoryPartyRosterInput input;
    AutoStoryPartyRosterExpectation expectation;
};

class Test_AutoStoryPartyRoster : public UnitTest{
public:
    explicit Test_AutoStoryPartyRoster(AutoStoryPartyRosterCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Party roster: {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body(logger);
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    AutoStoryPartyRosterCase m_case;
};

const AutoStoryPartyRosterCase AUTO_STORY_PARTY_ROSTER_CASES[] = {
    {.input = {"a Fly by species opens the party once", test_first_lookup_opens_party_once}, .expectation = {}},
    {.input = {"the second Fly reads no summary", test_second_lookup_reads_nothing}, .expectation = {}},
    {.input = {"the cache holds the members read on the way", test_cache_holds_what_was_read}, .expectation = {}},
    {.input = {"invalidate() reads the party again", test_invalidate_reads_again}, .expectation = {}},
    {.input = {"a missing species closes the party", test_missing_species_closes}, .expectation = {}},
    {.input = {"the first of two of a species wins", test_first_duplicate_wins}, .expectation = {}},
    {.input = {"a lookup that doesn't keep the menu closes it", test_closes_when_not_kept}, .expectation = {}},
    {.input = {"a cached slot for a TEACH opens nothing", test_cached_slot_opens_nothing}, .expectation = {}},
};

} // namespace


void add_tests_AutoStoryPartyRoster(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_PARTY_ROSTER_CASES){
        database.add<Test_AutoStoryPartyRoster>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
