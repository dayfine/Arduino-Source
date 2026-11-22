/*  Mail Glitch Plan Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <string_view>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_MailGlitchTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr std::string_view MAIL_GLITCH_ORDER[] = {
    "mg-01",
    "mg-02",
    "mg-03",
    "mg-04",
    "mg-05",
};

size_t mail_glitch_position(std::string_view id){
    return static_cast<size_t>(
        std::find(std::begin(MAIL_GLITCH_ORDER), std::end(MAIL_GLITCH_ORDER), id) - std::begin(MAIL_GLITCH_ORDER)
    );
}

std::vector<StoryCheckpoint> load_mail_glitch_checkpoints(){
    std::vector<StoryCheckpoint> rows;
    for (const auto& [name, text] : mail_glitch_checkpoint_json()){
        if (!name.ends_with(".plan")){
            continue;
        }
        StoryCheckpoint row = read_story_checkpoint_file(parse_json(text), name);
        if (mail_glitch_position(row.id) >= std::size(MAIL_GLITCH_ORDER)){
            throw ParseException(std::format("{}: unknown Mail Glitch checkpoint id {}", name, row.id));
        }
        rows.push_back(std::move(row));
    }
    std::sort(rows.begin(), rows.end(), [](const StoryCheckpoint& a, const StoryCheckpoint& b){
        return mail_glitch_position(a.id) < mail_glitch_position(b.id);
    });
    return rows;
}

} // namespace


const std::vector<StoryCheckpoint>& mail_glitch_checkpoints(){
    static const std::vector<StoryCheckpoint> ROWS = load_mail_glitch_checkpoints();
    return ROWS;
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
