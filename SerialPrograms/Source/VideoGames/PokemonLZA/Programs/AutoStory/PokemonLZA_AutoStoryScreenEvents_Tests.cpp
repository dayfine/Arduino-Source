/*  PokemonLZA AutoStory - Screen events Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Console frames (1920x1080) from CommandLineTests/PokemonLZA/. A frame
 *  of a folder names what it shows ("0076-interact_prompt+player_can_move.png",
 *  lab-B captures 2026-11-13).
 *
 */

#include <format>
#include <string>
#include <string_view>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"
#include "PokemonLZA_AutoStoryScreenEvents_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The events these tests check on a frame.
struct PromptAndControl{
    bool interact_prompt = false;
    bool can_move = false;

    bool operator==(const PromptAndControl&) const = default;
};

std::string to_text(const PromptAndControl& events){
    return std::format("A prompt {}, party bar {}", events.interact_prompt ? "yes" : "no", events.can_move ? "yes" : "no");
}

//  What the observer reads on the frame at `path`.
PromptAndControl observed(const std::string& path){
    PlzaScreenObserver observer;
    const ScreenEvents events = observer.read(ImageRGB32(path));
    return PromptAndControl{
        .interact_prompt = events.has(ScreenEvent::INTERACT_PROMPT),
        .can_move = events.has(ScreenEvent::PLAYER_CAN_MOVE),
    };
}

//  What a folder frame's name says it shows.
PromptAndControl as_named(std::string_view name){
    return PromptAndControl{
        .interact_prompt = name.find("interact_prompt") != std::string_view::npos,
        .can_move = name.find("player_can_move") != std::string_view::npos,
    };
}


struct FrameCase{
    std::string name;
    struct Input{
        std::string_view path;  //  In CommandLineTests/PokemonLZA/.
    };
    Input input;
    struct Expectation{
        PromptAndControl events;
    };
    Expectation expectation;
};

const FrameCase FRAME_CASES[] = {
    {
        .name = "an A prompt while the player is held",
        .input = {.path = "AutoStoryScreenEvents/za2_m3v_s1.png"},
        .expectation = {.events = {.interact_prompt = true, .can_move = false}},
    },
    {
        //  console B, 2026-11-15: 01.02's battle area, the scene over;
        //  "Parla" is Italian for "Talk", Ryon the man at the gate. No
        //  Pokemon yet, so no party bar.
        .name = "the A Talk prompt on the Italian game",
        .input = {.path = "AutoStoryScreenEvents/it_0102_parla_talk_prompt.png"},
        .expectation = {.events = {.interact_prompt = true, .can_move = false}},
    },
};

struct FolderCase{
    std::string name;
    struct Input{
        std::string_view folder;    //  In CommandLineTests/PokemonLZA/.
    };
    Input input;
    struct Expectation{
        //  How many frames the folder holds; each shows what its name says.
        size_t frames = 0;
    };
    Expectation expectation;
};

const FolderCase FOLDER_CASES[] = {
    {
        //  A black fade, the zone's gate with its "A" prompt and the party
        //  bar after the catches (03.07).
        .name = "the catches to the zone's start",
        .input = {.folder = "AutoStoryDetectors/CatchesToZone1Start"},
        .expectation = {.frames = 8},
    },
};

UnitTestResult check_frame(const FrameCase& test){
    const std::string path = std::format("{}PokemonLZA/{}", UNIT_TEST_RESOURCE_PATH(), test.input.path);
    const PromptAndControl got = observed(path);
    if (got != test.expectation.events){
        return std::format("{}: expected {}, got {}", test.input.path, to_text(test.expectation.events), to_text(got));
    }
    return true;
}

UnitTestResult check_folder(const FolderCase& test){
    const Filesystem::Path folder(std::format("{}PokemonLZA/{}/", UNIT_TEST_RESOURCE_PATH(), test.input.folder));
    if (!Filesystem::is_directory(folder)){
        return std::format("{} is not a folder.", folder.string());
    }
    size_t frames = 0;
    std::string failures;
    for (const Filesystem::Path& path : Filesystem::list_directory(folder)){
        const std::string name = path.filename().string();
        if (!name.ends_with(".jpg") && !name.ends_with(".png")){
            continue;
        }
        frames++;
        const PromptAndControl got = observed(path.string());
        if (got != as_named(name)){
            failures += std::format(" {}: {};", name, to_text(got));
        }
    }
    if (frames != test.expectation.frames){
        return std::format("Expected {} frames in {}, found {}.", test.expectation.frames, folder.string(), frames);
    }
    if (!failures.empty()){
        return std::format("Wrong on:{}", failures);
    }
    return true;
}


class Test_Frame : public UnitTest{
public:
    explicit Test_Frame(const FrameCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStoryScreenEvents - console frame: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_frame(m_test);
    }

private:
    const FrameCase& m_test;
};

class Test_Folder : public UnitTest{
public:
    explicit Test_Folder(const FolderCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStoryScreenEvents - console frames: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_folder(m_test);
    }

private:
    const FolderCase& m_test;
};

} // namespace


void add_tests_AutoStoryScreenEvents(UnitTestDatabase& database){
    for (const FrameCase& test : FRAME_CASES){
        database.add<Test_Frame>(test);
    }
    for (const FolderCase& test : FOLDER_CASES){
        database.add<Test_Folder>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
