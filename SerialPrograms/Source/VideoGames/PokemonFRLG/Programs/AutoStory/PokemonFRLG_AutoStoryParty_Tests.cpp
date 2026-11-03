/*  AutoStory Party Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The lead's moves as the story tracks them, and the battle loop's choices
 *  (BattleDecisions) against scripted screens: move prompts, a fainted lead,
 *  PP, and no B while an evolution may be on screen.
 *
 */

#include <format>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryBattleDecisions.h"
#include "PokemonFRLG_AutoStoryParty.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{

class Test_AutoStoryParty : public UnitTest{
public:
    Test_AutoStoryParty(std::string name, std::function<UnitTestResult(Logger&)> body)
        : UnitTest("PokemonFRLG::AutoStoryParty - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::function<UnitTestResult(Logger&)> m_body;
};


std::vector<std::string> move_names(const PartyMember& member){
    std::vector<std::string> names;
    for (const KnownMove& known : member.moves){
        names.push_back(known.move);
    }
    return names;
}

std::string joined(const std::vector<std::string>& names){
    std::string text;
    for (const std::string& name : names){
        text += (text.empty() ? "" : ", ") + name;
    }
    return "[" + text + "]";
}

PartyMember with_moves(std::string species, std::vector<std::string> moves, uint8_t through){
    PartyMember member{.species = std::move(species), .moves = {}, .moves_through_level = through, .moves_read = false};
    for (std::string& move : moves){
        const uint8_t pp = max_pp(move);
        member.moves.emplace_back(KnownMove{.move = std::move(move), .pp = pp});
    }
    return member;
}

const PartyStatus GREEN{.hp_seen = true, .hp_yellow = false, .hp_red = false, .status = StatusCondition::NONE};


//  The moves the game gives, and the story's plan played to a level.
UnitTestResult test_movesets(Logger&){
    struct Case{
        std::string name;
        PartyMember member;
        std::string species;
        std::vector<std::string> moves;
    };
    const std::vector<Case> cases{
        {.name = "new game Squirtle", .member = new_member("squirtle", 5), .species = "squirtle", .moves = {"tackle", "tail-whip"}},
        {.name = "Squirtle after Brock", .member = story_lead_after_brock(), .species = "squirtle", .moves = {"tackle", "tail-whip", "water-gun", "withdraw"}},
        {.name = "Squirtle to Lv20", .member = grown_to(new_member("squirtle", 5), 20), .species = "wartortle", .moves = {"tackle", "bite", "water-gun", "withdraw"}},
        {.name = "Magikarp to Lv25", .member = grown_to(new_member("magikarp", 5), 25), .species = "gyarados", .moves = {"splash", "tackle", "bite", "dragon-rage"}},
    };
    for (const Case& c : cases){
        if (c.member.species != c.species || move_names(c.member) != c.moves){
            return std::format(
                "{}: expected {} {}, got {} {}.",
                c.name, c.species, joined(c.moves), c.member.species, joined(move_names(c.member))
            );
        }
    }
    return true;
}

//  "Delete a move?": the move offered is the next level-up move, after the
//  moves learned into free slots, and the plan says which slot it takes.
UnitTestResult test_learn_prompts(Logger&){
    struct Case{
        std::string name;
        PartyMember member;
        std::string offered;
        std::optional<size_t> forget_slot;
        std::vector<std::string> after;
    };
    const std::vector<Case> cases{
        {
            .name = "first prompt of a new game",
            .member = new_member("squirtle", 5),
            .offered = "water-gun", .forget_slot = BOTTOM_LEFT_MOVE,
            .after = {"tackle", "tail-whip", "water-gun", "withdraw"},
        },
        {
            .name = "Bite after Brock",
            .member = story_lead_after_brock(),
            .offered = "bite", .forget_slot = 1,
            .after = {"tackle", "bite", "water-gun", "withdraw"},
        },
        {
            //  The lab's save from before the story learned moves.
            .name = "Bite with Bubble kept",
            .member = with_moves("squirtle", {"tackle", "tail-whip", "bubble", "withdraw"}, 13),
            .offered = "bite", .forget_slot = 1,
            .after = {"tackle", "bite", "bubble", "withdraw"},
        },
        {
            .name = "Wartortle declines Rapid Spin",
            .member = with_moves("wartortle", {"tackle", "bite", "water-gun", "withdraw"}, 19),
            .offered = "rapid-spin", .forget_slot = std::nullopt,
            .after = {"tackle", "bite", "water-gun", "withdraw"},
        },
    };
    for (Case c : cases){
        const LearnDecision decision = learn_move(c.member);
        if (decision.move != c.offered || decision.forget_slot != c.forget_slot || move_names(c.member) != c.after){
            return std::format(
                "{}: expected {} over slot {}, leaving {}; got {} over slot {}, leaving {}.",
                c.name, c.offered, c.forget_slot ? std::to_string(*c.forget_slot) : "none", joined(c.after),
                decision.move.value_or("nothing"), decision.forget_slot ? std::to_string(*decision.forget_slot) : "none",
                joined(move_names(c.member))
            );
        }
    }
    return true;
}

//  NO to "Delete a move?" is followed by YES to "Stop learning?" (B there
//  asks "Delete a move?" again, console B 2026-11-01); a move the plan
//  wants is YES with the slot to forget.
UnitTestResult test_prompt_answers(Logger&){
    Supplies supplies;
    supplies.lead = with_moves("wartortle", {"tackle", "bite", "water-gun", "withdraw"}, 19);
    BattleDecisions decline(supplies, BattleKind::TRAINER);
    decline.turn(GREEN);
    const PromptAnswer first = decline.prompt(false);
    const PromptAnswer second = decline.prompt(false);
    if (first.answer != YesNo::NO || first.forget_slot || second.answer != YesNo::YES || second.forget_slot){
        return "Expected NO to \"Delete a move?\", then YES to \"Stop learning?\".";
    }

    Supplies learning;
    learning.lead = story_lead_after_brock();
    BattleDecisions learn(learning, BattleKind::WILD_FIGHT);
    learn.turn(GREEN);
    const PromptAnswer answer = learn.prompt(false);
    if (answer.answer != YesNo::YES || answer.forget_slot != std::optional<size_t>(1)){
        return "Expected YES and Tail Whip's slot (1) for Bite.";
    }
    return true;
}

//  The lead fainted: a wild battle is run from, and a member sent only
//  runs; in a trainer's, the next member is sent and uses its first move.
//  Protected members are never sent: with only those left there is no
//  slot, and the loop stops. Either way the
//  battle is lost, and no item goes to the fainted lead.
UnitTestResult test_fainted_lead(Logger&){
    Supplies protected_party;
    protected_party.protected_slots = {1, 2, 3, 4, 5};
    BattleDecisions only_protected(protected_party, BattleKind::WILD_FIGHT);
    only_protected.turn(GREEN);
    if (only_protected.prompt(true).answer != YesNo::NO || !only_protected.lost()){
        return "Expected NO to \"Use next POKEMON?\" in a wild battle, and the battle lost.";
    }
    if (only_protected.next_member_slot()){
        return "Expected no member sent with every member after the lead protected.";
    }

    //  Slot 1 protected, the rest not.
    Supplies wild_supplies;
    wild_supplies.potions = 3;
    wild_supplies.protected_slots = {1};
    BattleDecisions wild(wild_supplies, BattleKind::WILD_FIGHT);
    wild.turn(GREEN);
    wild.prompt(true);
    if (wild.next_member_slot() != std::optional<size_t>(2) || wild.next_member_slot() != std::optional<size_t>(3)){
        return "Expected slot 2 past the protected slot 1, then slot 3 if 2 couldn't go.";
    }
    const PartyStatus red{.hp_seen = true, .hp_yellow = false, .hp_red = true, .status = StatusCondition::NONE};
    for (int c = 0; c < 10; c++){
        if (wild.turn(red).choice != TurnChoice::RUN){
            return std::format("Expected the next member to run at turn {}, not fight or use an item.", c + 1);
        }
    }

    Supplies trainer_supplies;
    BattleDecisions trainer(trainer_supplies, BattleKind::TRAINER);
    trainer.turn(GREEN);
    if (trainer.next_member_slot() != std::optional<size_t>(1) || !trainer.lost()){
        return "Expected the member after the lead sent in a trainer's battle, and the battle lost.";
    }
    const TurnAction action = trainer.turn(GREEN);
    if (action.choice != TurnChoice::MOVE || action.slot != TOP_LEFT_MOVE){
        return "Expected the next member to use its first move against a trainer.";
    }
    return true;
}

//  Water Gun until its 25 PP are gone, then Tackle; a "no PP left" moves
//  on at once; the nurse restores all. Before the story knows four moves,
//  the bottom left slot, as before.
UnitTestResult test_pp(Logger&){
    Supplies supplies;
    supplies.lead = story_lead_after_brock();
    BattleDecisions decisions(supplies, BattleKind::TRAINER);
    for (int c = 0; c < 25; c++){
        const TurnAction action = decisions.turn(GREEN);
        if (action.choice != TurnChoice::MOVE || action.slot != BOTTOM_LEFT_MOVE){
            return std::format("Expected Water Gun (slot 2) at turn {}.", c + 1);
        }
    }
    if (decisions.turn(GREEN).slot != TOP_LEFT_MOVE || attacks_low_on_pp(supplies)){
        return "Expected Tackle once Water Gun is out of PP, with its 34 PP not low.";
    }
    for (int c = 0; c < 30; c++){
        decisions.turn(GREEN);
    }
    if (!attacks_low_on_pp(supplies)){
        return "Expected the attacks low on PP with 4 Tackles left.";
    }
    healed_by_nurse(supplies);
    if (supplies.lead.moves[BOTTOM_LEFT_MOVE].pp != 25 || attacks_low_on_pp(supplies)){
        return "Expected the nurse to restore Water Gun's 25 PP.";
    }

    BattleDecisions out(supplies, BattleKind::TRAINER);
    out.turn(GREEN);
    out.turn_done(TurnChoice::MOVE, false);
    if (out.turn(GREEN).slot != TOP_LEFT_MOVE){
        return "Expected Tackle after the game said Water Gun has no PP left.";
    }

    Supplies new_game;
    BattleDecisions unknown(new_game, BattleKind::TRAINER);
    if (unknown.turn(GREEN).slot != BOTTOM_LEFT_MOVE){
        return "Expected the bottom left slot while the moves aren't all known.";
    }
    return true;
}

//  Regression: the save console B left after Route 3 (2026-11-02) has
//  Squirtle Lv16 with WATER GUN where TACKLE was and BUBBLE kept, as its
//  summary shows. From the moves read, the battle uses Water Gun from the
//  top left, then Bubble; never a Tackle it doesn't know, nor Bubble's slot
//  for Water Gun.
UnitTestResult test_moves_from_summary(Logger&){
    const std::vector<SummaryMove> read{
        {.move = "water-gun", .pp = 25, .max_pp = 25},
        {.move = "tail-whip", .pp = 30, .max_pp = 30},
        {.move = "bubble", .pp = 30, .max_pp = 30},
        {.move = "withdraw", .pp = 40, .max_pp = 40},
    };
    Supplies supplies;
    supplies.lead = member_from_summary("squirtle", 16, read);
    if (attack_pp(supplies.lead) != 55){
        return std::format("Expected 55 PP in Water Gun and Bubble, got {}.", attack_pp(supplies.lead));
    }
    BattleDecisions decisions(supplies, BattleKind::TRAINER);
    struct Turns{
        size_t count;
        size_t slot;
        std::string move;
    };
    const std::vector<Turns> plan{
        {.count = 25, .slot = TOP_LEFT_MOVE, .move = "Water Gun"},
        {.count = 30, .slot = BOTTOM_LEFT_MOVE, .move = "Bubble"},
        //  No damaging move left: a move with PP, so the turn isn't lost to "no PP left".
        {.count = 1, .slot = 1, .move = "Tail Whip"},
    };
    size_t turn = 0;
    for (const Turns& turns : plan){
        for (size_t c = 0; c < turns.count; c++){
            turn++;
            const TurnAction action = decisions.turn(GREEN);
            if (action.choice != TurnChoice::MOVE || action.slot != turns.slot){
                return std::format("Expected {} (slot {}) at turn {}, got slot {}.", turns.move, turns.slot, turn, action.slot);
            }
        }
    }

    //  Bite at Lv18 takes Tail Whip's slot, as the plan says.
    PartyMember lead = member_from_summary("squirtle", 16, read);
    const LearnDecision bite = learn_move(lead);
    if (bite.move != std::optional<std::string>("bite") || bite.forget_slot != std::optional<size_t>(1)){
        return "Expected Bite offered over Tail Whip (slot 1) after the moves read.";
    }
    return true;
}

//  Moves read off a summary: all it knows even when fewer than four, a PP
//  not read taken as half the full PP (the game colors it from half down),
//  and the strongest damaging move once the plan's attacks are gone.
UnitTestResult test_summary_members(Logger&){
    struct Case{
        std::string name;
        PartyMember member;
        std::optional<size_t> slot;
        size_t pp;
    };
    const std::vector<Case> cases{
        {
            .name = "three moves read",
            .member = member_from_summary("squirtle", 9, {
                {.move = "tackle", .pp = 35, .max_pp = 35}, {.move = "tail-whip", .pp = 30, .max_pp = 30},
                {.move = "bubble", .pp = 30, .max_pp = 30},
            }),
            .slot = 2, .pp = 65,
        },
        {
            .name = "Water Gun's PP not read",
            .member = member_from_summary("squirtle", 16, {
                {.move = "water-gun", .pp = -1, .max_pp = -1}, {.move = "tail-whip", .pp = 30, .max_pp = 30},
                {.move = "bubble", .pp = 30, .max_pp = 30}, {.move = "withdraw", .pp = 40, .max_pp = 40},
            }),
            .slot = 0, .pp = 42,
        },
        {
            .name = "none of the plan's attacks",
            .member = member_from_summary("wartortle", 40, {
                {.move = "tail-whip", .pp = 30, .max_pp = 30}, {.move = "rapid-spin", .pp = 40, .max_pp = 40},
                {.move = "skull-bash", .pp = 15, .max_pp = 15}, {.move = "withdraw", .pp = 40, .max_pp = 40},
            }),
            .slot = 2, .pp = 55,
        },
        {
            .name = "four moves not read, fewer known",
            .member = with_moves("squirtle", {"tackle", "tail-whip", "bubble"}, 7),
            .slot = std::nullopt, .pp = 65,
        },
    };
    for (const Case& c : cases){
        const std::optional<size_t> slot = attack_slot(c.member);
        if (slot != c.slot || attack_pp(c.member) != c.pp){
            return std::format(
                "{}: expected slot {} and {} PP in attacks, got slot {} and {}.",
                c.name, c.slot ? std::to_string(*c.slot) : "none", c.pp,
                slot ? std::to_string(*slot) : "none", attack_pp(c.member)
            );
        }
    }
    return true;
}


//  #211: Bite, not Water Gun, against the trainers whose parties resist
//  Water Gun: the Cerulean rival's Bulbasaur, and Misty's Staryu and
//  Starmie (Water, Psychic: Bite does twice). Once Bite is out of PP,
//  Tackle (35) over the resisted Water Gun (40 x 1.5 x 0.5); and the battle
//  loop picks Bite against a known party, but not in a wild battle.
UnitTestResult test_attack_by_matchup(Logger&){
    PartyMember wartortle = grown_to(new_member("squirtle", 5), 20);
    const std::vector<std::string> bulbasaur{"bulbasaur"};
    const std::optional<size_t> bite = 1;
    const std::optional<size_t> water_gun = BOTTOM_LEFT_MOVE;
    if (move_names(wartortle) != std::vector<std::string>{"tackle", "bite", "water-gun", "withdraw"}){
        return "Expected Wartortle Lv20 with Tackle, Bite, Water Gun, Withdraw, got " + joined(move_names(wartortle)) + ".";
    }
    struct Case{
        std::string name;
        std::vector<std::string> foes;
    };
    const std::vector<Case> cases{
        {.name = "the rival's Bulbasaur", .foes = bulbasaur},
        {.name = "the Cerulean rival's party", .foes = trainer_party("rival_cerulean")},
        {.name = "Misty's party", .foes = trainer_party("misty")},
    };
    for (const Case& c : cases){
        const std::optional<size_t> slot = attack_slot_against(wartortle, c.foes);
        if (slot != bite){
            return std::format("{} ({}): expected Bite (slot 1), got slot {}.", c.name, joined(c.foes), slot ? std::to_string(*slot) : "none");
        }
    }
    if (trainer_party("rival_cerulean") != std::vector<std::string>{"pidgeotto", "abra", "rattata", "bulbasaur"}){
        return "Expected the Cerulean rival's party from pret's sParty_RivalCeruleanBulbasaur, got " + joined(trainer_party("rival_cerulean")) + ".";
    }

    PartyMember no_bite = wartortle;
    no_bite.moves[1].pp = 0;
    if (attack_slot_against(no_bite, bulbasaur) != TOP_LEFT_MOVE || attack_slot_against(no_bite, {"pidgeotto"}) != water_gun){
        return "Expected Tackle against Bulbasaur once Bite is out of PP, and Water Gun against Pidgeotto.";
    }

    Supplies supplies;
    supplies.lead = wartortle;
    supplies.foes = bulbasaur;
    BattleDecisions trainer(supplies, BattleKind::TRAINER);
    if (trainer.turn(GREEN).slot != *bite || supplies.lead.moves[1].pp != 24){
        return "Expected the trainer battle's turn to use Bite against Bulbasaur, spending its PP.";
    }
    BattleDecisions wild(supplies, BattleKind::WILD_FIGHT);
    supplies.potions = 5;
    if (wild.turn(GREEN).slot != *water_gun){
        return "Expected a wild battle to keep the plan's order (Water Gun), whatever foes are set.";
    }
    return true;
}

//  A trainer whose party the story doesn't know, and no foes at all, get
//  the plan's order: Water Gun first.
UnitTestResult test_attack_unknown_foe(Logger&){
    const PartyMember wartortle = grown_to(new_member("squirtle", 5), 20);
    if (!trainer_party("bug_catcher_cale").empty()){
        return "Expected no party for a trainer the story doesn't know.";
    }
    const std::vector<std::vector<std::string>> unknown{{}, {"missingno"}, trainer_party("bug_catcher_cale")};
    for (const std::vector<std::string>& foes : unknown){
        if (attack_slot_against(wartortle, foes) != attack_slot(wartortle) || attack_slot(wartortle) != BOTTOM_LEFT_MOVE){
            return "Expected the plan's order (Water Gun) against " + joined(foes) + ".";
        }
    }
    return true;
}

//  Bite is learned when offered (Wartortle Lv19, pret's
//  sWartortleLevelUpLearnset) over Tail Whip, else over Withdraw; never
//  over Water Gun.
UnitTestResult test_bite_learned(Logger&){
    struct Case{
        std::string name;
        std::vector<std::string> moves;
        size_t forget;
    };
    const std::vector<Case> cases{
        {.name = "with Tail Whip", .moves = {"tackle", "tail-whip", "water-gun", "withdraw"}, .forget = 1},
        {.name = "Water Gun over Tackle, no Tail Whip", .moves = {"water-gun", "bubble", "tackle", "withdraw"}, .forget = 3},
    };
    for (const Case& c : cases){
        PartyMember member = with_moves("wartortle", c.moves, 18);
        const LearnDecision decision = learn_move(member);
        if (decision.move != "bite" || decision.forget_slot != c.forget){
            return std::format(
                "{}: expected Bite over slot {}, got {} over slot {}.", c.name, c.forget,
                decision.move.value_or("nothing"), decision.forget_slot ? std::to_string(*decision.forget_slot) : "none"
            );
        }
    }
    return true;
}


//  A fake evolution scene: B pressed while it shows "is evolving!" cancels
//  the evolution, as in the game.
struct FakeEvolution{
    std::vector<BattleTextScreen> frames;
    size_t evolving_from = 0;
    size_t evolving_to = 0;
};

//  Plays `scene` against the loop's rule (B at any screen it doesn't hold
//  for), after `turns` turns: whether the evolution went through.
bool evolves_against_loop(const FakeEvolution& scene, const EvolutionHold& hold, size_t turns){
    bool cancelled = false;
    for (size_t frame = 0; frame < scene.frames.size(); frame++){
        const bool pressed_b = !hold.holds(scene.frames[frame], turns);
        if (pressed_b && frame >= scene.evolving_from && frame < scene.evolving_to){
            cancelled = true;
        }
    }
    return !cancelled;
}

//  Regression: Squirtle stayed Lv16 on console B (2026-11-02), the B
//  presses after the battle cancelling its evolution. No B while "What?
//  SQUIRTLE is evolving!" shows; B again for the arrow after it.
UnitTestResult test_no_b_during_evolution(Logger&){
    const BattleTextScreen in_battle{.text_box = true, .hp_box = true, .arrow = false, .yes_no = false};
    const BattleTextScreen evolving{.text_box = true, .hp_box = false, .arrow = false, .yes_no = false};
    const BattleTextScreen congratulations{.text_box = true, .hp_box = false, .arrow = true, .yes_no = false};
    const BattleTextScreen learn_prompt{.text_box = true, .hp_box = false, .arrow = false, .yes_no = true};
    FakeEvolution scene;
    scene.frames = {in_battle, in_battle, congratulations};
    scene.frames.insert(scene.frames.begin() + 2, 40, evolving);
    scene.evolving_from = 2;
    scene.evolving_to = 42;

    PartyMember lead = with_moves("squirtle", {"tackle", "tail-whip", "bubble", "withdraw"}, 13);
    EvolutionHold hold(lead);
    if (!evolves_against_loop(scene, hold, 1)){
        return "B was pressed during \"is evolving!\": the evolution would be cancelled.";
    }
    if (hold.holds(congratulations, 1) || hold.holds(learn_prompt, 1)){
        return "Expected B (or the prompt's answer) for the text with an arrow and the YES/NO after the evolution.";
    }

    hold.held(1s, false);
    if (lead.species != "squirtle"){
        return "A text box held for 1 s was taken for an evolution.";
    }
    hold.held(15s, false);
    if (lead.species != "wartortle"){
        return "Expected Squirtle to be Wartortle after 15 s of its evolution screen, got " + lead.species + ".";
    }
    hold.held(40s, false);
    if (hold.holds(evolving, 1)){
        return "Expected no more holding once a battle's holds reach their cap.";
    }

    PartyMember other = new_member("squirtle", 5);
    EvolutionHold before_turn(other);
    if (before_turn.holds(evolving, 0)){
        return "Held before the first turn, where Oak's text box needs B.";
    }
    return true;
}


}


