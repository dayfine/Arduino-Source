/*  AutoStory Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include <memory>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/Tools/DebugDumper.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleFlashWatcher.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleHpBarReader.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattlePokemonDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleScreenDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_PartyMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SummaryMovesReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryBattleDecisions.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryFoeTracker.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{

constexpr Milliseconds POLL = 300ms;
constexpr Milliseconds BATTLE_TIMEOUT = 600000ms;
//  To see the battle at all, from its first flash.
constexpr Milliseconds BATTLE_START_TIMEOUT = 20000ms;
//  No battle screen for this long: check for the overworld with START.
constexpr Milliseconds BATTLE_GONE = 2000ms;
//  A battle screen with neither an arrow nor a menu for this long: B, for
//  a prompt the detectors miss.
constexpr Milliseconds STUCK = 8000ms;
//  Seen this many polls in a row: the starter fainted.
constexpr int FAINT_POLLS = 3;
//  No battle menu for this long: A and B in turn, for a prompt nothing reads.
//  Only near battle screens: A on the overworld after a gym leader's speech
//  would talk to them again.
constexpr Milliseconds NO_MENU = 60000ms;
constexpr Milliseconds NO_MENU_NEAR = 10000ms;
//  No battle screen yet and no turn taken: whether the overworld is back
//  is checked by START this often (a trainer's battle that never started,
//  console B, 2026-11-01: 10 minutes of B on the overworld).
constexpr Milliseconds OVERWORLD_CHECK = 3000ms;
//  No battle menu or HP box this long after a battle was seen: there was
//  none (console B, 2026-11-01, Viridian Forest's north gate mat).
constexpr Milliseconds NOT_A_BATTLE = 15000ms;
//  An evolution's animation, with no presses: as long as exit_wild_battle()
//  (PokemonFRLG_Navigation.cpp) waits for one.
constexpr Milliseconds EVOLUTION = 20000ms;
//  Text and the move learn prompts after a battle, on the overworld.
constexpr Milliseconds AFTER_BATTLE_TEXT = 60000ms;
//  The move list to fade in after "Delete a move?", and its cursor to move.
constexpr Milliseconds MOVE_LIST_TIMEOUT = 5000ms;

//  In the player's box, as a fraction of the 240x160 screen: the status
//  badge left of the HP numbers. The box bobs by a pixel; this keeps a
//  pixel inside.
const ImageFloatBox STATUS_BADGE_BOX(146.0 / 240, 99.0 / 160, 12.0 / 240, 4.0 / 160);

StatusCondition read_status_badge(const ImageViewRGB32& badge){
    const ImageStats stats = image_stats(badge);
    const double r = stats.average.r;
    const double g = stats.average.g;
    const double b = stats.average.b;
    const double off_white = std::abs(r - 255) + std::abs(g - 255) + std::abs(b - 217);
    if (off_white < 60){
        return StatusCondition::NONE;
    }
    if (r > 200 && g < 200 && b > 180){
        return StatusCondition::POISON;
    }
    //  Not measured yet: PAR's badge is taken to be the yellow one.
    if (r > 180 && g > 150 && b < 140){
        return StatusCondition::PARALYSIS;
    }
    return StatusCondition::OTHER;
}


enum class BattleScreen{
    //  Keep in step with screen_name().
    MENU,       //  "What will <Pokemon> do?" with FIGHT/BAG/POKEMON/RUN
    LEARN,      //  a YES/NO about a move to learn
    ADVANCE,    //  battle text waiting for a button
    OTHER,      //  the battle, busy
    NONE,       //  no battle on screen
};

const char* screen_name(BattleScreen screen){
    switch (screen){
    case BattleScreen::MENU:
        return "battle menu";
    case BattleScreen::LEARN:
        return "move prompt";
    case BattleScreen::ADVANCE:
        return "battle text with arrow";
    case BattleScreen::OTHER:
        return "battle text box";
    case BattleScreen::NONE:
        return "no battle screen";
    }
    return "?";
}

//  Until `confirmed`, the battle text box alone counts for nothing: it also
//  reads on the black below a small indoor map.
BattleScreen read_battle_screen(const ImageViewRGB32& screen, bool confirmed){
    if (BattleMenuDetector(COLOR_RED).detect(screen)){
        return BattleScreen::MENU;
    }
    if (BattleLearnDialogDetector(COLOR_RED).detect(screen)){
        return BattleScreen::LEARN;
    }
    if (!confirmed && !BattleScreenDetector(COLOR_RED).detect(screen)){
        return BattleScreen::NONE;
    }
    if (AdvanceBattleDialogDetector(COLOR_RED).detect(screen)){
        return BattleScreen::ADVANCE;
    }
    if (BattleDialogDetector(COLOR_RED).detect(screen)){
        return BattleScreen::OTHER;
    }
    return BattleScreen::NONE;
}

BattleTextScreen read_text_screen(const ImageViewRGB32& screen){
    return BattleTextScreen{
        .text_box = BattleDialogDetector(COLOR_RED).detect(screen),
        .hp_box = BattleScreenDetector(COLOR_RED).detect(screen),
        .arrow = AdvanceBattleDialogDetector(COLOR_RED).detect(screen),
        .yes_no = BattleLearnDialogDetector(COLOR_RED).detect(screen) || yes_no_cursor(screen).has_value(),
    };
}

//  The player's Pokemon gone while the opponent's is still there.
bool starter_fainted(const ImageViewRGB32& screen){
    return !BattlePokemonDetector(COLOR_RED).detect(screen) && BattleOpponentDetector(COLOR_RED).detect(screen);
}

void tap(ProControllerContext& context, DpadPosition position){
    pbf_press_dpad(context, position, 160ms, 240ms);
}

//  In the bag, with the cursor on `row`: whether that row is `item`. FAST
//  doesn't look. A row that can't be read (as in a language the reader has
//  no names for) is taken by its position; only another item's name stops it.
bool bag_row_is(
    ConsoleHandle& console, ProControllerContext& context,
    int row, const std::string& item, StoryChecks checks, Language language
){
    if (checks == StoryChecks::FAST){
        return true;
    }
    context.wait_for_all_requests();
    VideoSnapshot screen = console.video().snapshot();
    const std::string read = screen ? read_item_list_name(console, language, screen, row) : "";
    if (read == item){
        return true;
    }
    if (read.empty()){
        console.log(std::format("Bag: row {} unread, taking it as the {} by its position.", row + 1, item), COLOR_ORANGE);
        return true;
    }
    console.log(std::format("Bag: row {} reads \"{}\", not {}: not using it.", row + 1, read, item), COLOR_RED);
    dump_debug_image(console, "AutoStory/bag", std::format("row{}-not-{}", row + 1, item), screen);
    return false;
}

//  From the battle menu: `item`, in `row` (from 0) of the Items pocket, on
//  the starter. The bag opens on the pocket used last; Items is the first.
//  Returns whether it was used; if the row holds something else (THOROUGH),
//  backs out to the battle menu. Not run on the console yet.
bool use_item(
    ConsoleHandle& console, ProControllerContext& context,
    int row, const std::string& item, StoryChecks checks, Language language
){
    console.log(std::format("Battle: using the {} in row {} of the Items pocket.", item, row + 1));
    tap(context, DPAD_LEFT);
    tap(context, DPAD_UP);
    tap(context, DPAD_RIGHT);
    pbf_press_button(context, BUTTON_A, 160ms, 1500ms);
    for (int c = 0; c < 2; c++){
        pbf_press_dpad(context, DPAD_LEFT, 160ms, 500ms);
    }
    for (int c = 0; c < 12; c++){
        pbf_press_dpad(context, DPAD_UP, 100ms, 150ms);
    }
    for (int c = 0; c < row; c++){
        tap(context, DPAD_DOWN);
    }
    if (!bag_row_is(console, context, row, item, checks, language)){
        pbf_press_button(context, BUTTON_B, 160ms, 1500ms);
        return false;
    }
    //  The item, USE, then the starter in the party list.
    pbf_press_button(context, BUTTON_A, 160ms, 800ms);
    pbf_press_button(context, BUTTON_A, 160ms, 1500ms);
    pbf_press_button(context, BUTTON_A, 160ms, 1000ms);
    return true;
}

//  RUN, bottom right of the battle menu.
void run_away(ProControllerContext& context){
    tap(context, DPAD_LEFT);
    tap(context, DPAD_UP);
    tap(context, DPAD_RIGHT);
    tap(context, DPAD_DOWN);
    pbf_press_button(context, BUTTON_A, 160ms, 500ms);
}

//  FIGHT, then the move in `slot` from the top left. The cursor doesn't
//  move onto an empty slot.
void use_move(ProControllerContext& context, size_t slot){
    tap(context, DPAD_LEFT);
    tap(context, DPAD_UP);
    pbf_press_button(context, BUTTON_A, 160ms, 1000ms);
    tap(context, DPAD_LEFT);
    tap(context, DPAD_UP);
    if (slot % 2 == 1){
        tap(context, DPAD_RIGHT);
    }
    if (slot >= 2){
        tap(context, DPAD_DOWN);
    }
    pbf_press_button(context, BUTTON_A, 160ms, 500ms);
}

//  The move list's cursor row once it shows, and, if `before` is given, once
//  it has left that row: up to `timeout`.
std::optional<size_t> read_move_list_cursor(
    ConsoleHandle& console, ProControllerContext& context,
    std::optional<size_t> before, Milliseconds timeout
){
    const WallClock deadline = current_time() + timeout;
    while (true){
        context.wait_for_all_requests();
        VideoSnapshot screen = console.video().snapshot();
        const std::optional<size_t> cursor = screen ? read_summary_move_cursor(screen) : std::nullopt;
        if (cursor && cursor != before){
            return cursor;
        }
        if (current_time() >= deadline){
            return std::nullopt;
        }
        pbf_wait(context, POLL);
    }
}

//  After YES to "Delete a move?": the move list fades in with its cursor on
//  the top move (pret pokemon_summary_screen.c); A on a move forgets it,
//  B would go back to "Stop learning?". The cursor is read before A and
//  after each press: DOWNs sent while the list faded in were lost, and A
//  forgot the top move, Tackle, for Water Gun instead of Bubble (console B,
//  2026-11-02). Done in one go, so the loop's B presses can't land on the
//  list. A list whose cursor never shows gets the presses by time, as
//  before; a cursor that can't be brought to `slot` fails the battle.
//  YES, then `slot` on the move list. False, with nothing more pressed, if no move list showed.
bool forget_move(ConsoleHandle& console, ProControllerContext& context, size_t slot){
    pbf_press_button(context, BUTTON_A, 160ms, 0ms);
    std::optional<size_t> cursor = read_move_list_cursor(console, context, std::nullopt, MOVE_LIST_TIMEOUT);
    if (!cursor){
        console.log("Battle: no move list after YES; the moves stay as they were.", COLOR_ORANGE);
        return false;
    }
    for (size_t presses = 0; *cursor != slot; presses++){
        if (presses >= 2 * SUMMARY_MOVE_ROWS){
            break;
        }
        const size_t from = *cursor;
        pbf_press_dpad(context, from < slot ? DPAD_DOWN : DPAD_UP, 160ms, 0ms);
        cursor = read_move_list_cursor(console, context, from, MOVE_LIST_TIMEOUT);
        if (!cursor){
            break;
        }
    }
    if (cursor != slot){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format("forget_move(): the cursor didn't reach move {} on the move list.", slot + 1),
            console
        );
    }
    pbf_press_button(context, BUTTON_A, 160ms, 2000ms);
    return true;
}

//  The party list in battle opens on the lead: down to `slot`, then A, and
//  SHIFT, the top of its menu.
TurnResult item_result(bool used){
    return used ? TurnResult::DONE : TurnResult::NOT_DONE;
}

void send_member(ProControllerContext& context, size_t slot){
    for (size_t c = 0; c < slot; c++){
        pbf_press_dpad(context, DPAD_DOWN, 160ms, 400ms);
    }
    pbf_press_button(context, BUTTON_A, 160ms, 800ms);
    pbf_press_button(context, BUTTON_A, 160ms, 1500ms);
}


class BattleLoop{
public:
    BattleLoop(
        ConsoleHandle& console, ProControllerContext& context,
        Supplies& supplies, BattleKind kind, StoryChecks checks, Language language
    )
        : m_console(console)
        , m_context(context)
        , m_supplies(supplies)
        , m_decisions(supplies, kind)
        , m_evolution(supplies.lead)
        , m_foes(kind == BattleKind::TRAINER ? supplies.trainer : "", supplies.lead)
        , m_kind(kind)
        , m_checks(checks)
        , m_language(language)
        , m_start(current_time())
        , m_last_battle(m_start)
        , m_last_press(m_start)
        , m_last_menu(m_start)
        , m_last_overworld_check(m_start)
    {}

    BattleEnd run(){
        m_console.log(
            m_kind == BattleKind::TRAINER ? "Battle: fighting a trainer." :
            m_kind == BattleKind::WILD_FIGHT ? "Battle: fighting a wild Pokemon." : "Battle: running from a wild Pokemon."
        );
        bool over = false;
        while (!over){
            over = step();
        }
        m_console.log(std::format(
            "Battle: over after {} turns{}.", m_decisions.turns(), m_decisions.lost() ? ", the starter fainted" : ""
        ));
        return m_decisions.lost() ? BattleEnd::LOST : BattleEnd::WON;
    }

private:
    //  One look at the screen and what it asks for. True once the battle is over.
    bool step(){
        m_context.wait_for_all_requests();
        const WallClock now = current_time();
        check_time(now);
        VideoSnapshot screen = m_console.video().snapshot();
        if (!screen){
            pbf_wait(m_context, POLL);
            return false;
        }
        if (!m_confirmed && BattleScreenDetector(COLOR_RED).detect(screen)){
            m_console.log("Battle: the battle menu or an HP box shows.");
            m_confirmed = true;
        }
        if (!m_confirmed && now - m_start > NOT_A_BATTLE){
            m_console.log("Battle: no battle menu or HP box in 15 s: not a battle.", COLOR_ORANGE);
            return true;
        }
        if (between_turns(screen, now)){
            return false;
        }
        if (battle_gone(screen, now)){
            return true;
        }
        const BattleScreen seen = read_battle_screen(screen, m_confirmed);
        if (seen != m_last_seen){
            m_console.log(std::format("Battle: {} (turn {}).", screen_name(seen), m_decisions.turns()));
            m_last_seen = seen;
        }
        if (seen == BattleScreen::MENU){
            m_last_menu = now;
            m_move_chosen = false;
        }else if (now - m_last_menu > NO_MENU && now - m_last_battle < NO_MENU_NEAR && now - m_last_press > 1500ms){
            //  Something waits that none of the battle screens knows.
            return press_blind(now);
        }
        if (seen == BattleScreen::NONE){
            m_refusal.no_battle_screen();
            return no_battle_screen(screen, now);
        }
        m_seen_battle = true;
        m_last_battle = now;
        read_hp(screen);
        watch_for_faint(screen);
        m_foes.boxes_seen(BattleBoxes{.lead = BattlePokemonDetector(COLOR_RED).detect(screen), .foe = BattleOpponentDetector(COLOR_RED).detect(screen)});
        act(seen, screen, now);
        return false;
    }

    //  The screens between the turns that need more than B: "no PP left",
    //  the party list once the lead fainted, and an evolution after the
    //  battle. True if one was handled.
    bool between_turns(const ImageViewRGB32& screen, WallClock now){
        if (!m_confirmed){
            return false;
        }
        if (m_move_chosen && BattleOutOfPpDetector(COLOR_RED).detect(screen)){
            m_console.log("Battle: no PP left for that move; choosing again.", COLOR_ORANGE);
            m_decisions.turn_done(TurnChoice::MOVE, TurnResult::NOT_DONE);
            m_move_chosen = false;
            m_refusal = MoveRefusalWatch();
            pbf_press_button(m_context, BUTTON_B, 160ms, 800ms);
            pbf_press_button(m_context, BUTTON_B, 160ms, 800ms);
            m_last_press = now;
            return true;
        }
        if (m_decisions.lost() && PartyMenuDetector(COLOR_RED).detect(screen)){
            send_next_member(now);
            return true;
        }
        if (m_evolution.holds(read_text_screen(screen))){
            wait_out_evolution();
            return true;
        }
        return false;
    }

    //  The party list after the lead fainted: the next member allowed in, or
    //  a stop with only protected members left.
    void send_next_member(WallClock now){
        const std::optional<size_t> slot = m_decisions.next_member_slot();
        if (!slot){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                "fight_battle(): the starter fainted and only protected members are left; not sending one.",
                m_console
            );
        }
        m_console.log(std::format("Battle: the lead fainted; sending party slot {}.", *slot + 1), COLOR_ORANGE);
        send_member(m_context, *slot);
        m_last_press = now;
    }

    //  Nothing pressed while the screen may be an evolution, up to EVOLUTION.
    void wait_out_evolution(){
        const WallClock start = current_time();
        bool holds = true;
        while (holds && current_time() - start < EVOLUTION){
            pbf_wait(m_context, POLL);
            m_context.wait_for_all_requests();
            VideoSnapshot screen = m_console.video().snapshot();
            holds = !screen || m_evolution.holds(read_text_screen(screen));
        }
        const WallClock end = current_time();
        const Milliseconds held = std::chrono::duration_cast<Milliseconds>(end - start);
        if (held >= 2000ms){
            m_console.log(std::format("Battle: no B for {} s, for an evolution.", held.count() / 1000));
        }
        m_evolution.held(held, m_decisions.lost());
        m_last_press = end;
        m_last_battle = end;
    }

    //  True once START opens the menu, after no battle menu, HP box or
    //  YES/NO for a while: whatever the text box reads, the battle is over.
    bool battle_gone(const ImageViewRGB32& screen, WallClock now){
        const BattleSeen seen = BattleMenuDetector(COLOR_RED).detect(screen) ? BattleSeen::MENU
            : battle_shows(screen) ? BattleSeen::BATTLE : BattleSeen::NONE;
        m_end.seen(seen, now);
        if (!m_end.check_overworld(now)){
            return false;
        }
        m_last_overworld_check = now;
        return overworld_shows(m_console, m_context);
    }

    //  A or B, in turn. True once they have gone on too long: the walk,
    //  which finds the player again, takes over.
    bool press_blind(WallClock now){
        if (m_end.pressed_blind(now)){
            m_console.log("Battle: no battle menu after pressing A and B; leaving the rest to the walk.", COLOR_RED);
            return true;
        }
        m_console.log(std::format("Battle: no battle menu for {} s, pressing {}.",
            std::chrono::duration_cast<std::chrono::seconds>(now - m_last_menu).count(), m_press_a ? "A" : "B"));
        pbf_press_button(m_context, m_press_a ? BUTTON_A : BUTTON_B, 160ms, 200ms);
        m_press_a = !m_press_a;
        m_last_press = now;
        return false;
    }

    //  True once the battle is over: only when START opens the menu. The
    //  battle screens are gone during the opening slide-in (console B: "over
    //  after 0 turns" at the rival) and under a move prompt's YES/NO (console
    //  B, 2026-11-01, Viridian Forest: called over, then stuck at "Stop
    //  learning?").
    bool no_battle_screen(const ImageViewRGB32& screen, WallClock now){
        if (!m_seen_battle){
            pbf_wait(m_context, POLL);
            return false;
        }
        if (m_decisions.turns() > 0 && yes_no_cursor(screen)){
            answer_prompt(screen, now);
            return false;
        }
        if (now - m_last_battle > BATTLE_GONE && now - m_last_overworld_check > OVERWORLD_CHECK){
            m_last_overworld_check = now;
            if (overworld_shows(m_console, m_context)){
                return true;
            }
        }
        //  Oak's white text box in the rival's battle reads as no battle screen:
        //  B moves it on, and B does nothing on the overworld once the battle is over.
        if (now - m_last_press > 1500ms){
            pbf_press_button(m_context, BUTTON_B, 160ms, 200ms);
            m_last_press = now;
            return false;
        }
        pbf_wait(m_context, POLL);
        return false;
    }

    void check_time(WallClock now){
        if (!m_seen_battle && now - m_start > BATTLE_START_TIMEOUT){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT, "fight_battle(): no battle on screen.", m_console
            );
        }
        if (now - m_start > BATTLE_TIMEOUT){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT, "fight_battle(): the battle took over 10 minutes.", m_console
            );
        }
    }

    void act(BattleScreen seen, const ImageViewRGB32& screen, WallClock now){
        switch (seen){
        case BattleScreen::MENU:
            take_turn(screen);
            m_last_press = now;
            return;
        case BattleScreen::LEARN:
            if (m_decisions.turns() == 0){
                //  The prompts come after a turn; before one it is a misread.
                break;
            }
            answer_prompt(screen, now);
            return;
        case BattleScreen::ADVANCE:
            pbf_press_button(m_context, BUTTON_B, 160ms, 200ms);
            m_last_press = now;
            return;
        default:
            break;
        }
        if (now - m_last_press > STUCK){
            pbf_press_button(m_context, BUTTON_B, 160ms, 200ms);
            m_last_press = now;
        }
        pbf_wait(m_context, POLL);
    }

    //  The HP bar whenever the starter's box shows, so the last reading
    //  includes the battle's last hit. Not once another member is out.
    void read_hp(const ImageViewRGB32& screen){
        if (m_decisions.lost() || !BattlePokemonDetector(COLOR_RED).detect(screen)){
            return;
        }
        const PartyStatus status = read_party_status(screen);
        if (status.hp_seen){
            m_supplies.hurt = status.hp_red || status.hp_yellow;
            m_supplies.hp_red = status.hp_red;
            m_supplies.hp_fill = status.hp_fill;
        }
        m_supplies.status = status.status;
    }

    void answer_prompt(const ImageViewRGB32& screen, WallClock now){
        dump_debug_image(m_console, "AutoStory/battle", std::format("turn{}-yes-no", m_decisions.turns()), screen);
        const PromptAnswer answer = m_decisions.prompt(starter_fainted(screen));
        if (answer.forget_slot){
            m_console.log(std::format("Battle: learning the new move over move {}.", *answer.forget_slot + 1));
            if (forget_move(m_console, m_context, *answer.forget_slot) && answer.learned){
                m_supplies.lead = *answer.learned;
            }
        }else{
            m_console.log(std::format("Battle: answering {} to a YES/NO.", answer.answer == YesNo::YES ? "YES" : "NO"));
            pbf_press_button(m_context, answer.answer == YesNo::YES ? BUTTON_A : BUTTON_B, 160ms, 1500ms);
        }
        m_last_press = now;
    }

    void take_turn(const ImageViewRGB32& screen){
        m_faint_polls = 0;
        if (m_kind != BattleKind::TRAINER && m_decisions.turns() == 0){
            m_supplies.wild_foes = read_wild_foes(m_console.logger(), m_language, screen);
        }
        if (!m_foes.species().empty()){
            m_foes.at_menu(read_trainer_foe(m_console.logger(), m_language, screen, m_foes.species()));
            m_supplies.foe_out = m_foes.out();
        }
        if (m_refusal.refused(current_time())){
            m_console.log("Battle: the move list came back without a turn (Disable?); choosing another move.", COLOR_ORANGE);
            m_decisions.turn_done(TurnChoice::MOVE, TurnResult::REFUSED);
        }
        const PartyStatus status = read_party_status(screen);
        const TurnAction action = m_decisions.turn(status);
        m_evolution.turn_taken(action.choice);
        m_console.log(std::format(
            "Battle: turn {}, HP {} ({:.0f}% of the bar), {}, {} Potions, {} Antidotes, {} Parlyz Heals.",
            m_decisions.turns(), status.hp_red ? "red" : status.hp_yellow ? "yellow" : "green", 100 * status.hp_fill,
            status_name(status.status), m_supplies.potions, m_supplies.antidotes, m_supplies.parlyz_heals
        ));
        dump_debug_image(m_console, "AutoStory/battle", std::format("turn{}-menu", m_decisions.turns()), screen);
        switch (action.choice){
        case TurnChoice::RUN:
            m_console.log("Battle: RUN.");
            run_away(m_context);
            return;
        case TurnChoice::POTION:
            m_decisions.turn_done(action.choice, item_result(use_item(m_console, m_context, item_row(m_supplies, "potion"), "potion", m_checks, m_language)));
            return;
        case TurnChoice::ANTIDOTE:
            m_decisions.turn_done(action.choice, item_result(use_item(m_console, m_context, item_row(m_supplies, "antidote"), "antidote", m_checks, m_language)));
            return;
        case TurnChoice::MOVE:
            m_console.log(std::format("Battle: move {}.", action.slot + 1));
            m_refusal.move_chosen(current_time());
            use_move(m_context, action.slot);
            m_move_chosen = true;
            return;
        }
    }

    void watch_for_faint(const ImageViewRGB32& screen){
        if (m_decisions.turns() == 0 || m_decisions.lost()){
            return;
        }
        m_faint_polls = starter_fainted(screen) ? m_faint_polls + 1 : 0;
        if (m_faint_polls >= FAINT_POLLS){
            m_console.log("Battle: the starter fainted.", COLOR_RED);
            m_decisions.lead_fainted();
        }
    }

    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    Supplies& m_supplies;
    BattleDecisions m_decisions;
    EvolutionHold m_evolution;
    TrainerFoeTracker m_foes;
    BattleEndWatch m_end;
    MoveRefusalWatch m_refusal;
    const BattleKind m_kind;
    const StoryChecks m_checks;
    const Language m_language;
    const WallClock m_start;
    WallClock m_last_battle;
    WallClock m_last_press;
    WallClock m_last_menu;
    WallClock m_last_overworld_check;
    BattleScreen m_last_seen = BattleScreen::NONE;
    bool m_seen_battle = false;
    bool m_confirmed = false;
    bool m_press_a = true;
    //  A move was chosen and no battle menu has shown since.
    bool m_move_chosen = false;
    int m_faint_polls = 0;
};


//  From the overworld: the bag open on the Items pocket, cursor on its top.
void open_items_pocket(ConsoleHandle& console, ProControllerContext& context){
    open_bag_from_overworld(console, context);
    //  The bag ignores presses while it slides in.
    pbf_wait(context, 1000ms);
    for (int c = 0; c < 3; c++){
        pbf_press_dpad(context, DPAD_LEFT, 100ms, 500ms);
    }
    for (int c = 0; c < 20; c++){
        pbf_press_dpad(context, DPAD_UP, 100ms, 100ms);
    }
}

//  From the overworld: `item`, in `row` (from 0) of the Items pocket, on
//  the starter. Returns whether it was used (THOROUGH reads the row first).
//  Not run on the console yet.
bool use_item_on_overworld(
    ConsoleHandle& console, ProControllerContext& context,
    int row, const std::string& item, StoryChecks checks, Language language
){
    console.log(std::format("Story: using a {} from the bag (Items pocket, row {}).", item, row + 1));
    open_items_pocket(console, context);
    for (int c = 0; c < row; c++){
        pbf_press_dpad(context, DPAD_DOWN, 100ms, 300ms);
    }
    if (!bag_row_is(console, context, row, item, checks, language)){
        mash_until_overworld(console, context, AFTER_BATTLE_TEXT);
        return false;
    }
    //  The item, USE, the starter, then its message.
    pbf_press_button(context, BUTTON_A, 160ms, 800ms);
    pbf_press_button(context, BUTTON_A, 160ms, 1500ms);
    pbf_press_button(context, BUTTON_A, 160ms, 2500ms);
    pbf_press_button(context, BUTTON_B, 160ms, 1500ms);
    mash_until_overworld(console, context, AFTER_BATTLE_TEXT);
    return true;
}

void fail_on_blackout(ConsoleHandle& console, BattleEnd end){
    if (end == BattleEnd::LOST){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::NO_ERROR_REPORT, "Story: blackout: the starter fainted in a battle on the way.", console
        );
    }
}


//  A wild battle, seen by its flashes or, when they were missed, by "Wild
//  <Pokemon> appeared!".
class StoryWildBattle : public OverworldInterrupt{
public:
    StoryWildBattle(Supplies& supplies, bool fight, bool late, StoryChecks checks, Language language)
        : m_supplies(supplies)
        , m_checks(checks)
        , m_language(language)
        , m_fight(fight)
        , m_late(late)
    {}
    virtual std::string name() const override{
        return m_late ? "a wild battle (seen late)" : "a wild battle";
    }
    virtual std::unique_ptr<VisualInferenceCallback> make_watcher() const override{
        if (m_late){
            return std::make_unique<BattleScreenWatcher>(COLOR_RED);
        }
        return std::make_unique<BattleFlashWatcher>(COLOR_RED);
    }
    //  As the walker's own wild battle interrupts.
    virtual Milliseconds detection_delay() const override{
        return m_late ? OverworldWalkSteps::LATE_WILD_BATTLE_DELAY : 300ms;
    }
    //  Run from or fought by the battle loop, which returns only once START
    //  opens on the overworld, so nothing of the battle is left before the
    //  next leg.
    virtual void clear(ConsoleHandle& console, ProControllerContext& context) const override{
        const BattleKind kind = m_fight ? BattleKind::WILD_FIGHT : BattleKind::WILD_RUN;
        fail_on_blackout(console, fight_battle(console, context, m_supplies, kind, m_checks, m_language));
        recover_after_battle(console, context, m_supplies, m_checks, m_language);
    }

private:
    Supplies& m_supplies;
    const StoryChecks m_checks;
    const Language m_language;
    const bool m_fight;
    const bool m_late;
};

//  A message: read to its end, then a trainer's battle if one starts.
class StoryMessage : public OverworldInterrupt{
public:
    StoryMessage(Supplies& supplies, StoryChecks checks, Language language)
        : m_supplies(supplies)
        , m_checks(checks)
        , m_language(language)
    {}
    virtual std::string name() const override{
        return "a message";
    }
    virtual std::unique_ptr<VisualInferenceCallback> make_watcher() const override{
        return std::make_unique<WhiteDialogWatcher>(COLOR_RED);
    }
    virtual Milliseconds detection_delay() const override{
        return 400ms;
    }
    virtual void clear(ConsoleHandle& console, ProControllerContext& context) const override{
        if (advance_text(console, context, AFTER_BATTLE_TEXT, true) == TextEnd::OVERWORLD){
            return;
        }
        console.log("Story: a trainer's battle after a message on the overworld.");
        fail_on_blackout(console, fight_battle(console, context, m_supplies, BattleKind::TRAINER, m_checks, m_language));
        mash_until_overworld(console, context, AFTER_BATTLE_TEXT);
        recover_after_battle(console, context, m_supplies, m_checks, m_language);
    }

private:
    Supplies& m_supplies;
    const StoryChecks m_checks;
    const Language m_language;
};

}


const char* status_name(StatusCondition status){
    switch (status){
    case StatusCondition::NONE:
        return "no status";
    case StatusCondition::POISON:
        return "PSN";
    case StatusCondition::PARALYSIS:
        return "PAR";
    case StatusCondition::OTHER:
        return "a status";
    }
    return "?";
}

PartyStatus read_party_status(const ImageViewRGB32& screen){
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    const HpBarReading bar = read_player_hp_bar(game);
    PartyStatus status;
    status.hp_seen = bar.seen;
    status.hp_yellow = bar.seen && bar.color == HpBarColor::YELLOW;
    status.hp_red = bar.seen && bar.color == HpBarColor::RED;
    status.hp_fill = bar.fill;
    status.status = read_status_badge(extract_box_reference(game, STATUS_BADGE_BOX));
    return status;
}

bool battle_shows(const ImageViewRGB32& screen){
    const BattleTextScreen text = read_text_screen(screen);
    return text.hp_box || text.yes_no;
}

//  The count the story keeps for `item`, or null for an item it doesn't count.
static int* count_slot(Supplies& supplies, const std::string& item){
    if (item == "potion"){
        return &supplies.potions;
    }
    if (item == "antidote"){
        return &supplies.antidotes;
    }
    if (item == "parlyz-heal"){
        return &supplies.parlyz_heals;
    }
    return nullptr;
}

int item_count(const Supplies& supplies, const std::string& item){
    if (item == "potion"){
        return supplies.potions;
    }
    if (item == "antidote"){
        return supplies.antidotes;
    }
    if (item == "parlyz-heal"){
        return supplies.parlyz_heals;
    }
    return 0;
}

//  Whether the story keeps a count for `item`.
static bool counted(const std::string& item){
    Supplies probe;
    return count_slot(probe, item) != nullptr;
}

int item_row(const Supplies& supplies, const std::string& item){
    int row = 0;
    for (const std::string& slot : supplies.pocket){
        if (counted(slot) && item_count(supplies, slot) <= 0){
            continue;
        }
        if (slot == item){
            return row;
        }
        row++;
    }
    return -1;
}

std::vector<std::string> set_from_bag(Supplies& supplies, const std::vector<BagRow>& rows){
    const Supplies before = supplies;
    supplies.potions = 0;
    supplies.antidotes = 0;
    supplies.parlyz_heals = 0;
    supplies.pocket.clear();
    std::vector<std::string> unread;
    for (const BagRow& row : rows){
        supplies.pocket.push_back(row.item);
        int* slot = count_slot(supplies, row.item);
        if (slot == nullptr){
            continue;
        }
        if (row.count > 0){
            *slot = row.count;
            continue;
        }
        //  Taking an unread count for none left the starter unhealed at
        //  9 HP until it fainted (console B, 2026-11-02).
        *slot = std::max(item_count(before, row.item), 1);
        unread.push_back(row.item);
    }
    return unread;
}

std::vector<std::string> set_from_loaded_bag(Supplies& supplies, Supplies& saved, const std::vector<BagRow>& rows){
    if (rows.empty()){
        return {};
    }
    std::vector<std::string> unread = set_from_bag(supplies, rows);
    saved.potions = supplies.potions;
    saved.antidotes = supplies.antidotes;
    saved.parlyz_heals = supplies.parlyz_heals;
    saved.pocket = supplies.pocket;
    return unread;
}

std::vector<BagRow> read_items_pocket(ConsoleHandle& console, ProControllerContext& context, Language language){
    open_items_pocket(console, context);
    context.wait_for_all_requests();
    VideoSnapshot screen = console.video().snapshot();
    dump_debug_image(console, "AutoStory/bag", "items-pocket", screen);
    std::vector<BagRow> rows = screen ? read_bag_pocket(console, language, screen) : std::vector<BagRow>{};
    std::string text;
    for (const BagRow& row : rows){
        text += std::format(" {} x{};", row.item, row.count);
    }
    console.log("Bag: Items pocket:" + (text.empty() ? std::string(" nothing read.") : text));
    mash_until_overworld(console, context, AFTER_BATTLE_TEXT);
    return rows;
}

void recover_after_battle(
    ConsoleHandle& console, ProControllerContext& context,
    Supplies& supplies, StoryChecks checks, Language language
){
    if (supplies.status == StatusCondition::POISON && supplies.antidotes > 0){
        if (use_item_on_overworld(console, context, item_row(supplies, "antidote"), "antidote", checks, language)){
            supplies.antidotes--;
            supplies.status = StatusCondition::NONE;
        }
    }else if (supplies.status == StatusCondition::PARALYSIS && supplies.parlyz_heals > 0){
        if (use_item_on_overworld(console, context, item_row(supplies, "parlyz-heal"), "parlyz-heal", checks, language)){
            supplies.parlyz_heals--;
            supplies.status = StatusCondition::NONE;
        }
    }
    //  One Potion (20 HP) takes a starter of these levels (about 30 HP)
    //  from red to green; the bar can't be read again on the overworld.
    if (supplies.hurt && supplies.potions > 0){
        if (use_item_on_overworld(console, context, item_row(supplies, "potion"), "potion", checks, language)){
            supplies.potions--;
            supplies.hurt = false;
            supplies.hp_red = false;
            supplies.hp_fill = 1.0;
        }
    }
}

BattleEnd fight_battle(
    ConsoleHandle& console, ProControllerContext& context,
    Supplies& supplies, BattleKind kind, StoryChecks checks, Language language
){
    return BattleLoop(console, context, supplies, kind, checks, language).run();
}

OverworldInterruptSet story_interrupts(Supplies& supplies, bool fight_wild, StoryChecks checks, Language language){
    //  Battles first: their screens are checked before the message box's.
    OverworldInterruptSet ret;
    ret.wild.emplace_back(std::make_unique<StoryWildBattle>(supplies, fight_wild, false, checks, language));
    ret.wild.emplace_back(std::make_unique<StoryWildBattle>(supplies, fight_wild, true, checks, language));
    ret.wild.emplace_back(std::make_unique<StoryMessage>(supplies, checks, language));
    ret.other.emplace_back(std::make_unique<StoryMessage>(supplies, checks, language));
    return ret;
}



}
}
}
