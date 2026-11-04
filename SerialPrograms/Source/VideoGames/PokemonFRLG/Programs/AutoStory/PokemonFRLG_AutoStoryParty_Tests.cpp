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
#include "PokemonFRLG_AutoStoryBattleData.h"
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

//  Mega Kick, from Route 4's tutor over Withdraw: no level-up rule takes
//  its slot, so Rapid Spin and Protect are declined.
UnitTestResult test_mega_kick_kept(Logger&){
    const std::vector<std::string> moves{"bite", "water-pulse", "bubble", "mega-kick"};
    for (const auto& [level, offered] : {std::pair<uint8_t, std::string>{24, "rapid-spin"}, {30, "protect"}}){
        PartyMember member = with_moves("wartortle", moves, level);
        const LearnDecision decision = learn_move(member);
        if (decision.move != offered || decision.forget_slot || move_names(member) != moves){
            return std::format("Lv{}: expected {} declined, leaving {}; got {}, leaving {}.",
                level + 1, offered, joined(moves), decision.move.value_or("nothing"), joined(move_names(member)));
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
    out.turn_done(TurnChoice::MOVE, TurnResult::NOT_DONE);
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
//  Tackle (35, on Bulbasaur's lower Defense) over the resisted Water Gun
//  (40 x 1.5 x 0.5); and the battle loop picks Bite against a known party,
//  but not in a wild battle, which has its own foes (BattleData tests).
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
        {.name = "the Cerulean rival's party", .foes = trainer_party("TRAINER_RIVAL_CERULEAN_BULBASAUR")},
        {.name = "Misty's party", .foes = trainer_party("TRAINER_LEADER_MISTY")},
    };
    for (const Case& c : cases){
        const std::optional<size_t> slot = attack_slot_against(wartortle, c.foes);
        if (slot != bite){
            return std::format("{} ({}): expected Bite (slot 1), got slot {}.", c.name, joined(c.foes), slot ? std::to_string(*slot) : "none");
        }
    }
    const std::vector<std::string> rival = trainer_party(story_trainer("CeruleanCity", "rival_cerulean").value_or(""));
    if (rival != std::vector<std::string>{"pidgeotto", "abra", "rattata", "bulbasaur"}){
        return "Expected the Cerulean rival's party from pret's sParty_RivalCeruleanBulbasaur, got " + joined(rival) + ".";
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
        return "Expected a wild battle with nothing read to keep the plan's order (Water Gun), whatever foes are set.";
    }
    return true;
}

//  A trainer the data doesn't have, a species it doesn't have, and no foes
//  at all get the plan's order: Water Gun first.
UnitTestResult test_attack_unknown_foe(Logger&){
    const PartyMember wartortle = grown_to(new_member("squirtle", 5), 20);
    if (!trainer_party("TRAINER_NOBODY").empty() || story_trainer("Route24", "nobody")){
        return "Expected no party for a trainer the data doesn't have.";
    }
    const std::vector<std::vector<std::string>> unknown{{}, {"missingno"}, trainer_party("TRAINER_NOBODY")};
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
//  for): whether the evolution went through.
bool evolves_against_loop(const FakeEvolution& scene, const EvolutionHold& hold){
    bool cancelled = false;
    for (size_t frame = 0; frame < scene.frames.size(); frame++){
        const bool pressed_b = !hold.holds(scene.frames[frame]);
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
    hold.turn_taken(TurnChoice::MOVE);
    if (!evolves_against_loop(scene, hold)){
        return "B was pressed during \"is evolving!\": the evolution would be cancelled.";
    }
    if (hold.holds(congratulations) || hold.holds(learn_prompt)){
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
    if (hold.holds(evolving)){
        return "Expected no more holding once a battle's holds reach their cap.";
    }

    PartyMember other = new_member("squirtle", 5);
    EvolutionHold before_turn(other);
    if (before_turn.holds(evolving)){
        return "Held before the first turn, where Oak's text box needs B.";
    }
    return true;
}

//  A poll of the battle loop: what it read, at `ms` from the start.
struct LoopPoll{
    int ms;
    BattleTextScreen screen;
    bool menu = false;
};

//  Regression: console B, 2026-11-03, Mt. Moon B2F: RUN, "Got away
//  safely!", then the cave corridor, whose black lower half reads as the
//  battle's text box with no HP box. The loop held B for an evolution for
//  a minute, then pressed A and B for 9 minutes. After RUN nothing is held,
//  and START looks for the overworld within seconds of the last HP box.
UnitTestResult test_run_then_overworld(Logger&){
    const BattleTextScreen menu_screen{.text_box = true, .hp_box = true, .arrow = false, .yes_no = false};
    const BattleTextScreen got_away{.text_box = true, .hp_box = true, .arrow = true, .yes_no = false};
    const BattleTextScreen dark_cave{.text_box = true, .hp_box = false, .arrow = false, .yes_no = false};
    std::vector<LoopPoll> polls{{.ms = 0, .screen = menu_screen, .menu = true}};
    for (int ms = 300; ms <= 1500; ms += 300){
        polls.push_back({.ms = ms, .screen = got_away});
    }
    for (int ms = 1800; ms <= 60000; ms += 300){
        polls.push_back({.ms = ms, .screen = dark_cave});
    }

    Supplies supplies;
    BattleDecisions decisions(supplies, BattleKind::WILD_RUN);
    EvolutionHold hold(supplies.lead);
    BattleEndWatch watch;
    const WallClock start = current_time();
    const TurnChoice choice = decisions.turn(PartyStatus{.hp_seen = true}).choice;
    if (choice != TurnChoice::RUN){
        return "Expected RUN from a wild Pokemon.";
    }
    hold.turn_taken(choice);
    for (const LoopPoll& poll : polls){
        const WallClock now = start + std::chrono::milliseconds(poll.ms);
        if (hold.holds(poll.screen)){
            return std::format("Held B for an evolution at {} ms, after RUN.", poll.ms);
        }
        const bool battle = poll.screen.hp_box || poll.screen.yes_no;
        watch.seen(poll.menu ? BattleSeen::MENU : battle ? BattleSeen::BATTLE : BattleSeen::NONE, now);
        if (watch.check_overworld(now)){
            //  The last HP box at 1.5 s; START within the next 3.5 s.
            if (poll.ms > 5000){
                return std::format("START first looked for the overworld at {} ms.", poll.ms);
            }
            return true;
        }
    }
    return "START never looked for the overworld in the dark cave.";
}

//  START only once the battle is gone for a while, and then not at every
//  poll; A and B blind for at most 30 s since the last battle menu.
UnitTestResult test_end_watch_timing(Logger&){
    const WallClock start = current_time();
    const auto at = [&](int ms){ return start + std::chrono::milliseconds(ms); };
    BattleEndWatch watch;
    if (watch.check_overworld(at(10000))){
        return "Looked for the overworld before any battle screen showed.";
    }
    for (int ms = 0; ms <= 20000; ms += 300){
        watch.seen(BattleSeen::BATTLE, at(ms));
        if (watch.check_overworld(at(ms))){
            return std::format("Looked for the overworld at {} ms with an HP box on screen.", ms);
        }
    }
    int checks = 0;
    for (int ms = 20100; ms <= 32000; ms += 300){
        watch.seen(BattleSeen::NONE, at(ms));
        checks += watch.check_overworld(at(ms)) ? 1 : 0;
    }
    if (checks < 2 || checks > 4){
        return std::format("Expected START every 3 s for 12 s without the battle, got {} looks.", checks);
    }
    for (int ms = 0; ms < 30000; ms += 1500){
        if (watch.pressed_blind(at(40000 + ms))){
            return std::format("Gave up pressing blind after {} ms.", ms);
        }
    }
    if (!watch.pressed_blind(at(70000))){
        return "Expected to give up after 30 s of blind presses.";
    }
    watch.seen(BattleSeen::MENU, at(71000));
    if (watch.pressed_blind(at(72000))){
        return "Expected the battle menu to start the blind presses' time over.";
    }
    return true;
}


}


//  The lead console B took into Super Nerd Miguel's battle (2026-11-03,
//  Mt. Moon B2F), as its summary read before the run.
PartyMember squirtle_before_miguel(){
    return member_from_summary("squirtle", 16, {
        {.move = "water-gun", .pp = 25, .max_pp = 25},
        {.move = "tail-whip", .pp = 30, .max_pp = 30},
        {.move = "bubble", .pp = 30, .max_pp = 30},
        {.move = "withdraw", .pp = 40, .max_pp = 40},
    });
}

//  Regression, console B 2026-11-03: in the trainer's battle before the
//  Rocket Grunt, a YES/NO read at a level-up (Lv17: no move to learn) got
//  the plan's YES for Bite over Tail Whip, and no move list followed. The
//  story still took Tail Whip as Bite, and in Miguel's battle chose it as
//  its attack for 7 turns, until the starter fainted. The moves change only
//  once the move list was seen.
UnitTestResult test_yes_no_without_move_list(Logger&){
    Supplies supplies;
    supplies.lead = squirtle_before_miguel();
    BattleDecisions decisions(supplies, BattleKind::TRAINER);
    decisions.turn(GREEN);
    const PromptAnswer answer = decisions.prompt(false);
    if (answer.answer != YesNo::YES || answer.forget_slot != std::optional<size_t>(1)){
        return "Expected the plan's YES, Bite over Tail Whip (slot 1).";
    }
    const std::vector<std::string> before{"water-gun", "tail-whip", "bubble", "withdraw"};
    if (move_names(supplies.lead) != before){
        return "Expected the moves unchanged with no move list seen, got " + joined(move_names(supplies.lead)) + ".";
    }
    const std::vector<std::string> after{"water-gun", "bite", "bubble", "withdraw"};
    if (!answer.learned || move_names(*answer.learned) != after){
        return "Expected the lead to keep once the move list shows to have Bite over Tail Whip.";
    }
    return true;
}

//  Regression, console B 2026-11-03, Super Nerd Miguel (pret
//  sParty_SuperNerdMiguel: Grimer Lv12, whose moves include Disable): after
//  turn 1, Water Gun was turned down 17 times with no turn passing, and
//  each try spent a PP of the story's count. Turned down once, it is left
//  alone for as long as Disable lasts, its PP given back, and Bubble (not
//  Tail Whip) attacks meanwhile.
UnitTestResult test_disabled_move(Logger&){
    Supplies supplies;
    supplies.lead = squirtle_before_miguel();
    supplies.foes = {"grimer", "voltorb", "koffing"};
    BattleDecisions decisions(supplies, BattleKind::TRAINER);
    if (decisions.turn(GREEN).slot != TOP_LEFT_MOVE || decisions.turn(GREEN).slot != TOP_LEFT_MOVE){
        return "Expected Water Gun (slot 0) at turns 1 and 2.";
    }
    decisions.turn_done(TurnChoice::MOVE, TurnResult::REFUSED);
    if (supplies.lead.moves[TOP_LEFT_MOVE].pp != 24){
        return std::format("Expected Water Gun at 24 PP after one turn and one refused, got {}.", supplies.lead.moves[TOP_LEFT_MOVE].pp);
    }
    for (int c = 0; c < 5; c++){
        const TurnAction action = decisions.turn(GREEN);
        if (action.choice != TurnChoice::MOVE || action.slot != BOTTOM_LEFT_MOVE){
            return std::format("Expected Bubble (slot 2) at turn {} after Water Gun was turned down, got slot {}.", c + 1, action.slot);
        }
    }
    if (decisions.turn(GREEN).slot != TOP_LEFT_MOVE){
        return "Expected Water Gun again once Disable is over.";
    }
    return true;
}

//  Regression, console B 2026-11-03, m68, the S.S. Anne rival: "no PP left
//  for that move" read 37 s after the battle's last move, as it ended, and
//  the story took the move's PP as 0. Read once, a move is left alone as a
//  Disable (Kadabra's) would have it, its PP kept; read again for the same
//  move, its PP is 0.
UnitTestResult test_no_pp_read_once(Logger&){
    Supplies supplies;
    supplies.lead = squirtle_before_miguel();
    supplies.foes = {"grimer", "voltorb", "koffing"};
    BattleDecisions decisions(supplies, BattleKind::TRAINER);
    if (decisions.turn(GREEN).slot != TOP_LEFT_MOVE){
        return "Expected Water Gun (slot 0) at turn 1.";
    }
    decisions.turn_done(TurnChoice::MOVE, TurnResult::NOT_DONE);
    if (supplies.lead.moves[TOP_LEFT_MOVE].pp != 25){
        return std::format("Expected Water Gun's 25 PP kept after one \"no PP\" read, got {}.", supplies.lead.moves[TOP_LEFT_MOVE].pp);
    }
    for (int c = 0; c < 5; c++){
        if (decisions.turn(GREEN).slot == TOP_LEFT_MOVE){
            return std::format("Expected Water Gun left alone at turn {} after the read, as a Disable.", c + 1);
        }
    }
    if (decisions.turn(GREEN).slot != TOP_LEFT_MOVE){
        return "Expected Water Gun again once a Disable would be over.";
    }
    decisions.turn_done(TurnChoice::MOVE, TurnResult::NOT_DONE);
    if (supplies.lead.moves[TOP_LEFT_MOVE].pp != 0){
        return std::format("Expected Water Gun at 0 PP after a second read, got {}.", supplies.lead.moves[TOP_LEFT_MOVE].pp);
    }
    return true;
}

//  "no PP left" counts only as soon after the choice as a refusal's menu
//  comes: m68's read 37 s after the battle's last move doesn't.
UnitTestResult test_no_pp_due(Logger&){
    const WallClock start = current_time();
    MoveRefusalWatch watch;
    if (watch.no_pp_due(start)){
        return "Expected no \"no PP\" read counted with no move chosen.";
    }
    watch.move_chosen(start);
    if (!watch.no_pp_due(start + 4000ms)){
        return "Expected a \"no PP\" read 4 s after the choice counted.";
    }
    if (watch.no_pp_due(start + 37000ms)){
        return "Expected a \"no PP\" read 37 s after the choice not counted.";
    }
    return true;
}

//  The loop's view of a move turned down (console B, 2026-11-03): the move
//  list back (no battle screen) and the battle menu 5.7 s after the choice.
//  Not a turn (8.3 s and up, with no move list), nor a menu long after one
//  (a YES/NO's presses, 25 s).
UnitTestResult test_move_refusal_watch(Logger&){
    const WallClock start = current_time();
    MoveRefusalWatch watch;
    watch.move_chosen(start);
    watch.no_battle_screen();
    if (!watch.refused(start + 5700ms)){
        return "Expected the move list then the menu at 5.7 s read as the move turned down.";
    }
    watch.move_chosen(start);
    if (watch.refused(start + 5000ms)){
        return "Expected no refusal without the move list between.";
    }
    watch.move_chosen(start);
    watch.no_battle_screen();
    if (watch.refused(start + 25000ms)){
        return "Expected no refusal for a menu 25 s after the choice.";
    }
    watch.no_battle_screen();
    if (watch.refused(start + 26000ms)){
        return "Expected no refusal with no move chosen.";
    }
    return true;
}

//  Regression, console B 2026-11-03, Miguel's battle (m48): the move list
//  opened after YES to "Delete a move to make room for BITE?", and closed
//  with no move forgotten, so the game asked "Stop learning BITE?" where
//  the story expected the next move's "Delete a move?". NO there and YES
//  at "Delete a move?" went round 13 times (75 s), until presses by time
//  ended it. A YES/NO right after the move list is "Stop learning?": YES,
//  and the lead keeps its moves.
UnitTestResult test_move_list_closed(Logger&){
    enum class Prompt{DELETE, STOP};
    Supplies supplies;
    supplies.lead = squirtle_before_miguel();
    BattleDecisions decisions(supplies, BattleKind::TRAINER);
    decisions.turn(GREEN);
    Prompt prompt = Prompt::DELETE;
    for (int answers = 1; answers <= 8; answers++){
        const PromptAnswer answer = decisions.prompt(false);
        if (answer.learned){
            //  The move list showed, as the loop saw it.
            supplies.lead = *answer.learned;
        }
        if (prompt == Prompt::STOP && answer.answer == YesNo::YES){
            const std::vector<std::string> before{"water-gun", "tail-whip", "bubble", "withdraw"};
            if (move_names(supplies.lead) != before){
                return "Expected the moves as before Bite was offered, got " + joined(move_names(supplies.lead)) + ".";
            }
            return true;
        }
        //  The move list closes with nothing forgotten: "Stop learning?",
        //  as after NO; NO there asks "Delete a move?" again.
        prompt = prompt == Prompt::DELETE ? Prompt::STOP : Prompt::DELETE;
    }
    return "Expected the move prompts to end within 8 answers.";
}

//  However the YES/NOs come, a few after a turn are all the move prompts
//  can be (two moves offered, NO then YES each); after those, YES only,
//  which ends either prompt ("Delete a move?": the move list, which the
//  loop's B closes; "Stop learning?": done), and the moves stay as they are.
UnitTestResult test_move_prompts_bounded(Logger&){
    Supplies supplies;
    supplies.lead = with_moves("wartortle", {"tackle", "bite", "water-gun", "withdraw"}, 19);
    BattleDecisions decisions(supplies, BattleKind::TRAINER);
    decisions.turn(GREEN);
    for (int answers = 1; answers <= 12; answers++){
        const PromptAnswer answer = decisions.prompt(false);
        if (answers > 4 && (answer.answer != YesNo::YES || answer.forget_slot)){
            return std::format("Expected a plain YES at the move prompt {} after a turn.", answers);
        }
    }
    const std::vector<std::string> moves{"tackle", "bite", "water-gun", "withdraw"};
    if (move_names(supplies.lead) != moves){
        return "Expected the moves unchanged, got " + joined(move_names(supplies.lead)) + ".";
    }
    return true;
}

//  Regression, console B 2026-11-03, before Miguel (m48): yellow HP
//  (17/55) and no Potion left, and the story went into the battle, as the
//  nurse was only for red HP. After the items, the nurse for any HP not
//  green, a status, or attacks low on PP.
UnitTestResult test_nurse_before_battle(Logger&){
    Supplies supplies;
    supplies.lead = squirtle_before_miguel();
    if (nurse_before_battle(supplies)){
        return "Expected no nurse with green HP, no status and full PP.";
    }
    supplies.hurt = true;
    if (!nurse_before_battle(supplies)){
        return "Expected the nurse for yellow HP that no Potion healed.";
    }
    supplies.hurt = false;
    supplies.status = StatusCondition::OTHER;
    if (!nurse_before_battle(supplies)){
        return "Expected the nurse for a status no item cured.";
    }
    return true;
}

void add_tests_AutoStoryParty(UnitTestDatabase& database){
    database.add<Test_AutoStoryParty>("movesets the game gives and the story plays", test_movesets);
    database.add<Test_AutoStoryParty>("move offered and slot forgotten", test_learn_prompts);
    database.add<Test_AutoStoryParty>("Mega Kick kept over later level-up moves", test_mega_kick_kept);
    database.add<Test_AutoStoryParty>("YES/NO answers to move prompts", test_prompt_answers);
    database.add<Test_AutoStoryParty>("fainted lead runs or sends the next member", test_fainted_lead);
    database.add<Test_AutoStoryParty>("PP: preferred attack, then Tackle; nurse restores", test_pp);
    database.add<Test_AutoStoryParty>("moves read off the summary, Water Gun over Tackle", test_moves_from_summary);
    database.add<Test_AutoStoryParty>("summary members: fewer moves, unread PP, strongest attack", test_summary_members);
    database.add<Test_AutoStoryParty>("no B during an evolution", test_no_b_during_evolution);
    database.add<Test_AutoStoryParty>("RUN, then a dark cave: no evolution wait, START soon", test_run_then_overworld);
    database.add<Test_AutoStoryParty>("battle end: START after the battle is gone, A and B for 30 s at most", test_end_watch_timing);
    database.add<Test_AutoStoryParty>("attacks by matchup: Bite on the rival's Bulbasaur and Misty", test_attack_by_matchup);
    database.add<Test_AutoStoryParty>("attacks against an unknown foe: the plan's order", test_attack_unknown_foe);
    database.add<Test_AutoStoryParty>("Bite learned over Tail Whip or Withdraw", test_bite_learned);
    database.add<Test_AutoStoryParty>("a YES/NO with no move list after it changes no move", test_yes_no_without_move_list);
    database.add<Test_AutoStoryParty>("a disabled move: left alone while Disable lasts, PP back", test_disabled_move);
    database.add<Test_AutoStoryParty>("a move turned down: the move list, then the menu sooner than a turn", test_move_refusal_watch);
    database.add<Test_AutoStoryParty>("\"no PP\" read once: left alone as a Disable; twice: PP 0", test_no_pp_read_once);
    database.add<Test_AutoStoryParty>("\"no PP\" counts only soon after the move chosen", test_no_pp_due);
    database.add<Test_AutoStoryParty>("the move list closed with nothing forgotten: YES to \"Stop learning?\"", test_move_list_closed);
    database.add<Test_AutoStoryParty>("move prompts after a turn: a plain YES after the first few", test_move_prompts_bounded);
    database.add<Test_AutoStoryParty>("the nurse before a battle for HP not green with no Potion", test_nurse_before_battle);
    add_tests_AutoStoryBattleData(database);
}



}
}
}
