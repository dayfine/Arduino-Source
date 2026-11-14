/*  AutoStory Start Frames Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The start frame comparison on synthetic screens: a map of textured
 *  16x16 tiles seen through the GBA's 15x10-tile camera, with people drawn
 *  on it, scaled up to the size the game takes in a 1080p capture and
 *  given a capture's noise. The thresholds were chosen on console B's
 *  captures (see PokemonFRLG_AutoStoryStartFrames.cpp); game captures
 *  aren't kept in the repo.
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryStartFrames.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr size_t TILE = 16;
constexpr size_t VIEW_TILES_X = 15;
constexpr size_t VIEW_TILES_Y = 10;
//  GAME_BOX in a 1920x1080 capture.
constexpr size_t CAPTURE_WIDTH = 1560;
constexpr size_t CAPTURE_HEIGHT = 1040;

uint32_t next_random(uint32_t& state){
    state = state * 1664525u + 1013904223u;
    return state >> 8;
}

uint32_t rgb(uint32_t r, uint32_t g, uint32_t b){
    return 0xff000000 | (std::min<uint32_t>(r, 255) << 16) | (std::min<uint32_t>(g, 255) << 8) | std::min<uint32_t>(b, 255);
}

//  A map's tiles: indoor floors and walls, or grass, paths and trees.
struct Tileset{
    std::vector<uint32_t> colors;
};
const Tileset INDOOR{{rgb(200, 184, 152), rgb(176, 160, 128), rgb(232, 224, 208), rgb(120, 112, 104), rgb(208, 96, 72), rgb(88, 136, 200)}};
const Tileset OUTDOOR{{rgb(96, 192, 120), rgb(72, 160, 96), rgb(216, 200, 144), rgb(48, 112, 64), rgb(120, 168, 232), rgb(152, 136, 120)}};

//  The tile `kind` of `tileset`: its colour, shaded in a pattern of its own.
uint32_t tile_pixel(const Tileset& tileset, size_t kind, size_t x, size_t y){
    const uint32_t base = tileset.colors[kind % tileset.colors.size()];
    const int shade = (int)((x * (3 + kind) + y * (5 + 2 * kind)) % 7) * 10 - 30;
    const auto channel = [&](int shift){
        return (uint32_t)std::clamp((int)((base >> shift) & 0xff) + shade, 0, 255);
    };
    return rgb(channel(16), channel(8), channel(0));
}

//  A map of 40x30 tiles, the same for the same seed.
struct Map{
    const Tileset* tileset;
    std::vector<size_t> tiles;
};
constexpr size_t MAP_WIDTH = 40;
constexpr size_t MAP_HEIGHT = 30;
Map make_map(const Tileset& tileset, uint32_t seed){
    Map map{&tileset, {}};
    for (size_t c = 0; c < MAP_WIDTH * MAP_HEIGHT; c++){
        map.tiles.push_back(next_random(seed) % tileset.colors.size());
    }
    return map;
}

//  A person standing on view tile (`tx`, `ty`): 16x32, their head on the
//  tile above.
struct Person{
    size_t tx;
    size_t ty;
};

//  What the camera at map tile (`cx`, `cy`) shows, with `people` on it, as
//  the capture shows the game: scaled up, with noise.
ImageRGB32 screen(const Map& map, size_t cx, size_t cy, const std::vector<Person>& people, uint32_t noise_seed){
    ImageRGB32 view(VIEW_TILES_X * TILE, VIEW_TILES_Y * TILE);
    for (size_t y = 0; y < view.height(); y++){
        for (size_t x = 0; x < view.width(); x++){
            const size_t kind = map.tiles[(cy + y / TILE) * MAP_WIDTH + cx + x / TILE];
            view.pixel(x, y) = tile_pixel(*map.tileset, kind, x % TILE, y % TILE);
        }
    }
    for (const Person& person : people){
        for (size_t y = person.ty * TILE - TILE; y < person.ty * TILE + TILE; y++){
            for (size_t x = person.tx * TILE; x < person.tx * TILE + TILE; x++){
                const size_t row = y - (person.ty * TILE - TILE);
                view.pixel(x, y) = row < 10 ? rgb(232, 64, 48) : row < 20 ? rgb(248, 208, 168) : rgb(48, 80, 176);
            }
        }
    }
    ImageRGB32 capture = view.scale_to(CAPTURE_WIDTH, CAPTURE_HEIGHT);
    for (size_t y = 0; y < capture.height(); y++){
        for (size_t x = 0; x < capture.width(); x++){
            const uint32_t pixel = capture.pixel(x, y);
            const int jitter = (int)(next_random(noise_seed) % 9) - 4;
            const auto channel = [&](int shift){
                return (uint32_t)std::clamp((int)((pixel >> shift) & 0xff) + jitter, 0, 255);
            };
            capture.pixel(x, y) = rgb(channel(16), channel(8), channel(0));
        }
    }
    return capture;
}

const Map CENTER = make_map(INDOOR, 27);
const Map CITY = make_map(OUTDOOR, 10);
const Map ROUTE = make_map(OUTDOOR, 4);
//  The player, in the middle of the view.
const Person PLAYER{7, 5};


struct CompareInput{
    std::string name;
    ImageRGB32 (*frame)();
};
struct CompareExpectation{
    bool same_place;
};
struct CompareCase{
    CompareInput input;
    CompareExpectation expectation;
};

//  The reference: Cerulean Center's counter, the nurse behind it, a
//  visitor by the door.
ImageRGB32 counter_reference(){
    return screen(CENTER, 10, 8, {PLAYER, {7, 3}, {11, 8}}, 1);
}

class Test_StartFrameCompare : public UnitTest{
public:
    explicit Test_StartFrameCompare(CompareCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryStartFrames - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const StartFrameDifference difference = compare_start_frames(counter_reference(), m_case.input.frame());
        if (same_start_place(difference) != m_case.expectation.same_place){
            return std::format(
                "expected {}, got {}",
                m_case.expectation.same_place ? "the same place" : "another place", to_string(difference)
            );
        }
        return true;
    }

private:
    CompareCase m_case;
};

const CompareCase COMPARE_CASES[] = {
    {
        .input = {"the same screen, another capture", []{ return screen(CENTER, 10, 8, {PLAYER, {7, 3}, {11, 8}}, 2); }},
        .expectation = {true},
    },
    {
        .input = {"the visitor walked off", []{ return screen(CENTER, 10, 8, {PLAYER, {7, 3}}, 3); }},
        .expectation = {true},
    },
    {
        .input = {"three people moved", []{ return screen(CENTER, 10, 8, {PLAYER, {8, 3}, {2, 7}, {12, 2}, {4, 8}}, 4); }},
        .expectation = {true},
    },
    {
        .input = {"one step off the counter", []{ return screen(CENTER, 10, 9, {PLAYER, {7, 2}, {11, 7}}, 5); }},
        .expectation = {false},
    },
    {
        .input = {"outside the Center's door", []{ return screen(CITY, 12, 6, {PLAYER}, 6); }},
        .expectation = {false},
    },
    {
        .input = {"another map", []{ return screen(ROUTE, 3, 14, {PLAYER, {3, 6}}, 7); }},
        .expectation = {false},
    },
    {
        .input = {"a black fade", []{
            ImageRGB32 black(CAPTURE_WIDTH, CAPTURE_HEIGHT);
            black.fill(0xff000000);
            return black;
        }},
        .expectation = {false},
    },
};


//  read_start_frames() on a run of frames.
struct ReadInput{
    std::string name;
    bool has_reference;
    //  The screens, one per frame read; an empty one is no video.
    std::vector<ImageRGB32 (*)()> frames;
};
struct ReadExpectation{
    StartFound found;
    size_t frames_read;
};
struct ReadCase{
    ReadInput input;
    ReadExpectation expectation;
};

ImageRGB32 no_video(){
    return ImageRGB32();
}
ImageRGB32 black_fade(){
    ImageRGB32 black(CAPTURE_WIDTH, CAPTURE_HEIGHT);
    black.fill(0xff000000);
    return black;
}
ImageRGB32 at_counter(){
    return screen(CENTER, 10, 8, {PLAYER, {7, 3}}, 8);
}
ImageRGB32 outside_door(){
    return screen(CITY, 12, 6, {PLAYER}, 9);
}

std::string to_string(StartFound found){
    switch (found){
    case StartFound::NOT_READ:
        return "NOT_READ";
    case StartFound::AT_START:
        return "AT_START";
    case StartFound::ELSEWHERE:
        return "ELSEWHERE";
    }
    return "?";
}

class Test_StartFrameRead : public UnitTest{
public:
    explicit Test_StartFrameRead(ReadCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryStartFrames - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::optional<ImageRGB32> reference;
        if (m_case.input.has_reference){
            reference = start_frame(counter_reference());
        }
        size_t read = 0;
        const StartRead got = read_start_frames(reference, [&]{
            return m_case.input.frames[std::min(read++, m_case.input.frames.size() - 1)]();
        }, 3);
        if (got.found != m_case.expectation.found || read != m_case.expectation.frames_read){
            return std::format(
                "expected {} after {} frames, got {} after {}: {}",
                to_string(m_case.expectation.found), m_case.expectation.frames_read, to_string(got.found), read, got.detail
            );
        }
        return true;
    }

private:
    ReadCase m_case;
};

const ReadCase READ_CASES[] = {
    {
        .input = {"no frame kept: not read", false, {at_counter}},
        .expectation = {StartFound::NOT_READ, 0},
    },
    {
        .input = {"no video: not read", true, {no_video}},
        .expectation = {StartFound::NOT_READ, 1},
    },
    {
        .input = {"the counter: at the start", true, {at_counter}},
        .expectation = {StartFound::AT_START, 1},
    },
    {
        .input = {"a fade, then the counter: at the start", true, {black_fade, at_counter}},
        .expectation = {StartFound::AT_START, 2},
    },
    {
        .input = {"outside the door three times: elsewhere", true, {outside_door}},
        .expectation = {StartFound::ELSEWHERE, 3},
    },
};


//  console B, FRLG 07.02, 2026-11-09: the save stood outside Cerulean
//  Center's door, not at the counter where 07.02 starts, and the run walked
//  off. A checkpoint of one walk with the console's start check on frames
//  (add_start_check(), as build_checkpoint() adds it): the walk runs only
//  when the screen after the save loads shows the kept frame's place, or
//  when no frame is kept.
struct StartCheckInput{
    std::string name;
    bool has_reference;
    ImageRGB32 (*screen)();
};
struct StartCheckExpectation{
    bool walked;
    //  The start check's log line starts with this.
    std::string logged;
};
struct StartCheckCase{
    StartCheckInput input;
    StartCheckExpectation expectation;
};

class Test_StartFrameCheck : public UnitTest{
public:
    explicit Test_StartFrameCheck(StartCheckCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryStartFrames - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        size_t walks = 0;
        std::vector<std::string> lines;
        Pokemon::Checkpoint checkpoint = one_walk(walks);
        add_start_check(
            checkpoint, to_pose(StoryPose{"CeruleanCity_PokemonCenter_1F", TilePos{7, 4}, Direction::NORTH}),
            [this]{ return read_start_frames(reference(), m_case.input.screen, 3); },
            [&lines](const std::string& line){ lines.push_back(line); }
        );
        Pokemon::RunState state;
        Pokemon::EventLogger log = [](const std::string&){};
        const Pokemon::CheckpointReport report = Pokemon::run_checkpoint(state, checkpoint, log);
        const bool stopped = !report.completed && report.failure.failure == "start_failed";
        const bool logged = lines.size() == 1 && lines[0].starts_with(m_case.expectation.logged);
        if ((m_case.expectation.walked ? !report.completed || walks != 1 : !stopped || walks != 0) || !logged){
            return std::format(
                "expected {}, \"{}\" logged; got {} walk(s), {}: {}",
                m_case.expectation.walked ? "the walk" : "a stop before it", m_case.expectation.logged,
                walks, Pokemon::to_string(report.failure), lines.empty() ? "nothing logged" : lines[0]
            );
        }
        return true;
    }

private:
    static Pokemon::Checkpoint one_walk(size_t& walks){
        Pokemon::Checkpoint checkpoint;
        checkpoint.id = "07.02";
        Pokemon::Event walk;
        walk.name = "to_the_door";
        walk.act = [&walks](Pokemon::RunState&){
            walks++;
            return Pokemon::outcome_ok();
        };
        checkpoint.events.push_back(std::move(walk));
        return checkpoint;
    }
    std::optional<ImageRGB32> reference() const{
        if (!m_case.input.has_reference){
            return std::nullopt;
        }
        return start_frame(counter_reference());
    }

    StartCheckCase m_case;
};

const StartCheckCase START_CHECK_CASES[] = {
    {
        .input = {"a save outside the door stops before any step", true, outside_door},
        .expectation = {false, "start pose mismatch: "},
    },
    {
        .input = {"a save at the counter walks", true, at_counter},
        .expectation = {true, "start pose matches: "},
    },
    {
        .input = {"no frame kept walks, as before", false, outside_door},
        .expectation = {true, "start pose not read: no start frame kept"},
    },
};


//  The store, given two frames for 08.01 (outside the door, then the
//  counter): what it finds for 08.01 and 07.02.
struct StoreInput{
    std::string name;
    //  A folder to keep them in; none for the simulation's store.
    bool has_folder;
};
struct StoreExpectation{
    //  08.01's frame is found, and is the last one kept (the counter).
    bool finds_last;
};
struct StoreCase{
    StoreInput input;
    StoreExpectation expectation;
};

class Test_StartFrameStore : public UnitTest{
public:
    explicit Test_StartFrameStore(StoreCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryStartFrames - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        if (!m_case.input.has_folder){
            return check(StartFrameStore());
        }
        const std::string folder = Filesystem::unique_temp_path(USER_FILE_PATH() + "AutoStoryStartFrames").string() + "/";
        const UnitTestResult result = check(StartFrameStore(folder));
        Filesystem::remove_all(folder);
        return result;
    }

private:
    UnitTestResult check(const StartFrameStore& store) const{
        if (store.load("08.01")){
            return "expected no frame before one is kept";
        }
        const bool kept = store.save("08.01", start_frame(outside_door()), "test")
            && store.save("08.01", start_frame(at_counter()), "test");
        const std::optional<ImageRGB32> found = store.load("08.01");
        const bool finds_last = kept && found && found->width() == 240 && found->height() == 160
            && same_start_place(compare_start_frames(*found, at_counter()));
        if (finds_last != m_case.expectation.finds_last || store.load("07.02")){
            return std::format(
                "expected 08.01's last frame {}found and none for 07.02; kept: {}, found: {}",
                m_case.expectation.finds_last ? "" : "not ", kept, found.has_value()
            );
        }
        return true;
    }

    StoreCase m_case;
};

const StoreCase STORE_CASES[] = {
    {
        .input = {"the store keeps the last frame per checkpoint", true},
        .expectation = {true},
    },
    {
        .input = {"a store with no folder keeps nothing", false},
        .expectation = {false},
    },
};

} // namespace


void add_tests_AutoStoryStartFrames(UnitTestDatabase& database){
    for (const auto& item : COMPARE_CASES){
        database.add<Test_StartFrameCompare>(item);
    }
    for (const auto& item : READ_CASES){
        database.add<Test_StartFrameRead>(item);
    }
    for (const auto& item : START_CHECK_CASES){
        database.add<Test_StartFrameCheck>(item);
    }
    for (const auto& item : STORE_CASES){
        database.add<Test_StartFrameStore>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
