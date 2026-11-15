/*  Story Files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A game's story read from its checkpoint files, passed in by name and
 *  text. The game reads each file into its own checkpoint type; this reads
 *  the shared execution fields (Pokemon_StoryRowExecution.h) into the rows
 *  they mark, and checks what holds for every game: a row reloads only a
 *  save it can count on.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryFiles_H
#define PokemonAutomation_Pokemon_StoryFiles_H

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon_AuthoredCheckpointJson.h"
#include "Pokemon_StoryRowExecution.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a game reads from one checkpoint file.
template <typename Checkpoint>
struct StoryFileRow{
    Checkpoint checkpoint;
    StoryRowExecution execution;
};

//  How a game reads its checkpoint files.
template <typename Checkpoint>
struct StoryFileReader{
    //  The file `text`'s checkpoint and shared execution fields
    //  (read_story_row_execution()); `source` names the file in errors.
    //  Throws ParseException when it is malformed.
    std::function<StoryFileRow<Checkpoint>(std::string_view text, std::string_view source)> read;
    //  Whether a row starts with no save to load (a new game): it may not
    //  reload. None set: every row starts from a save.
    std::function<bool(const Checkpoint&)> starts_without_save = {};
};

//  The story of the built-in checkpoint files `builtin`, in their order.
//  Throws ParseException naming the file when a file does not parse, or a
//  row reloads a save it cannot count on in the order the story plays.
template <typename Checkpoint>
StoryData<Checkpoint> load_story_data(
    std::span<const EmbeddedJsonFile> builtin, const StoryFileReader<Checkpoint>& reader
);

//  The rows a run from `first` to `last` of `rows` plays, in order. Empty
//  if `first` comes after `last` or `last` is past the end.
template <typename Checkpoint>
std::vector<Checkpoint> story_run_rows(std::span<const Checkpoint> rows, size_t first, size_t last);



namespace detail{

//  Whether a row starts from a save, which a reload can load.
enum class RowSave{
    FROM_SAVE,
    WITHOUT_SAVE,   //  a new game
};

//  Adds row `id` to the sets of `rows`. Throws ParseException naming
//  `source` when it reloads and starts WITHOUT_SAVE or the row it follows
//  (`previous`, none for the first row) does not end on the game's
//  autosave: a reload loads the last save, which must be that autosave.
void add_story_row(
    std::string_view id, const StoryRowExecution& execution, RowSave start,
    std::optional<std::string_view> previous, std::string_view source, StoryRows& rows
);

} // namespace detail



template <typename Checkpoint>
StoryData<Checkpoint> load_story_data(
    std::span<const EmbeddedJsonFile> builtin, const StoryFileReader<Checkpoint>& reader
){
    StoryData<Checkpoint> data;
    for (const auto& [name, text] : builtin){
        StoryFileRow<Checkpoint> row = reader.read(text, name);
        const bool new_game = reader.starts_without_save && reader.starts_without_save(row.checkpoint);
        const std::optional<std::string_view> previous = data.checkpoints.empty()
            ? std::nullopt
            : std::optional<std::string_view>(data.checkpoints.back().id);
        detail::add_story_row(
            row.checkpoint.id, row.execution,
            new_game ? detail::RowSave::WITHOUT_SAVE : detail::RowSave::FROM_SAVE,
            previous, name, data
        );
        data.checkpoints.push_back(std::move(row.checkpoint));
    }
    return data;
}

template <typename Checkpoint>
std::vector<Checkpoint> story_run_rows(std::span<const Checkpoint> rows, size_t first, size_t last){
    if (first > last || last >= rows.size()){
        return {};
    }
    return std::vector<Checkpoint>(rows.begin() + first, rows.begin() + last + 1);
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
