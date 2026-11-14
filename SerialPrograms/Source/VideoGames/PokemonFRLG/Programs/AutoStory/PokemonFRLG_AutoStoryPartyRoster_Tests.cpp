/*  AutoStory Party Roster Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Counts the screens a species lookup and a field move's user scan go
 *  through, on a fake party.
 *
 */

#include <format>
#include <optional>
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
        , m_menus(m_species.size())
    {}
    //  Each member's menu: the field moves it lists, nothing if unread.
    FakeParty(std::vector<std::string> species, std::vector<std::optional<PartyMemberMenu>> menus)
        : m_species(std::move(species))
        , m_menus(std::move(menus))
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
        m_menu_opens++;
        return true;
    }
    virtual std::optional<PartyMemberMenu> read_member_menu() override{
        m_menu_reads++;
        return m_menu_slot < m_menus.size() ? m_menus[m_menu_slot] : std::nullopt;
    }
    virtual bool close_member_menu() override{
        m_menu_closes++;
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
    std::vector<std::optional<PartyMemberMenu>> m_menus;
    size_t m_menu_opens = 0;
    size_t m_menu_reads = 0;
    size_t m_menu_closes = 0;
};

//  A menu listing `moves` in blue.
PartyMemberMenu menu_with(std::vector<std::string> moves){
    return PartyMemberMenu{.field_moves = std::move(moves)};
}
const PartyMemberMenu NO_FIELD_MOVES = {};

//  The story's party: the Pidgey third, with Fly; the Oddish with Cut.
FakeParty fly_party(){
    return FakeParty(
        {"blastoise", "oddish", "pidgey", "eevee"},
        {NO_FIELD_MOVES, menu_with({"cut", "flash"}), menu_with({"fly"}), NO_FIELD_MOVES}
    );
}

//  Party opens, members' menus opened and closed (B), summaries entered.
UnitTestResult check_menus(
    const std::string& what, const FakeParty& party,
    size_t party_opens, size_t menu_opens, size_t menu_closes, size_t closes
){
    if (party.m_party_opens != party_opens || party.m_menu_opens != menu_opens
        || party.m_menu_closes != menu_closes || party.m_closes != closes || party.m_summary_opens != 0
    ){
        return std::format(
            "{}: {} party opens, {} member menus opened, {} closed with B, {} party closes, {} summaries; "
            "expected {}, {}, {}, {}, 0.",
            what, party.m_party_opens, party.m_menu_opens, party.m_menu_closes, party.m_closes,
            party.m_summary_opens, party_opens, menu_opens, menu_closes, closes
        );
    }
    return true;
}

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

//  A Fly from the member's menu: it works on the slot that holds `holder`,
//  from FLY's row `row`.
FieldMoveFromMenu fly_from(const FakeParty& party, size_t holder, size_t row, size_t& calls){
    return [&party, holder, row, &calls](size_t picked) -> std::string{
        calls++;
        return party.m_menu_slot == holder && picked == row ? "" : "fly_no_map";
    };
}

//  The first Fly: the party opened once, the menus down to the Pidgey's,
//  FLY picked there; no summary.
UnitTestResult test_scan_stops_at_first_user(Logger& logger){
    FakeParty party = fly_party();
    PartyCache cache;
    size_t calls = 0;
    const std::string failure = use_field_move(logger, party, cache, "fly", "pidgey", fly_from(party, 2, 1, calls));
    if (!failure.empty() || calls != 1){
        return std::format("the first Fly: failure '{}', {} menu uses.", failure, calls);
    }
    const std::optional<FieldMoveUser> user = cache.users().find("fly");
    if (!user || user->slot != 2 || user->species != "pidgey"){
        return "the scan didn't record the Pidgey at slot 3.";
    }
    return check_menus("first fly", party, 1, 3, 2, 0);
}

//  FLY under another field move is picked from its own row.
UnitTestResult test_fly_row_from_menu(Logger& logger){
    FakeParty party({"pidgeot"}, {menu_with({"teleport", "fly"})});
    PartyCache cache;
    size_t calls = 0;
    const std::string failure = use_field_move(logger, party, cache, "fly", "pidgeot", fly_from(party, 0, 2, calls));
    if (!failure.empty() || calls != 1){
        return std::format("FLY on the menu's third row: failure '{}', {} menu uses.", failure, calls);
    }
    return true;
}

