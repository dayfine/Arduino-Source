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
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattlePokemonDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleScreenDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattle.h"

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
//  A wild Pokemon is fought (WILD_FIGHT) only with green HP and this many
//  Potions left; otherwise it is run from.
constexpr int MIN_POTIONS_TO_FIGHT = 2;
//  RUN this many times ("Can't escape!") before fighting instead.
constexpr int RUN_TRIES = 4;
//  No battle menu or HP box this long after a battle was seen: there was
//  none (console B, 2026-11-01, Viridian Forest's north gate mat).
constexpr Milliseconds NOT_A_BATTLE = 15000ms;
//  PSN read at this many turns in a row before an Antidote.
constexpr int PSN_READS = 2;
//  Text and the move learn prompts after a battle, on the overworld.
constexpr Milliseconds AFTER_BATTLE_TEXT = 60000ms;

//  In the player's box, as fractions of the 240x160 screen: the HP bar's
//  fill, which grows from its left end, and the status badge left of the
//  HP numbers. The box bobs by a pixel; these keep a pixel inside.
const ImageFloatBox HP_BAR_BOX(174.0 / 240, 91.0 / 160, 48.0 / 240, 2.0 / 160);
const ImageFloatBox STATUS_BADGE_BOX(146.0 / 240, 99.0 / 160, 12.0 / 240, 4.0 / 160);


struct HpCounts{
    size_t green = 0;
    size_t yellow = 0;
    size_t red = 0;
    size_t total = 0;
};

HpCounts count_hp_colors(const ImageViewRGB32& image){
    HpCounts counts;
    for (size_t y = 0; y < image.height(); y++){
        for (size_t x = 0; x < image.width(); x++){
            const uint32_t pixel = image.pixel(x, y);
            const int r = (pixel >> 16) & 0xff;
            const int g = (pixel >> 8) & 0xff;
            const int b = pixel & 0xff;
            counts.total++;
            if (g > 200 && r < 160 && b > 120 && b < 210){
                counts.green++;
            }else if (r > 220 && g > 180 && b < 80){
                counts.yellow++;
            }else if (r > 220 && g > 80 && g < 150 && b < 110){
                counts.red++;
            }
        }
    }
    return counts;
}

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

