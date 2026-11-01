/*  AutoStory Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <memory>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleFlashWatcher.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattlePokemonDetector.h"
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
//  No battle screen for this long: the battle is over.
constexpr Milliseconds BATTLE_GONE = 4000ms;
//  A battle screen with neither an arrow nor a menu for this long: B, for
//  a prompt the detectors miss.
constexpr Milliseconds STUCK = 8000ms;
//  Seen this many polls in a row: the starter fainted.
constexpr int FAINT_POLLS = 3;
//  Text and the move learn prompts after a battle, on the overworld.
constexpr Milliseconds AFTER_BATTLE_TEXT = 60000ms;

//  Inside the player's status box (name, level, HP bar, status), which
//  BattlePokemonDetector frames. Not calibrated on the console yet.
const ImageFloatBox PLAYER_STATUS_BOX(0.600, 0.500, 0.340, 0.165);


struct ColorCounts{
    size_t green = 0;
    size_t yellow = 0;
    size_t red = 0;
    size_t purple = 0;
    size_t total = 0;
};

//  The HP bar's three colors and the PSN badge's purple. Guessed from the
//  game's palette, not measured.
ColorCounts count_colors(const ImageViewRGB32& image){
    ColorCounts counts;
    for (size_t y = 0; y < image.height(); y++){
        for (size_t x = 0; x < image.width(); x++){
            const uint32_t pixel = image.pixel(x, y);
            const int r = (pixel >> 16) & 0xff;
            const int g = (pixel >> 8) & 0xff;
            const int b = pixel & 0xff;
            counts.total++;
            if (g > 180 && r < 150 && b > 90 && b < 210){
                counts.green++;
            }else if (r > 190 && g > 160 && b < 90){
                counts.yellow++;
            }else if (r > 190 && g < 110 && b < 90){
                counts.red++;
            }else if (r > 140 && b > 140 && g < 110){
                counts.purple++;
            }
        }
    }
    return counts;
}


enum class BattleScreen{
    MENU,       //  "What will <Pokemon> do?" with FIGHT/BAG/POKEMON/RUN
    LEARN,      //  a YES/NO about a move to learn
    ADVANCE,    //  battle text waiting for a button
    OTHER,      //  the battle, busy
    NONE,       //  no battle on screen
};

BattleScreen read_battle_screen(const ImageViewRGB32& screen){
    if (BattleMenuDetector(COLOR_RED).detect(screen)){
        return BattleScreen::MENU;
    }
    if (BattleLearnDialogDetector(COLOR_RED).detect(screen)){
        return BattleScreen::LEARN;
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

//  From the battle menu: the item in `row` (from 0) of the Items pocket, on
//  the starter. The bag opens on the pocket used last; Items is the first.
//  Not run on the console yet.
void use_item(ConsoleHandle& console, ProControllerContext& context, int row){
    console.log(std::format("Battle: using the item in row {} of the Items pocket.", row + 1));
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
    //  The item, USE, then the starter in the party list.
    pbf_press_button(context, BUTTON_A, 160ms, 800ms);
    pbf_press_button(context, BUTTON_A, 160ms, 1500ms);
    pbf_press_button(context, BUTTON_A, 160ms, 1000ms);
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
    BattleLoop(ConsoleHandle& console, ProControllerContext& context, Supplies& supplies, bool trainer)
        : m_console(console)
        , m_context(context)
        , m_supplies(supplies)
        , m_trainer(trainer)
        , m_start(current_time())
        , m_last_battle(m_start)
        , m_last_press(m_start)
    {}

    BattleEnd run(){
        m_console.log(m_trainer ? "Battle: fighting a trainer." : "Battle: fighting a wild Pokemon.");
        while (!step()){}
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
        const BattleScreen seen = read_battle_screen(screen);
        if (seen == BattleScreen::NONE){
            if (m_seen_battle && now - m_last_battle > BATTLE_GONE){
                return true;
            }
            pbf_wait(m_context, POLL);
            return false;
        }
        m_seen_battle = true;
        m_last_battle = now;
        act(seen, screen, now);
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
            //  "Delete a move to make room?" NO, then "Stop learning?" YES.
            m_console.log(m_learn_prompts % 2 == 0 ? "Battle: not deleting a move." : "Battle: stop learning.");
            pbf_press_button(m_context, m_learn_prompts % 2 == 0 ? BUTTON_B : BUTTON_A, 160ms, 1500ms);
            m_learn_prompts++;
            m_last_press = now;
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

    void take_turn(const ImageViewRGB32& screen){
        m_turns++;
        m_faint_polls = 0;
        const PartyStatus status = read_party_status(screen);
        const bool hurt = status.hp_red || (m_trainer && status.hp_yellow);
        if (hurt && m_supplies.potions > 0){
            m_supplies.potions--;
            use_item(m_console, m_context, 0);
        }else if (status.poisoned && m_supplies.antidotes > 0){
            m_supplies.antidotes--;
            use_item(m_console, m_context, m_supplies.potions > 0 ? 1 : 0);
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
    const bool m_trainer;
    const WallClock m_start;
    WallClock m_last_battle;
    WallClock m_last_press;
    bool m_seen_battle = false;
    bool m_lost = false;
    size_t m_turns = 0;
    size_t m_learn_prompts = 0;
    int m_faint_polls = 0;
};


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
    StoryWildBattle(Supplies& supplies, bool fight, bool late)
        : m_supplies(supplies)
        , m_fight(fight)
        , m_late(late)
    {}
    virtual std::string name() const override{
        return m_late ? "a wild battle (seen late)" : "a wild battle";
    }
    virtual std::unique_ptr<VisualInferenceCallback> make_watcher() const override{
        if (m_late){
            return std::make_unique<BattleDialogWatcher>(COLOR_RED);
        }
        return std::make_unique<BattleFlashWatcher>(COLOR_RED);
    }
    //  As the walker's own wild battle interrupts.
    virtual Milliseconds detection_delay() const override{
        return m_late ? 2000ms : 300ms;
    }
    virtual void clear(ConsoleHandle& console, ProControllerContext& context) const override{
        if (!m_fight){
            flee_battle(console, context);
            return;
        }
        fail_on_blackout(console, fight_battle(console, context, m_supplies, false));
        pbf_wait(context, 1000ms);
        context.wait_for_all_requests();
    }

private:
    Supplies& m_supplies;
    const bool m_fight;
    const bool m_late;
};

//  A message: read to its end, then a trainer's battle if one starts.
class StoryMessage : public OverworldInterrupt{
public:
    explicit StoryMessage(Supplies& supplies)
        : m_supplies(supplies)
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
        fail_on_blackout(console, fight_battle(console, context, m_supplies, true));
        mash_until_overworld(console, context, AFTER_BATTLE_TEXT);
    }

private:
    Supplies& m_supplies;
};

}


PartyStatus read_party_status(const ImageViewRGB32& screen){
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    const ColorCounts counts = count_colors(extract_box_reference(game, PLAYER_STATUS_BOX));
    //  A full bar fills a few percent of the box; a sliver of one, much less.
    const size_t seen = counts.total / 1000 + 1;
    PartyStatus status;
    const bool green = counts.green > seen;
    status.hp_yellow = !green && counts.yellow > seen;
    status.hp_red = !green && !status.hp_yellow && counts.red > seen;
    status.poisoned = counts.purple > seen;
    return status;
}

BattleEnd fight_battle(ConsoleHandle& console, ProControllerContext& context, Supplies& supplies, bool trainer){
    return BattleLoop(console, context, supplies, trainer).run();
}

OverworldInterruptSet story_interrupts(Supplies& supplies, bool fight_wild){
    //  Battles first: their screens are checked before the message box's.
    OverworldInterruptSet ret;
    ret.wild.emplace_back(std::make_unique<StoryWildBattle>(supplies, fight_wild, false));
    ret.wild.emplace_back(std::make_unique<StoryWildBattle>(supplies, fight_wild, true));
    ret.wild.emplace_back(std::make_unique<StoryMessage>(supplies));
    ret.other.emplace_back(std::make_unique<StoryMessage>(supplies));
    return ret;
}



}
}
}
