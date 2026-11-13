/*  BDSP AutoStory - Battle Intro
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cctype>
#include <chrono>
#include <format>
#include <vector>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "Common/Cpp/Strings/Unicode.h"
#include "PokemonBDSP_AutoStory_BattleIntro.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


namespace{

//  A letter with its accent dropped: OCR reads "Arene" as often as "Arène",
//  so both sides of a match are folded. Latin-1 letters and the French
//  ligature; anything else that is not ASCII is not a letter here.
std::string_view fold_letter(char32_t c){
    static constexpr std::string_view LATIN1 =
        "aaaaaaaceeeeiiii"  //  U+00E0 - U+00EF
        "dnooooo/ouuuuyty"; //  U+00F0 - U+00FF
    if (c < 0x80){
        return {};
    }
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7){
        c += 0x20;
    }
    if (c >= 0xE0 && c <= 0xFF && c != 0xF7){
        return LATIN1.substr(c - 0xE0, 1);
    }
    if (c == 0x152 || c == 0x153){
        return "oe";
    }
    return {};
}

//  Lower case ASCII letters and single spaces only: OCR adds punctuation and
//  stray marks, and the match is on words.
std::string words_of(std::string_view text){
    std::string out;
    const auto space = [&out](){
        if (!out.empty() && out.back() != ' '){
            out += ' ';
        }
    };
    for (char32_t c : utf8_to_utf32(std::string(text))){
        if (c < 0x80 && std::isalpha((int)c)){
            out += (char)std::tolower((int)c);
            continue;
        }
        const std::string_view folded = fold_letter(c);
        if (folded.empty()){
            space();
        }else{
            out += folded;
        }
    }
    while (!out.empty() && out.back() == ' '){
        out.pop_back();
    }
    return out;
}

//  What the intro says in one language, in folded words (words_of()).
struct IntroWords{
    Language language;
    //  Any of these in the line makes it the leader's: his name or his title.
    //  No gym trainer's class or name has them.
    std::vector<std::string_view> leader;
    //  The words just before a trainer's class and name. Empty when the
    //  trainer's line is not known word for word: then any line that is not
    //  the leader's is a trainer's, once read the same twice (it is the
    //  battle's first text).
    std::string_view trainer;
};

//  English: the game's text ("You are challenged by Gym Leader Roark!", read
//  on lab-B). French: Roark is "Pierrick", the title "Champion d'Arene"
//  (pokepedia.fr, "Pierrick" and "Champion d'Arene"); the trainer's line is
//  not verified word for word, so it has no words of its own.
const std::vector<IntroWords>& intro_word_tables(){
    static const std::vector<IntroWords> TABLES{
        {Language::English, {"leader", "roark"}, "challenged by"},
        {Language::French, {"pierrick", "champion", "arene"}, ""},
    };
    return TABLES;
}

const IntroWords* intro_words(Language language){
    for (const IntroWords& table : intro_word_tables()){
        if (table.language == language){
            return &table;
        }
    }
    return nullptr;
}

size_t letters_in(std::string_view words){
    size_t letters = 0;
    for (char ch : words){
        letters += ch != ' ';
    }
    return letters;
}

//  After the trainer's words, at least this many letters of the class and name
//  must be there for the line to be a trainer's, not the start of a typed one.
constexpr size_t TRAINER_TEXT_MIN = 5;
//  A line with no trainer's words to look for: at least this many letters.
constexpr size_t TRAINER_LINE_MIN = 12;

//  The battle dialog's text, inside its margins (its frame: BattleDialogDetector).
const ImageFloatBox INTRO_TEXT(0.10, 0.86, 0.80, 0.08);
//  Reads of the text box are this far apart: OCR is slow and the line types out over seconds.
constexpr std::chrono::milliseconds INTRO_READ_PERIOD = 300ms;

} // namespace


std::string_view battle_opponent_name(BattleOpponent opponent){
    switch (opponent){
    case BattleOpponent::LEADER:
        return "the gym leader";
    case BattleOpponent::TRAINER:
        return "a trainer";
    case BattleOpponent::UNREAD:
        break;
    }
    return "not read";
}

bool battle_intro_reads(Language language){
    return intro_words(language) != nullptr;
}

BattleOpponent classify_battle_intro(std::string_view text, Language language){
    const IntroWords* table = intro_words(language);
    const std::string words = words_of(text);
    if (table == nullptr || words.empty()){
        return BattleOpponent::UNREAD;
    }
    for (std::string_view word : table->leader){
        if (words.find(word) != std::string::npos){
            return BattleOpponent::LEADER;
        }
    }
    if (table->trainer.empty()){
        return letters_in(words) >= TRAINER_LINE_MIN ? BattleOpponent::TRAINER : BattleOpponent::UNREAD;
    }
    const std::string marker = std::format("{} ", table->trainer);
    const size_t at = words.find(marker);
    if (at == std::string::npos){
        return BattleOpponent::UNREAD;
    }
    return letters_in(std::string_view(words).substr(at + marker.size())) >= TRAINER_TEXT_MIN
        ? BattleOpponent::TRAINER
        : BattleOpponent::UNREAD;
}

BattleOpponent BattleIntroReading::add(std::string_view text){
    if (m_verdict == BattleOpponent::LEADER){
        return m_verdict;
    }
    const BattleOpponent read = classify_battle_intro(text, m_language);
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


BattleIntroWatcher::BattleIntroWatcher(Language language, Color color)
    : VisualInferenceCallback("BattleIntroWatcher")
    , m_language(language)
    , m_color(color)
    , m_dialog(color)
    , m_text(INTRO_TEXT)
    , m_reading(language)
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
    if (!m_dialog.detect(frame) || !battle_intro_reads(m_language)){
        return false;
    }
    const std::string text = OCR::ocr_read(m_language, extract_box_reference(frame, m_text), OCR::PageSegMode::SINGLE_BLOCK);
    LockGuard<Mutex> lg(m_lock);
    m_last_text = text;
    if (m_frames.size() >= INTRO_FRAMES_KEPT){
        m_frames.erase(m_frames.begin());
    }
    m_frames.emplace_back(frame.copy());
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

std::vector<ImageRGB32> BattleIntroWatcher::frames_read() const{
    LockGuard<Mutex> lg(m_lock);
    std::vector<ImageRGB32> out;
    for (const ImageRGB32& frame : m_frames){
        out.emplace_back(frame.copy());
    }
    return out;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