//  FIGHT, then the bottom left move (Bubble), or Tackle while that slot is
//  empty: the cursor doesn't move onto an empty slot.
void use_move(ProControllerContext& context){
    tap(context, DPAD_LEFT);
    tap(context, DPAD_UP);
    pbf_press_button(context, BUTTON_A, 160ms, 1000ms);
    tap(context, DPAD_LEFT);
    tap(context, DPAD_UP);
    tap(context, DPAD_DOWN);
    pbf_press_button(context, BUTTON_A, 160ms, 500ms);
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
        m_console.log(std::format("Battle: over after {} turns{}.", m_turns, m_lost ? ", the starter fainted" : ""));
        return m_lost ? BattleEnd::LOST : BattleEnd::WON;
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
        const BattleScreen seen = read_battle_screen(screen, m_confirmed);
        if (seen != m_last_seen){
            m_console.log(std::format("Battle: {} (turn {}).", screen_name(seen), m_turns));
            m_last_seen = seen;
        }
        if (seen == BattleScreen::MENU){
            m_last_menu = now;
        }else if (now - m_last_menu > NO_MENU && now - m_last_battle < NO_MENU_NEAR && now - m_last_press > 1500ms){
            //  Something waits that none of the battle screens knows.
            m_console.log(std::format("Battle: no battle menu for {} s, pressing {}.",
                std::chrono::duration_cast<std::chrono::seconds>(now - m_last_menu).count(), m_press_a ? "A" : "B"));
            pbf_press_button(m_context, m_press_a ? BUTTON_A : BUTTON_B, 160ms, 200ms);
            m_press_a = !m_press_a;
            m_last_press = now;
            return false;
        }
        if (seen == BattleScreen::NONE){
            return no_battle_screen(screen, now);
        }
        m_seen_battle = true;
        m_last_battle = now;
        read_hp(screen);
        act(seen, screen, now);
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
        if (m_turns > 0 && yes_no_cursor(screen)){
            answer_learn_prompt(now);
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
            if (m_turns == 0){
                //  The learn prompts come after a turn; before one it is a misread.
                break;
            }
            answer_learn_prompt(now);
            return;
        case BattleScreen::ADVANCE:
            pbf_press_button(m_context, BUTTON_B, 160ms, 200ms);
            m_last_press = now;
            return;
        default:
            break;
        }
        watch_for_faint(screen);
        if (now - m_last_press > STUCK){
            pbf_press_button(m_context, BUTTON_B, 160ms, 200ms);
            m_last_press = now;
        }
        pbf_wait(m_context, POLL);
    }

    //  The HP bar whenever the starter's box shows, so the last reading
    //  includes the battle's last hit.
    void read_hp(const ImageViewRGB32& screen){
        if (!BattlePokemonDetector(COLOR_RED).detect(screen)){
            return;
        }
        const PartyStatus status = read_party_status(screen);
        if (status.hp_seen){
            m_supplies.hurt = status.hp_red || status.hp_yellow;
            m_supplies.hp_red = status.hp_red;
        }
        m_supplies.status = status.status;
    }

    //  "Delete a move to make room?" NO, then "Stop learning?" YES.
    void answer_learn_prompt(WallClock now){
        m_console.log(m_learn_prompts % 2 == 0 ? "Battle: not deleting a move." : "Battle: stop learning.");
        pbf_press_button(m_context, m_learn_prompts % 2 == 0 ? BUTTON_B : BUTTON_A, 160ms, 1500ms);
        m_learn_prompts++;
        m_last_press = now;
    }

    void take_turn(const ImageViewRGB32& screen){
        m_turns++;
        m_faint_polls = 0;
        const PartyStatus status = read_party_status(screen);
        const bool hurt = status.hp_red || status.hp_yellow;
        m_supplies.hurt = hurt;
        m_supplies.status = status.status;
        m_psn_reads = status.status == StatusCondition::POISON ? m_psn_reads + 1 : 0;
        m_console.log(std::format(
            "Battle: turn {}, HP {}, {}, {} Potions, {} Antidotes, {} Parlyz Heals.",
            m_turns, status.hp_red ? "red" : status.hp_yellow ? "yellow" : "green",
            status_name(status.status), m_supplies.potions, m_supplies.antidotes, m_supplies.parlyz_heals
        ));
        //  A wild Pokemon is fought for EXP only at full strength: fighting
        //  every one in Viridian Forest used up all 4 Potions and then the
        //  starter (console B, 2026-11-01). With a status, it is run from too.
        const bool weak = hurt || status.status != StatusCondition::NONE || m_supplies.potions < MIN_POTIONS_TO_FIGHT;
        const bool run = m_kind == BattleKind::WILD_RUN || (m_kind == BattleKind::WILD_FIGHT && weak);
        if (run && m_run_tries < RUN_TRIES){
            m_run_tries++;
            m_console.log(std::format("Battle: RUN (try {}).", m_run_tries));
            run_away(m_context);
            return;
        }
        if (!m_items_off && hurt && m_supplies.potions > 0){
            if (use_item(m_console, m_context, item_row(m_supplies, "potion"), "potion", m_checks, m_language)){
                m_supplies.potions--;
                m_supplies.hurt = false;
                m_supplies.hp_red = false;
            }else{
                m_items_off = true;
            }
        }else if (!m_items_off && m_psn_reads >= PSN_READS && !m_antidote_used && m_supplies.antidotes > 0){
            m_antidote_used = true;
            if (use_item(m_console, m_context, item_row(m_supplies, "antidote"), "antidote", m_checks, m_language)){
                m_supplies.antidotes--;
            }else{
                m_items_off = true;
            }
        }else{
            use_move(m_context);
        }
    }

    void watch_for_faint(const ImageViewRGB32& screen){
        if (m_turns == 0 || m_lost){
            return;
        }
        m_faint_polls = starter_fainted(screen) ? m_faint_polls + 1 : 0;
        if (m_faint_polls >= FAINT_POLLS){
            m_console.log("Battle: the starter fainted.", COLOR_RED);
            m_lost = true;
        }
    }

    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    Supplies& m_supplies;
    const BattleKind m_kind;
    const StoryChecks m_checks;
    const Language m_language;
    //  An item read as another in the bag: none is used again this battle.
    bool m_items_off = false;
    const WallClock m_start;
    WallClock m_last_battle;
    WallClock m_last_press;
    WallClock m_last_menu;
    WallClock m_last_overworld_check;
    BattleScreen m_last_seen = BattleScreen::NONE;
    bool m_seen_battle = false;
    bool m_confirmed = false;
    bool m_lost = false;
    bool m_press_a = true;
    bool m_antidote_used = false;
    int m_psn_reads = 0;
    int m_run_tries = 0;
    size_t m_turns = 0;
    size_t m_learn_prompts = 0;
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
        return m_late ? 2000ms : 300ms;
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
    const HpCounts counts = count_hp_colors(extract_box_reference(game, HP_BAR_BOX));
    //  At 1 HP the red sliver was 32 of the box's 4000 pixels (console B,
    //  2026-11-01, 1/25 HP).
    const size_t seen = counts.total / 200 + 1;
    PartyStatus status;
    const size_t most = std::max({counts.green, counts.yellow, counts.red});
    status.hp_seen = most >= seen;
    status.hp_yellow = status.hp_seen && counts.yellow == most;
    status.hp_red = status.hp_seen && !status.hp_yellow && counts.red == most;
    status.status = read_status_badge(extract_box_reference(game, STATUS_BADGE_BOX));
    return status;
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

int item_row(const Supplies& supplies, const std::string& item){
    int row = 0;
    for (const std::string& slot : supplies.pocket){
        if (item_count(supplies, slot) <= 0){
            continue;
        }
        if (slot == item){
            return row;
        }
        row++;
    }
    return -1;
}

void set_from_bag(Supplies& supplies, const std::vector<BagRow>& rows){
    supplies.potions = 0;
    supplies.antidotes = 0;
    supplies.parlyz_heals = 0;
    supplies.pocket.clear();
    for (const BagRow& row : rows){
        supplies.pocket.push_back(row.item);
        int* slot = count_slot(supplies, row.item);
        if (slot != nullptr){
            *slot = std::max(row.count, 0);
        }
    }
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
