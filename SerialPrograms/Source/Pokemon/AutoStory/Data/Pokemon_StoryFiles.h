/*  Story Files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A game's story read from its built-in checkpoint files.
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
    Checkpoint checkpoint = {};
    StoryRowExecution execution = {};
};

//  How a game reads its checkpoint files.
template <typename Checkpoint>
struct StoryFileReader{
    //  The file's checkpoint and shared execution fields; throws
    //  ParseException naming `source` when it is malformed.
    std::function<StoryFileRow<Checkpoint>(std::string_view text, std::string_view source)> read = {};
    //  Whether a row starts with no save to load (a new game). None: every
    //  row starts from a save.
    std::function<bool(const Checkpoint&)> starts_without_save = {};
};

//  The story of the files `builtin`, in their order. Throws ParseException
//  naming the file when it does not parse or reloads a save it cannot
//  count on.
template <typename Checkpoint>
StoryData<Checkpoint> load_story_data(
    std::span<const EmbeddedJsonFile> builtin, const StoryFileReader<Checkpoint>& reader
);

//  The rows a run from `first` to `last` of `rows` plays, in order. Empty
//  if `first` comes after `last` or `last` is past the end.
template <typename Checkpoint>
std::vector<Checkpoint> story_run_rows(std::span<const Checkpoint> rows, size_t first, size_t last);

//  `rows` cut before each row after the first that `story` marks
//  reload_when_chained, where a run reloads.
template <typename Checkpoint>
std::vector<std::vector<Checkpoint>> split_at_reloads(std::span<const Checkpoint> rows, const StoryRows& story);



namespace detail{

//  Whether a row starts from a save, which a reload can load.
enum class RowSave{
    FROM_SAVE,
    WITHOUT_SAVE,   //  a new game
};

//  Adds row `id` to the sets of `rows`. Throws ParseException naming
//  `source` when it reloads but its start (the end of `previous`) is not an
//  autosave.
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

template <typename Checkpoint>
std::vector<std::vector<Checkpoint>> split_at_reloads(std::span<const Checkpoint> rows, const StoryRows& story){
    std::vector<std::vector<Checkpoint>> ranges;
    for (const Checkpoint& row : rows){
        if (ranges.empty() || story.reload_when_chained.contains(row.id)){
            ranges.emplace_back();
        }
        ranges.back().push_back(row);
    }
    return ranges;
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
