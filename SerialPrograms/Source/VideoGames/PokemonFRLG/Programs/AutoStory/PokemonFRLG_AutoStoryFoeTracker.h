/*  AutoStory Foe Tracker
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which of a trainer's Pokemon is out, in a story battle whose trainer
 *  the story knows, so the lead's move is chosen against that one. The
 *  name in the foe's HP box is read at each battle menu
 *  (read_trainer_foe()). When it isn't read, the tracker goes by the game:
 *  the party's first Pokemon opens, a foe faints when its box is gone
 *  while the lead's stays, and the next one is the AI's choice
 *  (ai_next_foe()) against the lead.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryFoeTracker_H
#define PokemonAutomation_PokemonFRLG_AutoStoryFoeTracker_H

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "PokemonFRLG_AutoStoryBattleSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Which HP boxes a battle screen shows.
struct BattleBoxes{
    bool lead = false;
    bool foe = false;
};

class TrainerFoeTracker{
public:
    //  `trainer`: pret's constant, or "" when the battle's trainer isn't
    //  known (then nothing is tracked). `lead`: the story's lead, for the
    //  AI's next Pokemon.
    TrainerFoeTracker(std::string_view trainer, const PartyMember& lead);

    //  The species in the party, for reading the name.
    const std::vector<std::string>& species() const{ return m_species; }

    //  At each look at a battle screen.
    void boxes_seen(BattleBoxes boxes);

    //  At a battle menu, with the foe's species as read ("" if not read).
    void at_menu(std::string_view read);

    //  The party slot out; none when the trainer isn't known.
    std::optional<size_t> out() const;

    //  The party slots seen fainted.
    const std::set<size_t>& fainted() const{ return m_fainted; }

private:
    std::vector<SimBattler> m_party;
    std::vector<std::string> m_species;
    std::optional<SimBattler> m_lead;
    std::set<size_t> m_fainted;
    size_t m_out = 0;
    int m_gone_polls = 0;
    //  A foe fainted, and no menu since: the next one isn't known yet.
    bool m_next_due = false;
};



}
}
}
#endif
