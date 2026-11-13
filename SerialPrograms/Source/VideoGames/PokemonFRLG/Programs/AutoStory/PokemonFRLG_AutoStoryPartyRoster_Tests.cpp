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

//  A Fly from the member's menu: it works on the slot that holds `holder`.
FieldMoveFromMenu fly_from(const FakeParty& party, size_t holder, size_t& calls){
    return [&party, holder, &calls]() -> std::string{
        calls++;
        return party.m_menu_slot == holder ? "" : "fly_no_map";
    };
}

//  A recorded Fly user: the party opens once and no summary is read.
UnitTestResult test_recorded_user_reads_no_summary(Logger& logger){
    FakeParty party({"blastoise", "pidgey", "oddish"});
    PartyCache cache;
    cache.users().record("fly", 1, "pidgey");
    size_t calls = 0;
    const std::string failure = use_field_move(logger, party, cache, "fly", "pidgey", fly_from(party, 1, calls));
    if (!failure.empty() || calls != 1){
        return std::format("a recorded Fly: failure '{}', {} menu uses.", failure, calls);
    }
    return check_counts("recorded user", party, 1, 0, 0, 0);
}

//  The record names a slot another member holds now: the menu fails, the
//  party is closed, the scan finds the right slot and the record is updated.
UnitTestResult test_stale_record_scans_and_updates(Logger& logger){
    FakeParty party({"blastoise", "oddish", "pidgey"});
    PartyCache cache;
    cache.users().record("fly", 0, "pidgey");
    size_t calls = 0;
    const std::string failure = use_field_move(logger, party, cache, "fly", "pidgey", fly_from(party, 2, calls));
    if (!failure.empty() || calls != 2){
        return std::format("a stale Fly record: failure '{}', {} menu uses.", failure, calls);
    }
    const std::optional<FieldMoveUser> user = cache.users().find("fly");
    if (!user || user->slot != 2){
        return "the stale record isn't replaced by the slot the scan found.";
    }
    return check_counts("stale record", party, 2, 1, 3, 1);
}

//  A record for another species isn't trusted: the scan runs.
UnitTestResult test_other_species_record_scans(Logger& logger){
    FakeParty party({"blastoise", "pidgeot"});
    PartyCache cache;
    cache.users().record("fly", 0, "pidgey");
    size_t calls = 0;
    const std::string failure = use_field_move(logger, party, cache, "fly", "pidgeot", fly_from(party, 1, calls));
    if (!failure.empty() || calls != 1){
        return std::format("a record of another species: failure '{}', {} menu uses.", failure, calls);
    }
    return check_counts("other species", party, 1, 1, 2, 0);
}

//  No record: the scan runs once and records, so the next Fly reads nothing.
UnitTestResult test_scan_records_for_next_fly(Logger& logger){
    FakeParty party({"blastoise", "pidgey"});
    PartyCache cache;
    size_t calls = 0;
    use_field_move(logger, party, cache, "fly", "pidgey", fly_from(party, 1, calls));
    FakeParty again({"blastoise", "pidgey"});
    cache.invalidate_species();
    const std::string failure = use_field_move(logger, again, cache, "fly", "pidgey", fly_from(again, 1, calls));
    if (!failure.empty()){
        return std::format("the second Fly: {}", failure);
    }
    return check_counts("second fly", again, 1, 0, 0, 0);
}

//  A deposit or withdrawal moves members: nothing is recorded after it.
UnitTestResult test_invalidate_drops_records(Logger&){
    PartyCache cache;
    cache.users().record("fly", 1, "pidgey");
    cache.invalidate();
    if (cache.users().find("fly")){
        return "invalidate() kept a field move user.";
    }
    return true;
}

