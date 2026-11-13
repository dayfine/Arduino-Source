/*  BDSP AutoStory - Battle Intro
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cctype>
#include <chrono>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_StoryLanguage.h"
#include "PokemonBDSP_AutoStory_BattleIntro.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


namespace{

//  Lower case letters and single spaces only: OCR adds punctuation and
//  stray marks, and the match is on words.
std::string words_of(std::string_view text){
    std::string out;
    for (char ch : text){
        const unsigned char c = (unsigned char)ch;
        if (std::isalpha(c)){
            out += (char)std::tolower(c);
        }else if (!out.empty() && out.back() != ' '){
            out += ' ';
        }
    }
    while (!out.empty() && out.back() == ' '){
        out.pop_back();
    }
    return out;
}

//  After "challenged by", at least this many letters of the class and name
//  must be there for the line to be a trainer's, not the start of a typed one.
constexpr size_t TRAINER_TEXT_MIN = 5;

//  The battle dialog's text, inside its margins (its frame: BattleDialogDetector).
const ImageFloatBox INTRO_TEXT(0.10, 0.86, 0.80, 0.08);
//  Reads of the text box are this far apart: OCR is slow and the line types out over seconds.
constexpr std::chrono::milliseconds INTRO_READ_PERIOD = 300ms;

} // namespace


std::string_view battle_opponent_name(BattleOpponent opponent){
    switch (opponent){
    case BattleOpponent::LEADER:
        return "gym leader";
    case BattleOpponent::TRAINER:
        return "trainer";
    case BattleOpponent::UNREAD:
        break;
    }
    return "not read";
}

BattleOpponent classify_battle_intro(std::string_view text){
    const std::string words = words_of(text);
    if (words.empty()){
        return BattleOpponent::UNREAD;
    }
    //  No trainer's class or name is "leader", and Roark's name is on his line.
    if (words.find("leader") != std::string::npos || words.find("roark") != std::string::npos){
        return BattleOpponent::LEADER;
    }
    const size_t by = words.find("challenged by ");
    if (by == std::string::npos){
        return BattleOpponent::UNREAD;
    }
    const std::string rest = words.substr(by + std::string_view("challenged by ").size());
    size_t letters = 0;
    for (char ch : rest){
        letters += ch != ' ';
    }
    return letters >= TRAINER_TEXT_MIN ? BattleOpponent::TRAINER : BattleOpponent::UNREAD;
}

BattleOpponent BattleIntroReading::add(std::string_view text){
    if (m_verdict == BattleOpponent::LEADER){
        return m_verdict;
    }
    const BattleOpponent read = classify_battle_intro(text);
    if (read == BattleOpponent::LEADER){
        m_verdict = BattleOpponent::LEADER;
    }else if (read == BattleOpponent::TRAINER){
        const std::string words = words_of(text);
        if (words == m_last_trainer_text){
            m_verdict = BattleOpponent::TRAINER;
        }
        m_last_trainer_text = words;
    }else{
        m_last_trainer_text.clear();
    }
    return m_verdict;
}


BattleIntroWatcher::BattleIntroWatcher(Color color)
    : VisualInferenceCallback("BattleIntroWatcher")
    , m_color(color)
    , m_dialog(color)
    , m_text(INTRO_TEXT)
{}

void BattleIntroWatcher::make_overlays(VideoOverlaySet& items) const{
    m_dialog.make_overlays(items);
    items.add(m_color, m_text);
}

bool BattleIntroWatcher::process_frame(const ImageViewRGB32& frame, WallClock timestamp){
    if (frame.width() == 0 || frame.height() == 0){
        return false;
    }
    {
        LockGuard<Mutex> lg(m_lock);
        if (m_reading.verdict() != BattleOpponent::UNREAD){
            return true;
        }
        if (m_last_read != WallClock::min() && timestamp - m_last_read < INTRO_READ_PERIOD){
            return false;
        }
        m_last_read = timestamp;
    }
    if (!m_dialog.detect(frame) || !story_reads_text()){
        return false;
    }
    const std::string text = OCR::ocr_read(story_language(), extract_box_reference(frame, m_text), OCR::PageSegMode::SINGLE_BLOCK);
    LockGuard<Mutex> lg(m_lock);
    m_last_text = text;
    return m_reading.add(text) != BattleOpponent::UNREAD;
}

BattleOpponent BattleIntroWatcher::verdict() const{
    LockGuard<Mutex> lg(m_lock);
    return m_reading.verdict();
}

std::string BattleIntroWatcher::last_text() const{
    LockGuard<Mutex> lg(m_lock);
    return m_last_text;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
