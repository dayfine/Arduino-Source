/*  Battle Screen Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A battle on screen: the battle text box, and with it the opponent's HP
 *  box, the player's or the battle menu. The text box alone also reads on
 *  the black below a small indoor map (console B, 2026-11-01: Oak's Lab's
 *  mat, Viridian Forest's north gate).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_BattleScreenDetector_H
#define PokemonAutomation_PokemonFRLG_BattleScreenDetector_H

#include <chrono>
#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/VisualDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattlePokemonDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


class BattleScreenDetector : public StaticScreenDetector{
public:
    BattleScreenDetector(Color color = COLOR_RED);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    BattleDialogDetector m_dialog;
    BattleMenuDetector m_menu;
    BattleOpponentDetector m_opponent;
    BattlePokemonDetector m_player;
};

class BattleScreenWatcher : public DetectorToFinder<BattleScreenDetector>{
public:
    BattleScreenWatcher(Color color = COLOR_RED)
        : DetectorToFinder("BattleScreenWatcher", std::chrono::milliseconds(250), color)
    {}
};


//  A YES/NO box in a battle (a move to learn): BattleLearnDialogDetector,
//  and the box's slate-blue left frame. The detector alone reads white
//  where the box would be and the teal text box, which a foe's send-out
//  flash shows on the S.S. Anne's white floor (console B, 2026-11-03, m68:
//  "RIVAL GREEN sent out RATICATE!" and "...KADABRA!" taken for a move
//  prompt, NO at one and YES at the other).
bool battle_yes_no_shows(const ImageViewRGB32& screen);



}
}
}
#endif
