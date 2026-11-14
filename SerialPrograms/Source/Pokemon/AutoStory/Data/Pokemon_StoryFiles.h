/*  Story Files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A game's story read from its checkpoint files: the checked-in ones,
 *  built into the program (cmake/EmbedTextFiles.cmake), each replaced by
 *  the dev file of the same name if there is one (Pokemon_StoryDevFiles.h),
 *  so a walk can be retimed on the console without a rebuild. The game
 *  reads each file into its own checkpoint type; this reads the shared
 *  execution fields (Pokemon_StoryRowExecution.h) into the rows they mark,
 *  and checks what holds for every game: a dev file replaces a built-in
 *  file, keeps its checkpoint's id and passes the game's lint, and a row
 *  reloads only a save it can count on.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryFiles_H
#define PokemonAutomation_Pokemon_StoryFiles_H

#include <algorithm>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon_AuthoredCheckpointJson.h"
#include "Pokemon_StoryDevFiles.h"
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
    //  The problems the game finds in a dev file's checkpoint; the first
    //  one refuses the file. None set: no lint.
    std::function<std::vector<std::string>(const Checkpoint&)> lint = {};
    //  Whether a row starts with no save to load (a new game): it may not
    //  reload. None set: every row starts from a save.
    std::function<bool(const Checkpoint&)> starts_without_save = {};
    //  Whether the game plays checkpoint `a` before `b`, when that is not
    //  the order of its files' names. None set: file-name order.
    std::function<bool(const Checkpoint& a, const Checkpoint& b)> plays_before = {};
    //  Whether the game's story order lists checkpoint `id`: a file whose
    //  id it does not list is refused, not skipped or played out of place.
    //  None set: the game keeps no list, and every id plays.
    std::function<bool(std::string_view id)> in_story_order = {};
};


//  The story of the built-in checkpoint files `builtin`, in their order or
//  the game's (StoryFileReader::plays_before), each of `dev_files` in place
//  of the built-in file of its name. Throws ParseException naming the file
//  when a file does not parse, a dev file matches no built-in file, changes
//  its checkpoint's id or fails the game's lint, a file's checkpoint id is
//  not in the game's story order (StoryFileReader::in_story_order), or a
//  row reloads a save it cannot count on in the order the story plays.
template <typename Checkpoint>
StoryData<Checkpoint> load_story_data(
    std::span<const EmbeddedJsonFile> builtin, std::span<const StoryDevFile> dev_files,
    const StoryFileReader<Checkpoint>& reader
);

//  Who a run's rows are listed for.
enum class StoryRowsShown{
    USER,       //  every row but the dev-only ones
    DEVELOPER,  //  every row (the program's Developer Mode)
};

//  The rows of `story` a run may play, in order. The start and end
//  dropdowns list these, so their indices name these rows.
template <typename Checkpoint>
std::vector<Checkpoint> story_rows_for_mode(const StoryData<Checkpoint>& story, StoryRowsShown shown);

//  The rows a run from `first` to `last` of `rows` plays, in order. Empty
//  if `first` comes after `last` or `last` is past the end.
template <typename Checkpoint>
std::vector<Checkpoint> story_run_rows(std::span<const Checkpoint> rows, size_t first, size_t last);



namespace detail{

//  Throws ParseException naming the dev file that matches no built-in file.
void check_dev_file_names(std::span<const EmbeddedJsonFile> builtin, std::span<const StoryDevFile> dev_files);
//  The dev file that replaces the built-in file `builtin_name` (its name
//  without ".json"); null if none.
const StoryDevFile* find_dev_file(std::span<const StoryDevFile> dev_files, std::string_view builtin_name);
//  `error`, its message prefixed with `path` unless it already names it.
ParseException naming_path(const ParseException& error, std::string_view path);
//  Throws ParseException naming `path` when the dev file's checkpoint `id`
//  is not the built-in file's `builtin_id`, or `problems` is not empty.
void check_dev_checkpoint(
    std::string_view path, std::string_view id, std::string_view builtin_id, const std::vector<std::string>& problems
);

//  Throws ParseException naming `source` and `id` when `in_story_order`
//  is set and does not list `id`.
void check_in_story_order(
    const std::function<bool(std::string_view id)>& in_story_order, std::string_view id, std::string_view source
);

//  Whether a row starts from a save, which a reload can load.
enum class RowSave{
    FROM_SAVE,
    WITHOUT_SAVE,   //  a new game
};

//  Adds row `id` to the sets of `rows`. Throws ParseException naming
//  `source` when it reloads and starts WITHOUT_SAVE or the row before it,
//  `previous_id`, does not end on the game's autosave: a reload loads the
//  last save, which must be that autosave.
void add_story_row(
    std::string_view id, const StoryRowExecution& execution, RowSave start,
    std::optional<std::string_view> previous_id, std::string_view source, StoryRows& rows
);

//  The dev file `dev`'s row, in place of the built-in file `builtin_name`:
//  it must parse, keep the built-in checkpoint's id and pass the lint.
template <typename Checkpoint>
StoryFileRow<Checkpoint> read_dev_file(
    const StoryDevFile& dev, std::string_view builtin_text, std::string_view builtin_name,
    const StoryFileReader<Checkpoint>& reader
){
    StoryFileRow<Checkpoint> row;
    try{
        row = reader.read(dev.text, dev.path);
    }catch (const ParseException& error){
        //  Malformed JSON says nothing of where it came from.
        throw naming_path(error, dev.path);
    }
    const std::string builtin_id = reader.read(builtin_text, builtin_name).checkpoint.id;
    const std::vector<std::string> problems = reader.lint ? reader.lint(row.checkpoint) : std::vector<std::string>{};
    check_dev_checkpoint(dev.path, row.checkpoint.id, builtin_id, problems);
    return row;
}

} // namespace detail


template <typename Checkpoint>
StoryData<Checkpoint> load_story_data(
    std::span<const EmbeddedJsonFile> builtin, std::span<const StoryDevFile> dev_files,
    const StoryFileReader<Checkpoint>& reader
){
    detail::check_dev_file_names(builtin, dev_files);
    StoryData<Checkpoint> data;
    struct SourcedRow{
        StoryFileRow<Checkpoint> row;
        std::string_view source;
    };
    std::vector<SourcedRow> rows;
    for (const auto& [name, text] : builtin){
        const StoryDevFile* dev = detail::find_dev_file(dev_files, name);
        if (dev == nullptr){
            rows.push_back({reader.read(text, name), name});
            continue;
        }
        rows.push_back({detail::read_dev_file(*dev, text, name, reader), dev->path});
        data.dev_files.emplace_back(dev->name, dev->path);
    }
    for (const auto& [row, source] : rows){
        detail::check_in_story_order(reader.in_story_order, row.checkpoint.id, source);
    }
    if (reader.plays_before){
        std::stable_sort(rows.begin(), rows.end(), [&](const SourcedRow& a, const SourcedRow& b){
            return reader.plays_before(a.row.checkpoint, b.row.checkpoint);
        });
    }
    //  A reload loads the save the row before it in play ends on.
    for (auto& [row, source] : rows){
        const bool new_game = reader.starts_without_save && reader.starts_without_save(row.checkpoint);
        detail::add_story_row(
            row.checkpoint.id, row.execution,
            new_game ? detail::RowSave::WITHOUT_SAVE : detail::RowSave::FROM_SAVE,
            data.checkpoints.empty() ? std::nullopt : std::optional<std::string_view>(data.checkpoints.back().id),
            source, data
        );
        data.checkpoints.push_back(std::move(row.checkpoint));
    }
    return data;
}

template <typename Checkpoint>
std::vector<Checkpoint> story_rows_for_mode(const StoryData<Checkpoint>& story, StoryRowsShown shown){
    std::vector<Checkpoint> rows;
    for (const Checkpoint& row : story.checkpoints){
        if (shown == StoryRowsShown::DEVELOPER || !story.dev_only.contains(row.id)){
            rows.push_back(row);
        }
    }
    return rows;
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
