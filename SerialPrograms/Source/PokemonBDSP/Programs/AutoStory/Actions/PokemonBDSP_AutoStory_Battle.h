/*  BDSP AutoStory - Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 *  Fighting a battle with the best move on the menu: the move names are read
 *  and scored by power, so it works whatever the starter knows.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Battle_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Battle_H

#include <string>
#include "CommonFramework/Tools/VideoStream.h"
#include "CommonTools/VisualDetector.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  A battle's text box, as a watcher.
class BattleDialogWatcher : public DetectorToFinder<BattleDialogDetector>{
public:
    BattleDialogWatcher(Color color = COLOR_RED);
};

//  One opponent, or two (a tag or twin battle, which asks for a target).
enum class BattleFormat{
    SINGLES,
    DOUBLES,
};

//  Whether Potions are working in a battle. A Potion takes the turn; when our
//  HP at the next battle menu is no higher than before it, the heal is lost to
//  the foe's hits (or the Bag flow did not use it), and more Potions would only
//  hand over turns (Roark, lab-B 2026-11-12 23:49: 26%, 26%, 29%, 29% over four
//  Potions). FRLG's battle policy skips a futile heal the same way, from the
//  foe's mean hit; BDSP has no foe model, so the HP read after the turn is the
//  net of the heal and the hit. After a Potion that gained nothing, none is
//  used again this battle: the fight goes on with moves.
class PotionLedger{
public:
    //  At a battle menu with our HP read (-1 unread). Settles the last
    //  Potion, if any: it gained when `hp` is above the HP it was used at.
    void observe(double hp){
        if (m_before < 0 || hp < 0){
            //  Nothing to settle, or an unread bar: no verdict either way.
            return;
        }
        m_stalled = hp <= m_before;
        m_before = -1;
    }
    //  Whether a Potion may be used now.
    bool may_use() const{
        return !m_stalled && !m_out && m_used < MAX_POTIONS;
    }
    //  A Potion was used while our HP read `hp_before` (-1 unread).
    void used(double hp_before){
        m_used++;
        m_before = hp_before;
    }
    //  The Bag has no Potion left.
    void out_of_potions(){
        m_out = true;
    }
    //  Whether the last Potion gained nothing (log text for the caller).
    bool stalled() const{
        return m_stalled;
    }
    size_t count() const{
        return m_used;
    }
    //  Whether a Potion is waiting for the HP read that settles it.
    bool pending() const{
        return m_before >= 0;
    }

    static constexpr size_t MAX_POTIONS = 4;

private:
    size_t m_used = 0;
    double m_before = -1;
    bool m_stalled = false;
    bool m_out = false;
};

//  The speeches after a battle, to their end: talk_over(), B through the
//  lines until the overworld is quiet. Roark's badge lines pause for the
//  fanfare, and someone may walk off before the next line, so a moment
//  with no box is not the end. Fails with a screenshot after two minutes.
void confirm_battle_overworld(VideoStream& stream, ProControllerContext& context, const std::string& label);

//  A story battle that may go badly: the best move each turn, Potions
//  under 35 %, the next Pokemon when one faints, and "Keep old moves"
//  on level-up prompts. `doubles` confirms the default target each
//  turn in DOUBLES. Returns once the battle has ended,
//  won or lost; after a loss we stand in the last Pokemon Center.
void fight_story_battle(VideoStream& stream, ProControllerContext& context, const std::string& label, BattleFormat format);

//  Any trainer battle, whatever the starter: the best move on the menu each
//  turn until the battle ends, then the text after it.
void fight_trainer(VideoStream& stream, ProControllerContext& context, const std::string& label);

//  A wild battle at its battle menu, fought with the best move until it ends.
void fight_wild_battle(VideoStream& stream, ProControllerContext& context, const std::string& label);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