//  A recorded Fly user: one party open, its menu only.
UnitTestResult test_recorded_user_reads_no_summary(Logger& logger){
    FakeParty party = fly_party();
    PartyCache cache;
    cache.users().record("fly", 2, "pidgey");
    size_t calls = 0;
    const std::string failure = use_field_move(logger, party, cache, "fly", "pidgey", fly_from(party, 2, 1, calls));
    if (!failure.empty() || calls != 1){
        return std::format("a recorded Fly: failure '{}', {} menu uses.", failure, calls);
    }
    return check_menus("recorded user", party, 1, 1, 0, 0);
}

//  The record names a slot whose menu has no FLY now: B, and the scan runs
//  in the same party menu; the record is updated.
UnitTestResult test_stale_record_scans_and_updates(Logger& logger){
    FakeParty party = fly_party();
    PartyCache cache;
    cache.users().record("fly", 0, "pidgey");
    size_t calls = 0;
    const std::string failure = use_field_move(logger, party, cache, "fly", "pidgey", fly_from(party, 2, 1, calls));
    if (!failure.empty() || calls != 1){
        return std::format("a stale Fly record: failure '{}', {} menu uses.", failure, calls);
    }
    const std::optional<FieldMoveUser> user = cache.users().find("fly");
    if (!user || user->slot != 2){
        return "the stale record isn't replaced by the slot the scan found.";
    }
    //  Slot 1 (the record), then 2 and 3; slot 1 isn't opened twice.
    return check_menus("stale record", party, 1, 3, 2, 0);
}

//  A record of another species (the Pidgey evolved) is checked by its menu.
UnitTestResult test_other_species_record_checked_by_menu(Logger& logger){
    FakeParty party({"blastoise", "pidgeot"}, {NO_FIELD_MOVES, menu_with({"fly"})});
    PartyCache cache;
    cache.users().record("fly", 1, "pidgey");
    size_t calls = 0;
    const std::string failure = use_field_move(logger, party, cache, "fly", "pidgeot", fly_from(party, 1, 1, calls));
    if (!failure.empty() || calls != 1){
        return std::format("a record of another species: failure '{}', {} menu uses.", failure, calls);
    }
    return check_menus("other species", party, 1, 1, 0, 0);
}

//  No record: the scan runs once and records, so the next Fly opens one menu.
UnitTestResult test_scan_records_for_next_fly(Logger& logger){
    FakeParty party = fly_party();
    PartyCache cache;
    size_t calls = 0;
    use_field_move(logger, party, cache, "fly", "pidgey", fly_from(party, 2, 1, calls));
    FakeParty again = fly_party();
    const std::string failure = use_field_move(logger, again, cache, "fly", "pidgey", fly_from(again, 2, 1, calls));
    if (!failure.empty()){
        return std::format("the second Fly: {}", failure);
    }
    return check_menus("second fly", again, 1, 1, 0, 0);
}

//  No menu lists FLY: every member's menu opened once, the party closed.
UnitTestResult test_no_member_failure(Logger& logger){
    FakeParty party({"blastoise", "oddish"}, {NO_FIELD_MOVES, menu_with({"cut"})});
    PartyCache cache;
    size_t calls = 0;
    if (use_field_move(logger, party, cache, "fly", "pidgey", fly_from(party, 0, 1, calls)) != FIELD_MOVE_NO_MEMBER || calls != 0){
        return "a party without a Fly user should fail with no member and not fly.";
    }
    return check_menus("no member", party, 1, 2, 2, 1);
}

//  A menu not read is closed with B and the scan goes on: a later member
//  that lists FLY flies.
UnitTestResult test_unread_menu_skipped(Logger& logger){
    FakeParty party({"blastoise", "pidgey"}, {std::nullopt, menu_with({"fly"})});
    PartyCache cache;
    size_t calls = 0;
    const std::string failure = use_field_move(logger, party, cache, "fly", "pidgey", fly_from(party, 1, 1, calls));
    if (!failure.empty() || calls != 1){
        return std::format("past an unread menu: failure '{}', {} menu uses.", failure, calls);
    }
    return check_menus("unread menu", party, 1, 2, 1, 0);
}

