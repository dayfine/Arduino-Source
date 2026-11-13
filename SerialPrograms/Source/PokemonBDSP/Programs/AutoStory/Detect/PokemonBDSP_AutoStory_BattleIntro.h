/*  BDSP AutoStory - Battle Intro
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Who a trainer battle is against, read from the text it opens with: "You
 *  are challenged by Gym Leader Roark!" against "... by Youngster <name>!"
 *  for a gym trainer. Position and time walked only guess who stopped us
 *  (01.04 took Roark for a trainer after a reload, lab-B 2026-11-12 23:49);
 *  the text on the screen says it.
 *
 *  English only: other languages have no text reader (story_reads_text()),
 *  so the opponent stays UNREAD there.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_BattleIntro_H
#define PokemonAutomation_PokemonBDSP_AutoStory_BattleIntro_H

#include <string>
#include <string_view>
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Time.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  What the intro text said of the opponent.
enum class BattleOpponent{
    UNREAD,     //  No intro text was read (nothing, a garble, or another language).
    LEADER,     //  "Gym Leader <name>".
    TRAINER,    //  "challenged by <class> <name>", with no Leader in it.
};

//  For the log: "gym leader", "trainer" or "not read".
std::string_view battle_opponent_name(BattleOpponent opponent);

//  One OCR read of the intro's text box. Empty or unrelated text is UNREAD.
//  A read may be a line still being typed out, so TRAINER here is not final:
//  BattleIntroReading decides that.
BattleOpponent classify_battle_intro(std::string_view text);

//  The reads of one battle's intro, to a verdict. A leader's name settles it
//  at once (the line's words "Gym Leader" come before the name, and no
//  trainer's read has them); a trainer is settled by the same text twice in a
//  row, which says the line is fully typed.
class BattleIntroReading{
public:
    //  Takes one OCR read; returns the verdict so far.
    BattleOpponent add(std::string_view text);
    BattleOpponent verdict() const{
        return m_verdict;
    }

private:
    BattleOpponent m_verdict = BattleOpponent::UNREAD;
    std::string m_last_trainer_text;
};


//  Reads the intro's text box while the battle's dialog shows, a few times a
//  second, until a verdict. `process_frame()` is true once there is one.
class BattleIntroWatcher : public VisualInferenceCallback{
public:
    BattleIntroWatcher(Color color = COLOR_YELLOW);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool process_frame(const ImageViewRGB32& frame, WallClock timestamp) override;

    BattleOpponent verdict() const;
    //  The last text read, for the log.
    std::string last_text() const;

private:
    Color m_color;
    BattleDialogDetector m_dialog;
    ImageFloatBox m_text;
    mutable Mutex m_lock;
    BattleIntroReading m_reading;
    std::string m_last_text;
    WallClock m_last_read = WallClock::min();
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
