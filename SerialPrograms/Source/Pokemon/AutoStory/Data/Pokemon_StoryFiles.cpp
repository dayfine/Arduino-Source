/*  Story Files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon_StoryFiles.h"

namespace PokemonAutomation{
namespace Pokemon{
namespace detail{


void check_dev_file_names(std::span<const EmbeddedJsonFile> builtin, std::span<const StoryDevFile> dev_files){
    for (const StoryDevFile& dev : dev_files){
        bool known = false;
        for (const auto& [name, text] : builtin){
            known |= dev.name == std::format("{}.json", name);
        }
        if (!known){
            throw ParseException(std::format("{}: no built-in checkpoint file is named {}", dev.path, dev.name));
        }
    }
}

const StoryDevFile* find_dev_file(std::span<const StoryDevFile> dev_files, std::string_view builtin_name){
    const std::string file_name = std::format("{}.json", builtin_name);
    for (const StoryDevFile& file : dev_files){
        if (file.name == file_name){
            return &file;
        }
    }
    return nullptr;
}

ParseException naming_path(const ParseException& error, std::string_view path){
    const std::string message = error.message();
    if (message.find(path) != std::string::npos){
        return error;
    }
    return ParseException(std::format("{}: {}", path, message));
}

void check_dev_checkpoint(
    std::string_view path, std::string_view id, std::string_view builtin_id, const std::vector<std::string>& problems
){
    if (id != builtin_id){
        throw ParseException(std::format("{}: checkpoint id {} does not match the built-in {}", path, id, builtin_id));
    }
    if (!problems.empty()){
        throw ParseException(std::format("{}: {}", path, problems.front()));
    }
}

void check_in_story_order(
    const std::function<bool(std::string_view id)>& in_story_order, std::string_view id, std::string_view source
){
    if (in_story_order && !in_story_order(id)){
        throw ParseException(std::format("{}: checkpoint {} is not in the game's story order", source, id));
    }
}

namespace{

//  Which row sequence the reload check applies to.
enum class RunKind{
    DEVELOPER,
    USER,
};

//  Text naming `run` in a reload error.
std::string_view run_suffix(RunKind run){
    switch (run){
    case RunKind::DEVELOPER:
        return "";
    case RunKind::USER:
        return " in a user's run";
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown story run kind.");
}

//  Throws ParseException naming `source` unless `previous` ends on the
//  game's autosave in `rows`, for `run`.
void check_reload_after(
    std::optional<std::string_view> previous, const StoryRows& rows, RunKind run, std::string_view source
){
    if (!previous || !rows.autosaves.contains(*previous)){
        throw ParseException(std::format(
            "{}: reload_when_chained after a checkpoint that does not end on the game's autosave{}: the reload would load an older save",
            source, run_suffix(run)
        ));
    }
}

} // namespace

void add_story_row(
    std::string_view id, const StoryRowExecution& execution, RowSave start,
    const PreviousRows& previous, std::string_view source, StoryRows& rows
){
    if (execution.reload != StoryReload::NEVER){
        if (start == RowSave::WITHOUT_SAVE){
            throw ParseException(std::format("{}: reload_when_chained on a checkpoint with no save to reload", source));
        }
        check_reload_after(previous.in_dev_run, rows, RunKind::DEVELOPER, source);
        if (!execution.dev_only && previous.in_user_run != previous.in_dev_run){
            check_reload_after(previous.in_user_run, rows, RunKind::USER, source);
        }
        rows.reload_when_chained.emplace(id);
    }
    if (execution.reload == StoryReload::ALWAYS){
        rows.reload_always.emplace(id);
    }
    if (execution.autosave){
        rows.autosaves.emplace(id);
    }
    if (execution.dev_only){
        rows.dev_only.emplace(id);
    }
}



} // namespace detail
} // namespace Pokemon
} // namespace PokemonAutomation
