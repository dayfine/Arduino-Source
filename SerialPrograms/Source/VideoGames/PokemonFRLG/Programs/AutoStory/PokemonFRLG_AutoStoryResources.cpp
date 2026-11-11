/*  AutoStory Resources
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <iterator>
#include "PokemonFRLG_AutoStoryResources.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

const std::string_view PATHS[] = {
    //  Overworld/PokemonFRLG_PlayerFacingReader.cpp
    "PokemonFRLG/PlayerSprites/red_normal.png",
    "PokemonFRLG/PlayerSprites/green_normal.png",

    //  Inference/PokemonFRLG_SelectionArrowDetector.cpp
    "PokemonFRLG/SelectionArrow.png",
    //  Inference/PokemonFRLG_BattleSelectionArrowDetector.cpp
    "PokemonFRLG/BattleSelectionArrow.png",
    //  Inference/PokemonFRLG_PokedexRegisteredDetector.cpp
    "PokemonFRLG/PokedexButtonA.png",
    //  Inference/Menus/PokemonFRLG_PartyHeldItemDetector.cpp
    "PokemonFRLG/PartyHeldItem.png",
    //  Inference/PokemonFRLG_StatsReader.cpp
    "PokemonFRLG/NatureCheckerOCR.json",

    //  Inference/Box/PokemonFRLG_BoxSelectionModeDetector.cpp
    "PokemonFRLG/Box/SelectionHand_White.png",
    "PokemonFRLG/Box/SelectionHand_White2.png",
    "PokemonFRLG/Box/SelectionHand_White_Inverted.png",
    "PokemonFRLG/Box/SelectionHand_Gold.png",
    "PokemonFRLG/Box/SelectionHand_Gold2.png",
    "PokemonFRLG/Box/SelectionHand_Gold_Inverted.png",
    "PokemonFRLG/Box/SelectionHand_Closed_White.png",
    "PokemonFRLG/Box/SelectionHand_Closed_Gold.png",

    //  Inference/PokemonFRLG_DigitReader.cpp, digits 0 to 9 of each box.
    "PokemonFRLG/Digits/0.png", "PokemonFRLG/Digits/1.png", "PokemonFRLG/Digits/2.png",
    "PokemonFRLG/Digits/3.png", "PokemonFRLG/Digits/4.png", "PokemonFRLG/Digits/5.png",
    "PokemonFRLG/Digits/6.png", "PokemonFRLG/Digits/7.png", "PokemonFRLG/Digits/8.png",
    "PokemonFRLG/Digits/9.png",
    "PokemonFRLG/LevelDigits/0.png", "PokemonFRLG/LevelDigits/1.png", "PokemonFRLG/LevelDigits/2.png",
    "PokemonFRLG/LevelDigits/3.png", "PokemonFRLG/LevelDigits/4.png", "PokemonFRLG/LevelDigits/5.png",
    "PokemonFRLG/LevelDigits/6.png", "PokemonFRLG/LevelDigits/7.png", "PokemonFRLG/LevelDigits/8.png",
    "PokemonFRLG/LevelDigits/9.png",
    "PokemonFRLG/DialogDigits/0.png", "PokemonFRLG/DialogDigits/1.png", "PokemonFRLG/DialogDigits/2.png",
    "PokemonFRLG/DialogDigits/3.png", "PokemonFRLG/DialogDigits/4.png", "PokemonFRLG/DialogDigits/5.png",
    "PokemonFRLG/DialogDigits/6.png", "PokemonFRLG/DialogDigits/7.png", "PokemonFRLG/DialogDigits/8.png",
    "PokemonFRLG/DialogDigits/9.png",

    //  Inference/Sounds/PokemonFRLG_ShinySoundDetector.cpp and
    //  PokemonFRLG_CatchFanfareDetector.cpp, one per sample rate.
    "PokemonFRLG/ShinySound-44100.wav",
    "PokemonFRLG/ShinySound-48000.wav",
    "PokemonFRLG/CatchFanfare-44100.wav",
    "PokemonFRLG/CatchFanfare-48000.wav",

    //  Resources/PokemonFRLG_PokemonSprites.cpp
    "PokemonFRLG/PokemonSprites.png",
    "PokemonFRLG/PokemonSpritesFlipped.png",
    "PokemonFRLG/PokemonSprites.json",

    //  Programs/RngManipulation/PokemonFRLG_SeedsDatabase.cpp, for the
    //  story's RNG gifts.
    "PokemonFRLG/RngSeeds/FR_Eng.json",
    "PokemonFRLG/RngSeeds/FR_Jpn.json",
    "PokemonFRLG/RngSeeds/LG_Eng.json",
    "PokemonFRLG/RngSeeds/LG_Jpn.json",
};

} // namespace


const std::vector<std::string_view>& autostory_resource_paths(){
    static const std::vector<std::string_view> LIST(std::begin(PATHS), std::end(PATHS));
    return LIST;
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