void add_tests_AutoStoryParty(UnitTestDatabase& database){
    database.add<Test_AutoStoryParty>("movesets the game gives and the story plays", test_movesets);
    database.add<Test_AutoStoryParty>("move offered and slot forgotten", test_learn_prompts);
    database.add<Test_AutoStoryParty>("YES/NO answers to move prompts", test_prompt_answers);
    database.add<Test_AutoStoryParty>("fainted lead runs or sends the next member", test_fainted_lead);
    database.add<Test_AutoStoryParty>("PP: preferred attack, then Tackle; nurse restores", test_pp);
    database.add<Test_AutoStoryParty>("moves read off the summary, Water Gun over Tackle", test_moves_from_summary);
    database.add<Test_AutoStoryParty>("summary members: fewer moves, unread PP, strongest attack", test_summary_members);
    database.add<Test_AutoStoryParty>("no B during an evolution", test_no_b_during_evolution);
    database.add<Test_AutoStoryParty>("attacks by matchup: Bite on the rival's Bulbasaur and Misty", test_attack_by_matchup);
    database.add<Test_AutoStoryParty>("attacks against an unknown foe: the plan's order", test_attack_unknown_foe);
    database.add<Test_AutoStoryParty>("Bite learned over Tail Whip or Withdraw", test_bite_learned);
}



}
}
}