//  No member of the species: its failure, the party closed.
UnitTestResult test_no_member_failure(Logger& logger){
    FakeParty party({"blastoise"});
    PartyCache cache;
    size_t calls = 0;
    if (use_field_move(logger, party, cache, "fly", "pidgey", fly_from(party, 0, calls)) != FIELD_MOVE_NO_MEMBER || calls != 0){
        return "a party without the species should fail with no member and not fly.";
    }
    return check_counts("no member", party, 1, 1, 1, 1);
}

//  The users as the settings keep them read back the same, slots from 1.
UnitTestResult test_users_text_round_trip(Logger&){
    FieldMoveUsers users;
    users.record("fly", 2, "pidgey");
    users.record("cut", 1, "");
    const std::string text = users.to_text();
    if (text != "cut=2:,fly=3:pidgey"){
        return std::format("the users' text is '{}', expected 'cut=2:,fly=3:pidgey'.", text);
    }
    const ParsedFieldMoveUsers parsed = parse_field_move_users(text);
    if (!(parsed.users == users) || !parsed.rejected.empty()){
        return std::format("'{}' doesn't read back as the users it came from.", text);
    }
    return true;
}

//  Text that isn't readable is dropped entry by entry, never thrown on:
//  those moves are found by a scan. Empty text is no users.
UnitTestResult test_unreadable_users_text_dropped(Logger&){
    const ParsedFieldMoveUsers parsed = parse_field_move_users(
        " strength = 1 : blastoise ,fly=x:pidgey,surf=7:lapras,=2:oddish,junk,cut=0:oddish,flash=2:Bad Name,,"
    );
    const std::optional<FieldMoveUser> strength = parsed.users.find("strength");
    if (!strength || strength->slot != 0 || strength->species != "blastoise"){
        return "the readable entry, strength=1:blastoise, isn't kept.";
    }
    if (parsed.users.find("fly") || parsed.users.find("surf") || parsed.users.find("cut") || parsed.users.find("flash")){
        return "an unreadable entry was kept.";
    }
    if (parsed.rejected.size() != 6){
        return std::format("{} entries rejected, expected 6.", parsed.rejected.size());
    }
    const ParsedFieldMoveUsers empty = parse_field_move_users("");
    if (!(empty.users == FieldMoveUsers{}) || !empty.rejected.empty()){
        return "empty text isn't empty users.";
    }
    return true;
}

//  After a restart, the users read from the settings: the first Fly opens
//  the party once and reads no summary.
UnitTestResult test_restart_fly_reads_no_summary(Logger& logger){
    FieldMoveUsers before;
    before.record("fly", 2, "pidgey");
    PartyCache cache;
    cache.users() = parse_field_move_users(before.to_text()).users;
    FakeParty party({"blastoise", "oddish", "pidgey"});
    size_t calls = 0;
    const std::string failure = use_field_move(logger, party, cache, "fly", "pidgey", fly_from(party, 2, calls));
    if (!failure.empty() || calls != 1){
        return std::format("the Fly after a restart: failure '{}', {} menu uses.", failure, calls);
    }
    return check_counts("restart", party, 1, 0, 0, 0);
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
    {.input = {"a recorded field move user reads no summary", test_recorded_user_reads_no_summary}, .expectation = {}},
    {.input = {"a stale record falls back to the scan and is updated", test_stale_record_scans_and_updates}, .expectation = {}},
    {.input = {"a record of another species is not trusted", test_other_species_record_scans}, .expectation = {}},
    {.input = {"a scan records the user for the next Fly", test_scan_records_for_next_fly}, .expectation = {}},
    {.input = {"invalidate() drops the field move users", test_invalidate_drops_records}, .expectation = {}},
    {.input = {"no member of the species fails without flying", test_no_member_failure}, .expectation = {}},
    {.input = {"the field move users' text reads back the same", test_users_text_round_trip}, .expectation = {}},
    {.input = {"unreadable field move users' text is dropped", test_unreadable_users_text_dropped}, .expectation = {}},
    {.input = {"a Fly after a restart reads no summary", test_restart_fly_reads_no_summary}, .expectation = {}},
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
