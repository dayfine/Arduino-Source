/*  PokemonLZA AutoStory - Screen events Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
  *  Folder frames name what they show ("0076-interact_prompt+player_can_move.png").
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

struct PromptAndControl{
    bool interact_prompt = false;
    bool can_move = false;

    bool operator==(const PromptAndControl&) const = default;
};

std::string to_text(const PromptAndControl& events){
    return std::format("A prompt {}, party bar {}", events.interact_prompt ? "yes" : "no", events.can_move ? "yes" : "no");
}

PromptAndControl observed(const std::string& path){
    PlzaScreenObserver observer;
    const ScreenEvents events = observer.read(ImageRGB32(path));
    return PromptAndControl{
        .interact_prompt = events.has(ScreenEvent::INTERACT_PROMPT),
        .can_move = events.has(ScreenEvent::PLAYER_CAN_MOVE),
    };
}

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
        //  No D-pad arrows; the empty slot frames show the bar is up.
        .name = "the elevator's A prompt over the party bar of one",
        .input = {.path = "AutoStoryScreenEvents/za2_m3v_s1.png"},
        .expectation = {.events = {.interact_prompt = true, .can_move = true}},
    },
    {
        //  01.02: no Pokemon, no party bar.
        .name = "the A Talk prompt on the Italian game",
        .input = {.path = "AutoStoryScreenEvents/it_0102_parla_talk_prompt.png"},
        .expectation = {.events = {.interact_prompt = true, .can_move = false}},
    },
    {
        .name = "the party bar of one Pokemon: five empty slot frames, no D-pad arrows",
        .input = {.path = "AutoStoryScreenEvents/de_0205_lobby_start_party1.png"},
        .expectation = {.events = {.interact_prompt = false, .can_move = true}},
    },
    {
        .name = "the elevator's A prompt over the party bar of one on the German game",
        .input = {.path = "AutoStoryScreenEvents/de_0205_elevator_prompt_steady.png"},
        .expectation = {.events = {.interact_prompt = true, .can_move = true}},
    },
    {
        .name = "the party bar of four over a sunlit road",
        .input = {.path = "AutoStoryScreenEvents/it_0308_zone1_road_no_battle.png"},
        .expectation = {.events = {.interact_prompt = false, .can_move = true}},
    },
};

struct BattleFrameCase{
    std::string name;
    struct Input{
        std::string_view path;  //  In CommandLineTests/PokemonLZA/.
    };
    Input input;
    struct Expectation{
        bool battle = false;
    };
    Expectation expectation;
};

const BattleFrameCase BATTLE_FRAME_CASES[] = {
    {
        .name = "a battle with the player not locked on: the panel, no move hints",
        .input = {.path = "AutoStoryScreenEvents/it_0302b_battle_no_lock_on.png"},
        .expectation = {.battle = true},
    },
    {
        .name = "a wild battle at the zone's pillar, not locked on",
        .input = {.path = "AutoStoryScreenEvents/it_0308b_zone_pillar_wild_battle.png"},
        .expectation = {.battle = true},
    },
    {
        .name = "the Tepig battle's start over light paving stone in Spanish, not locked on",
        .input = {.path = "AutoStoryScreenEvents/es_0302b_tepig_battle_start.png"},
        .expectation = {.battle = true},
    },
    {
        .name = "the Tepig battle with the lead at 9 HP over light paving stone in Spanish",
        .input = {.path = "AutoStoryScreenEvents/es_0302b_tepig_battle_low_hp.png"},
        .expectation = {.battle = true},
    },
    {
        .name = "no battle at the zone's pillar with the lead's panel up",
        .input = {.path = "AutoStoryScreenEvents/it_0308b_zone_pillar_no_battle.png"},
        .expectation = {.battle = false},
    },
    {
        .name = "no battle on the zone's road with the lead's panel up",
        .input = {.path = "AutoStoryScreenEvents/it_0308_zone1_road_no_battle.png"},
        .expectation = {.battle = false},
    },
    {
        .name = "no battle at the A Talk prompt on the Italian game",
        .input = {.path = "AutoStoryScreenEvents/it_0102_parla_talk_prompt.png"},
        .expectation = {.battle = false},
    },
    {
        .name = "no battle at the elevator's A prompt over the party bar of one",
        .input = {.path = "AutoStoryScreenEvents/za2_m3v_s1.png"},
        .expectation = {.battle = false},
    },
};

UnitTestResult check_battle_frame(const BattleFrameCase& test){
    const std::string path = std::format("{}PokemonLZA/{}", UNIT_TEST_RESOURCE_PATH(), test.input.path);
    PlzaScreenObserver observer;
    const bool battle = observer.read(ImageRGB32(path)).has(ScreenEvent::BATTLE_STARTED);
    if (battle != test.expectation.battle){
        return std::format("{}: expected battle {}, got {}", test.input.path, test.expectation.battle ? "yes" : "no", battle ? "yes" : "no");
    }
    return true;
}

struct DialogFrameCase{
    std::string name;
    struct Input{
        std::string_view path;  //  In CommandLineTests/PokemonLZA/.
    };
    Input input;
    struct Expectation{
        bool dialog = false;
        bool interact_prompt = false;
    };
    Expectation expectation;
};

const DialogFrameCase DIALOG_FRAME_CASES[] = {
    {
        //  Ends 03.09's walk (walk_to_unread_marker()).
        .name = "Ryon's five-species dialog at the lab gate on the Italian game",
        .input = {.path = "AutoStoryScreenEvents/it_0309_lab_gate_ryon_dialog.png"},
        .expectation = {.dialog = true, .interact_prompt = false},
    },
};

UnitTestResult check_dialog_frame(const DialogFrameCase& test){
    const std::string path = std::format("{}PokemonLZA/{}", UNIT_TEST_RESOURCE_PATH(), test.input.path);
    PlzaScreenObserver observer;
    const ScreenEvents events = observer.read(ImageRGB32(path));
    const bool dialog = events.has(ScreenEvent::DIALOG_BOX);
    const bool prompt = events.has(ScreenEvent::INTERACT_PROMPT);
    if (dialog != test.expectation.dialog || prompt != test.expectation.interact_prompt){
        return std::format(
            "{}: expected dialog {}, A prompt {}; got dialog {}, A prompt {}", test.input.path,
            test.expectation.dialog ? "yes" : "no", test.expectation.interact_prompt ? "yes" : "no",
            dialog ? "yes" : "no", prompt ? "yes" : "no"
        );
    }
    return true;
}

struct FolderCase{
    std::string name;
    struct Input{
        std::string_view folder;    //  In CommandLineTests/PokemonLZA/.
    };
    Input input;
    struct Expectation{
        size_t frames = 0;
    };
    Expectation expectation;
};

const FolderCase FOLDER_CASES[] = {
    {
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

class Test_BattleFrame : public UnitTest{
public:
    explicit Test_BattleFrame(const BattleFrameCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStoryScreenEvents - battle frame: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_battle_frame(m_test);
    }

private:
    const BattleFrameCase& m_test;
};

class Test_DialogFrame : public UnitTest{
public:
    explicit Test_DialogFrame(const DialogFrameCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStoryScreenEvents - dialog frame: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_dialog_frame(m_test);
    }

private:
    const DialogFrameCase& m_test;
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
    for (const BattleFrameCase& test : BATTLE_FRAME_CASES){
        database.add<Test_BattleFrame>(test);
    }
    for (const DialogFrameCase& test : DIALOG_FRAME_CASES){
        database.add<Test_DialogFrame>(test);
    }
    for (const FolderCase& test : FOLDER_CASES){
        database.add<Test_Folder>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