//  A blue row whose name isn't read may be FLY, but a later member lists
//  it: that one flies (the console's Oddish, 12.03).
UnitTestResult test_unread_field_move_skipped(Logger& logger){
    FakeParty party({"oddish", "pidgey"}, {menu_with({"cut", ""}), menu_with({"fly"})});
    PartyCache cache;
    size_t calls = 0;
    const std::string failure = use_field_move(logger, party, cache, "fly", "pidgey", fly_from(party, 1, 1, calls));
    if (!failure.empty() || calls != 1){
        return std::format("past an unread field move: failure '{}', {} menu uses.", failure, calls);
    }
    return check_menus("unread field move", party, 1, 2, 1, 0);
}

//  No menu read lists FLY but one wasn't read: unread, not "no member",
//  and the record is kept.
UnitTestResult test_unread_menu_without_fly_fails(Logger& logger){
    FakeParty party({"blastoise", "oddish"}, {std::nullopt, menu_with({"cut"})});
    PartyCache cache;
    cache.users().record("fly", 0, "pidgey");
    size_t calls = 0;
    if (use_field_move(logger, party, cache, "fly", "pidgey", fly_from(party, 0, 1, calls)) != FIELD_MOVE_MENU_UNREAD || calls != 0){
        return "a party with an unread menu and no FLY should fail as unread and not fly.";
    }
    if (!cache.users().find("fly")){
        return "an unread menu dropped the Fly record.";
    }
    return check_menus("unread menu without fly", party, 1, 2, 2, 1);
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
//  the party once and only the recorded user's menu.
UnitTestResult test_restart_fly_reads_no_summary(Logger& logger){
    FieldMoveUsers before;
    before.record("fly", 2, "pidgey");
    PartyCache cache;
    cache.users() = parse_field_move_users(before.to_text()).users;
    FakeParty party = fly_party();
    size_t calls = 0;
    const std::string failure = use_field_move(logger, party, cache, "fly", "pidgey", fly_from(party, 2, 1, calls));
    if (!failure.empty() || calls != 1){
        return std::format("the Fly after a restart: failure '{}', {} menu uses.", failure, calls);
    }
    return check_menus("restart", party, 1, 1, 0, 0);
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
    {.input = {"a Fly scan stops at the first menu with FLY", test_scan_stops_at_first_user}, .expectation = {}},
    {.input = {"FLY is picked from its row on the menu", test_fly_row_from_menu}, .expectation = {}},
    {.input = {"a recorded field move user opens only its menu", test_recorded_user_reads_no_summary}, .expectation = {}},
    {.input = {"a stale record falls back to the scan and is updated", test_stale_record_scans_and_updates}, .expectation = {}},
    {.input = {"a record of another species is checked by its menu", test_other_species_record_checked_by_menu}, .expectation = {}},
    {.input = {"a scan records the user for the next Fly", test_scan_records_for_next_fly}, .expectation = {}},
    {.input = {"invalidate() drops the field move users", test_invalidate_drops_records}, .expectation = {}},
    {.input = {"no menu with FLY fails without flying", test_no_member_failure}, .expectation = {}},
    {.input = {"an unread member menu is skipped for a later Fly user", test_unread_menu_skipped}, .expectation = {}},
    {.input = {"an unread field move name is skipped for a later Fly user", test_unread_field_move_skipped}, .expectation = {}},
    {.input = {"an unread menu and no Fly user fails as unread", test_unread_menu_without_fly_fails}, .expectation = {}},
    {.input = {"the field move users' text reads back the same", test_users_text_round_trip}, .expectation = {}},
    {.input = {"unreadable field move users' text is dropped", test_unreadable_users_text_dropped}, .expectation = {}},
    {.input = {"a Fly after a restart opens only the recorded menu", test_restart_fly_reads_no_summary}, .expectation = {}},
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
