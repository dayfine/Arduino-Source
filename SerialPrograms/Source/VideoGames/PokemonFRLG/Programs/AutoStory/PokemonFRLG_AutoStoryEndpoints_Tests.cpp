/*  AutoStory Endpoint Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Each checkpoint's row (StoryData/frlg-NN.NN.plan.json) walked on the
 *  overworld sim from its start: it must end on the tile and facing its
 *  plan declares (checkpoint_end_pose()). The coverage test holds every
 *  checkpoint of the story table to a case here or an exemption with its
 *  reason.
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryPostconditionCheck.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryEndpoints_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryEndpointInput{
    std::string_view checkpoint;
    //  A map whose trainers the story has all beaten by the row's start (a
    //  gym after its leader: set_gym_trainers), which the story maps don't
    //  know.
    std::optional<std::string_view> trainers_beaten_on = std::nullopt;
};

//  The end its plan declares (checkpoint_end_pose()): one of its tiles,
//  and its facing if it has one.
struct AutoStoryEndpointExpectation{
    std::string_view map;
    TilePos tile;
    std::optional<Direction> facing;
};

struct AutoStoryEndpointCase{
    std::string name;
    AutoStoryEndpointInput input;
    AutoStoryEndpointExpectation expectation;
};

//  A checkpoint with no endpoint case, and why.
struct EndpointExemption{
    std::string_view checkpoint;
    std::string_view reason;
};

const StoryCheckpoint* find_row(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return &row;
        }
    }
    return nullptr;
}

const StoryCheckpoint& endpoint_row(std::string_view id){
    const StoryCheckpoint* row = find_row(id);
    if (row == nullptr){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
    }
    return *row;
}

//  `maps` with every trainer on the map `beaten` (if any) gone.
std::vector<MapData> beaten_on(const std::vector<MapData>& maps, std::optional<std::string_view> beaten){
    std::vector<MapData> out = maps;
    for (MapData& map : out){
        if (!beaten || map.name != *beaten){
            continue;
        }
        for (MapObject& object : map.objects){
            object.trainer.reset();
        }
    }
    return out;
}

//  Whether `step` is a plain step onto a scene's trigger: no variant, no
//  pose of its own, and the next step (a scene, talk or battle the sim
//  has no script for) says where the player ends. The trigger and its
//  scene are the console's; the sim has none of the game's coord events.
bool onto_trigger(const StoryCheckpoint& row, size_t index){
    const StoryStep& step = row.steps[index];
    if (step.kind != StoryStepKind::STEP || !step.action.empty() || step.pose_after || index + 1 >= row.steps.size()){
        return false;
    }
    const StoryStep& next = row.steps[index + 1];
    const bool scripted = next.kind == StoryStepKind::MASH || next.kind == StoryStepKind::TALK
        || next.kind == StoryStepKind::ANSWER || next.kind == StoryStepKind::BATTLE;
    return scripted && next.pose_after.has_value();
}

//  Whether the overworld sim plays step `index` of `row` from where the
//  player stands: a walk, or a battle whose trainer's message is up (one
//  that saw the player). Any other step, and a step onto a trigger, puts
//  the player on its pose_after, if it has one: a talk, a scene, a Fly, a
//  heal are the console's.
bool sim_plays(const StoryCheckpoint& row, size_t index, const OverworldSim& sim){
    const StoryStep& step = row.steps[index];
    switch (step.kind){
    case StoryStepKind::ROUTE:
    case StoryStepKind::LEGS:
        return true;
    case StoryStepKind::STEP:
        return !onto_trigger(row, index);
    case StoryStepKind::BATTLE:
        return sim.state().screen == SimScreen::MESSAGE;
    default:
        return false;
    }
}

//  The pose the sim ends on, or the failure: the row's steps from its start
//  on the overworld sim, over the maps of its start flags (start_flags),
//  with every trainer on `trainers_beaten_on` beaten.
struct EndpointWalk{
    std::string failure;
    MapPosition position;
    Direction facing = Direction::SOUTH;
};

EndpointWalk walk_row_to_end(const StoryCheckpoint& row, std::optional<std::string_view> trainers_beaten_on){
    if (!row.start){
        return {.failure = std::format("{}: no start pose (a new game).", row.id)};
    }
    std::set<std::string> flags(row.start_flags.begin(), row.start_flags.end());
    const std::vector<MapData> maps = beaten_on(run_story_maps(flags), trainers_beaten_on);
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    std::optional<OverworldSim> sim;
    sim.emplace(maps, MapPosition{at.map, at.tile}, at.facing.value_or(Direction::SOUTH));
    for (size_t index = 0; index < row.steps.size(); index++){
        const StoryStep& step = row.steps[index];
        if (sim_plays(row, index, *sim)){
            if (std::string failure = row_step_on_sim(*sim, at, step, row.id, flags); !failure.empty()){
                return {.failure = std::move(failure)};
            }
        }else if (step.pose_after){
            at = *story_pose(Pokemon::pose_of(*step.pose_after));
            sim.emplace(maps, MapPosition{at.map, at.tile}, at.facing.value_or(sim->state().facing));
        }
        if (!sim->state().violations.empty()){
            return {.failure = std::format("{}/{}: broke a rule: {}", row.id, step.name, sim->state().violations.front())};
        }
        flags.insert(step.flags_after.begin(), step.flags_after.end());
    }
    return {.position = sim->state().position, .facing = sim->state().facing};
}

std::string facing_name(std::optional<Direction> facing){
    return facing ? direction_name(*facing) : "any way";
}

UnitTestResult check_endpoint(const AutoStoryEndpointCase& test_case){
    const StoryCheckpoint& row = endpoint_row(test_case.input.checkpoint);
    const AutoStoryEndpointExpectation& expected = test_case.expectation;
    const MapPosition end{std::string(expected.map), expected.tile};
    const std::optional<Pokemon::GridPose> declared = Pokemon::checkpoint_end_pose(row);
    if (!declared || declared->map != expected.map || declared->facing != expected.facing
        || std::find(declared->tiles.begin(), declared->tiles.end(), expected.tile) == declared->tiles.end()
    ){
        return std::format("{}: the plan's end pose is not {} facing {}.", row.id, to_string(end), facing_name(expected.facing));
    }
    const EndpointWalk walk = walk_row_to_end(row, test_case.input.trainers_beaten_on);
    if (!walk.failure.empty()){
        return walk.failure;
    }
    if (!(walk.position == end) || (expected.facing && walk.facing != *expected.facing)){
        return std::format(
            "{}: ended on {} facing {}, not {} facing {}.",
            row.id, to_string(walk.position), direction_name(walk.facing), to_string(end), facing_name(expected.facing)
        );
    }
    return true;
}

class Test_AutoStoryEndpoint : public UnitTest{
public:
    explicit Test_AutoStoryEndpoint(AutoStoryEndpointCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Simulation: endpoint {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return check_endpoint(m_case);
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    AutoStoryEndpointCase m_case;
};

//  One case per checkpoint, its expectation the end its plan declares.
const AutoStoryEndpointCase AUTO_STORY_ENDPOINT_CASES[] = {
    {
        .name = "01.02 after SID Helper: close the Trainer Card, save in the bedroom",
        .input = {.checkpoint = "01.02"},
        .expectation = {.map = "PalletTown_PlayersHouse_2F", .tile = {6, 6}, .facing = Direction::NORTH},
    },
    {
        .name = "01.03 bedroom to Pallet Town",
        .input = {.checkpoint = "01.03"},
        .expectation = {.map = "PalletTown", .tile = {6, 8}, .facing = std::nullopt},
    },
    {
        .name = "01.04 Oak stops the player and walks them to the lab",
        .input = {.checkpoint = "01.04"},
        .expectation = {.map = "PalletTown_ProfessorOaksLab", .tile = {6, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "01.05 to Squirtle's ball, south of the table",
        .input = {.checkpoint = "01.05"},
        .expectation = {.map = "PalletTown_ProfessorOaksLab", .tile = {9, 5}, .facing = Direction::NORTH},
    },
    {
        .name = "01.06 Squirtle, then the rival's battle",
        .input = {.checkpoint = "01.06"},
        .expectation = {.map = "PalletTown_ProfessorOaksLab", .tile = {7, 8}, .facing = Direction::SOUTH},
    },
    {
        .name = "02.01 Route 1 to Viridian Mart: Oak's Parcel",
        .input = {.checkpoint = "02.01"},
        .expectation = {.map = "ViridianCity_Mart", .tile = {4, 3}, .facing = Direction::WEST},
    },
    {
        .name = "02.02 Oak's Parcel to the Pokedex",
        .input = {.checkpoint = "02.02"},
        .expectation = {.map = "PalletTown_ProfessorOaksLab", .tile = {6, 4}, .facing = Direction::SOUTH},
    },
    {
        .name = "02.03 Viridian shopping, then into Viridian Forest",
        .input = {.checkpoint = "02.03"},
        .expectation = {.map = "ViridianForest", .tile = {29, 62}, .facing = Direction::NORTH},
    },
    {
        .name = "03.01 Viridian Forest to Pewter Center",
        .input = {.checkpoint = "03.01"},
        .expectation = {.map = "PewterCity_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "03.02 Pewter Gym: Camper Liam, then Brock",
        .input = {.checkpoint = "03.02"},
        .expectation = {.map = "PewterCity_Gym", .tile = {6, 6}, .facing = Direction::NORTH},
    },
    {
        .name = "03.03 Pewter: the Running Shoes from Oak's aide",
        .input = {.checkpoint = "03.03"},
        .expectation = {.map = "PewterCity", .tile = {17, 26}, .facing = Direction::SOUTH},
    },
    {
        .name = "04.02 Route 3 and 4 to the Magikarp salesman",
        .input = {.checkpoint = "04.02"},
        .expectation = {.map = "Route4_PokemonCenter_1F", .tile = {2, 3}, .facing = Direction::WEST},
    },
    {
        .name = "04.03 after Gift RNG: save with the Magikarp",
        .input = {.checkpoint = "04.03"},
        .expectation = {.map = "Route4_PokemonCenter_1F", .tile = {2, 3}, .facing = Direction::WEST},
    },
    {
        .name = "04.04 Route 4 Center: the Magikarp into BOX 1",
        .input = {.checkpoint = "04.04"},
        .expectation = {.map = "Route4_PokemonCenter_1F", .tile = {11, 2}, .facing = Direction::NORTH},
    },
    {
        .name = "04.06 Route 4: Mega Kick from its tutor, then east to Cerulean's Pokemon Center",
        .input = {.checkpoint = "04.06"},
        .expectation = {.map = "CeruleanCity_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "05.02 Cerulean Gym: Misty, then Water Pulse",
        .input = {.checkpoint = "05.02"},
        .expectation = {.map = "CeruleanCity_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "05.03 Cerulean: Water Pulse, Potions, a GRIND until the rival is winnable, then saved below his trigger",
        .input = {.checkpoint = "05.03"},
        .expectation = {.map = "CeruleanCity", .tile = {23, 7}, .facing = Direction::NORTH},
    },
    {
        .name = "05.04 Cerulean: Water Pulse if not known, then the rival north of the city",
        .input = {.checkpoint = "05.04"},
        .expectation = {.map = "CeruleanCity", .tile = {23, 6}, .facing = Direction::SOUTH},
    },
    {
        .name = "05.05 Nugget Bridge: the nurse, five trainers and the Rocket",
        .input = {.checkpoint = "05.05"},
        .expectation = {.map = "Route24", .tile = {11, 15}, .facing = Direction::EAST},
    },
    {
        .name = "05.06 Route 24 and 25 to Bill's Sea Cottage: Franklin, Kelsey, Nob and Haley",
        .input = {.checkpoint = "05.06"},
        .expectation = {.map = "Route25_SeaCottage", .tile = {7, 8}, .facing = Direction::NORTH},
    },
    {
        .name = "05.07 Bill's Sea Cottage: the Cell Separator and the S.S. Ticket",
        .input = {.checkpoint = "05.07"},
        .expectation = {.map = "Route25", .tile = {51, 5}, .facing = Direction::SOUTH},
    },
    {
        .name = "05.08 Route 25's south path back to Cerulean's Pokemon Center",
        .input = {.checkpoint = "05.08"},
        .expectation = {.map = "CeruleanCity_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "05.09 Route 24's grass: an Oddish for Cut, then the Center",
        .input = {.checkpoint = "05.09"},
        .expectation = {.map = "CeruleanCity_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "05.10 The Cerulean Rocket, Route 5, the Underground Path and Route 6 to Vermilion's Pokemon Center: Jeff",
        .input = {.checkpoint = "05.10"},
        .expectation = {.map = "VermilionCity_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "06.01 The S.S. Anne: the rival, HM01 Cut from the Captain, back to Vermilion's Pokemon Center",
        .input = {.checkpoint = "06.01"},
        .expectation = {.map = "VermilionCity_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "06.03 Route 6's grass until the lead is Lv31, then through the open beams to Lt. Surge",
        //  06.02 fights Dwayne, Baily and Tucker.
        .input = {.checkpoint = "06.03", .trainers_beaten_on = "VermilionCity_Gym"},
        .expectation = {.map = "VermilionCity_Gym", .tile = {5, 3}, .facing = Direction::NORTH},
    },
    {
        .name = "06.04 Vermilion Gym: Lt. Surge, the THUNDERBADGE, then the Center",
        .input = {.checkpoint = "06.04"},
        .expectation = {.map = "VermilionCity_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "07.01 The BIKE VOUCHER from the Fan Club, back north to Cerulean, the BICYCLE, Repels and Escape Ropes",
        .input = {.checkpoint = "07.01"},
        .expectation = {.map = "CeruleanCity_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "07.02 East over Route 9 to Route 10's Pokemon Center: the two trees, Alicia, Jeremy and Conner",
        .input = {.checkpoint = "07.02"},
        .expectation = {.map = "Route10_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "08.01 Through Rock Tunnel: Ashton, Winston, Martha, Allen, Lenny, Sofia, Dudley, Cooper and Dana",
        .input = {.checkpoint = "08.01"},
        .expectation = {.map = "Route10", .tile = {8, 58}, .facing = Direction::SOUTH},
    },
    {
        .name = "08.02 Route 10 south to Lavender Town's Pokemon Center",
        .input = {.checkpoint = "08.02"},
        .expectation = {.map = "LavenderTown_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "08.03 Lavender Mart, then west over Route 8 on the Bicycle, the Underground Path, a Pidgey on Route 7, Celadon's Pokemon Center",
        .input = {.checkpoint = "08.03"},
        .expectation = {.map = "CeladonCity_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "09.02 Rocket Hideout: B2F's arrows and the MOON STONE, the lift to B4F, the two door guards, Giovanni, the SILPH SCOPE and an Escape Rope out",
        .input = {.checkpoint = "09.02"},
        .expectation = {.map = "CeladonCity", .tile = {34, 22}, .facing = Direction::SOUTH},
    },
    {
        .name = "09.03 Celadon Mansion: the TEA on 1F, then the roof room, saved facing the Eevee's ball",
        .input = {.checkpoint = "09.03"},
        .expectation = {.map = "CeladonCity_Condominiums_RoofRoom", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "09.04 after Gift RNG: save with the Eevee",
        .input = {.checkpoint = "09.04"},
        .expectation = {.map = "CeladonCity_Condominiums_RoofRoom", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "10.01 X items on the Department Store's 5F, HM02 Fly from Route 16's house, taught to the Pidgey, and Fly to Lavender Town's Pokemon Center",
        .input = {.checkpoint = "10.01"},
        .expectation = {.map = "LavenderTown_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "10.02 Pokemon Tower 1F and 2F, saved below the rival's trigger",
        .input = {.checkpoint = "10.02"},
        .expectation = {.map = "PokemonTower_2F", .tile = {17, 6}, .facing = Direction::WEST},
    },
    {
        .name = "10.03 Pokemon Tower 2F: the rival",
        .input = {.checkpoint = "10.03"},
        .expectation = {.map = "PokemonTower_2F", .tile = {16, 6}, .facing = Direction::NORTH},
    },
    {
        .name = "10.04 Pokemon Tower 3F to 5F: a Super Repel, Ruth and Tammy",
        .input = {.checkpoint = "10.04"},
        .expectation = {.map = "PokemonTower_5F", .tile = {18, 7}, .facing = Direction::SOUTH},
    },
    {
        .name = "10.05 Pokemon Tower 6F: Jennifer, Emilia, the Rare Candy and the Marowak ghost",
        .input = {.checkpoint = "10.05"},
        .expectation = {.map = "PokemonTower_6F", .tile = {11, 15}, .facing = Direction::SOUTH},
    },
    {
        .name = "10.06 Pokemon Tower 7F: three Rocket Grunts, Mr. Fuji and the POKE FLUTE",
        .input = {.checkpoint = "10.06"},
        .expectation = {.map = "LavenderTown_VolunteerPokemonHouse", .tile = {3, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "11.01 Fly to Celadon City, wake Route 16's Snorlax with the POKE FLUTE and run from it",
        .input = {.checkpoint = "11.01"},
        .expectation = {.map = "Route16", .tile = {28, 13}, .facing = Direction::WEST},
    },
    {
        .name = "11.02 Down the Cycling Road (Route 17) with its RARE CANDY and MAX ELIXIR, to the Safari Zone's door in Fuchsia City",
        .input = {.checkpoint = "11.02"},
        .expectation = {.map = "FuchsiaCity", .tile = {24, 6}, .facing = Direction::NORTH},
    },
    {
        .name = "11.03 The Safari Zone: the FULL RESTORE, the GOLD TEETH and HM03 SURF from the Secret House, and Fly back to Fuchsia City",
        .input = {.checkpoint = "11.03"},
        .expectation = {.map = "FuchsiaCity", .tile = {25, 32}, .facing = Direction::SOUTH},
    },
    {
        .name = "12.01 Fuchsia: heal, HM03 Surf over Water Gun, the gym past Jugglers Kayden and Kirk and round the top edge to west of Koga",
        .input = {.checkpoint = "12.01"},
        .expectation = {.map = "FuchsiaCity_Gym", .tile = {6, 13}, .facing = Direction::EAST},
    },
    {
        .name = "12.02 Koga, the SOULBADGE and TM06 Toxic",
        .input = {.checkpoint = "12.02"},
        .expectation = {.map = "FuchsiaCity_Gym", .tile = {6, 13}, .facing = Direction::EAST},
    },
    {
        .name = "12.02b The GOLD TEETH to the Warden for HM04 STRENGTH; heal at Fuchsia's Center",
        .input = {.checkpoint = "12.02b"},
        .expectation = {.map = "FuchsiaCity_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "13.01 Cinnabar to the Pokemon Mansion: 1F and 2F up to 3F's room of holes, the statue switch pressed on the way",
        .input = {.checkpoint = "13.01"},
        .expectation = {.map = "PokemonMansion_3F", .tile = {19, 13}, .facing = Direction::SOUTH},
    },
    {
        .name = "13.02 Pokemon Mansion 3F's hole to 1F, 1F's stairs to B1F",
        .input = {.checkpoint = "13.02"},
        .expectation = {.map = "PokemonMansion_B1F", .tile = {34, 29}, .facing = Direction::WEST},
    },
    {
        .name = "13.03 Pokemon Mansion B1F: the Elixir and TM14",
        .input = {.checkpoint = "13.03"},
        .expectation = {.map = "PokemonMansion_B1F", .tile = {23, 5}, .facing = Direction::NORTH},
    },
    {
        .name = "13.04 Pokemon Mansion B1F to the SECRET KEY, out by an Escape Rope",
        .input = {.checkpoint = "13.04"},
        .expectation = {.map = "CinnabarIsland", .tile = {8, 4}, .facing = Direction::SOUTH},
    },
    {
        .name = "14.02 Blaine and the VOLCANOBADGE",
        .input = {.checkpoint = "14.02"},
        .expectation = {.map = "CinnabarIsland_Gym", .tile = {5, 5}, .facing = Direction::NORTH},
    },
    {
        .name = "14.03 Cinnabar's Center out to the street and Fly to Celadon City, Cut into the gym's yard, Beauty Tamia, the inner tree, Beauty Lori, to below Erika",
        .input = {.checkpoint = "14.03"},
        .expectation = {.map = "CeladonCity_Gym", .tile = {6, 5}, .facing = Direction::NORTH},
    },
    {
        .name = "14.04 Celadon Gym: Erika, the RAINBOWBADGE and TM19",
        .input = {.checkpoint = "14.04"},
        .expectation = {.map = "CeladonCity_Gym", .tile = {6, 5}, .facing = Direction::NORTH},
    },
    {
        .name = "15.01 Celadon to Saffron: the TEA to Route 7's gate guard, heal at Saffron's Center",
        .input = {.checkpoint = "15.01"},
        .expectation = {.map = "SaffronCity_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "16.01 Saffron to Silph Co.: 3F's pad to 9F's, 9F's to 5F's corridor, the CARD KEY",
        .input = {.checkpoint = "16.01"},
        .expectation = {.map = "SilphCo_5F", .tile = {15, 7}, .facing = Direction::WEST},
    },
    {
        .name = "16.03b To the employee with the Lapras",
        .input = {.checkpoint = "16.03b"},
        .expectation = {.map = "SilphCo_7F", .tile = {1, 7}, .facing = Direction::WEST},
    },
    {
        .name = "16.04 after Gift RNG: save with the Lapras",
        .input = {.checkpoint = "16.04"},
        .expectation = {.map = "SilphCo_7F", .tile = {1, 7}, .facing = Direction::WEST},
    },
    {
        .name = "17.02 Saffron to Viridian Gym: Fly, Warren",
        .input = {.checkpoint = "17.02"},
        .expectation = {.map = "ViridianCity_Gym", .tile = {15, 7}, .facing = Direction::NORTH},
    },
    {
        .name = "17.03 Viridian Gym: Takashi, to Giovanni",
        .input = {.checkpoint = "17.03"},
        .expectation = {.map = "ViridianCity_Gym", .tile = {2, 3}, .facing = Direction::NORTH},
    },
    {
        .name = "17.04 Viridian Gym: Giovanni and the EARTHBADGE",
        .input = {.checkpoint = "17.04"},
        .expectation = {.map = "ViridianCity_Gym", .tile = {2, 3}, .facing = Direction::NORTH},
    },
    {
        .name = "17.04b Out of the gym, heal at Viridian's Center",
        .input = {.checkpoint = "17.04b", .trainers_beaten_on = "ViridianCity_Gym"},
        .expectation = {.map = "ViridianCity_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "18.01 Saffron Gym: the teleport pads from the door to Sabrina's room",
        .input = {.checkpoint = "18.01"},
        .expectation = {.map = "SaffronCity_Gym", .tile = {14, 12}, .facing = Direction::NORTH},
    },
    {
        .name = "18.02 Saffron Gym: Sabrina and the MARSHBADGE, the pads back out, heal at Saffron's Center",
        .input = {.checkpoint = "18.02"},
        .expectation = {.map = "SaffronCity_PokemonCenter_1F", .tile = {7, 4}, .facing = Direction::NORTH},
    },
    {
        .name = "19.01 The rival on Route 22, at the road's east end",
        .input = {.checkpoint = "19.01"},
        .expectation = {.map = "Route22", .tile = {33, 5}, .facing = Direction::WEST},
    },
    {
        .name = "19.01a Viridian's Mart: potions before the rival on Route 22",
        .input = {.checkpoint = "19.01a"},
        .expectation = {.map = "ViridianCity_Mart", .tile = {4, 3}, .facing = Direction::WEST},
    },
    {
        .name = "19.02 Route 22's north gate: the Boulder Badge guard, onto Route 23",
        .input = {.checkpoint = "19.02"},
        .expectation = {.map = "Route23", .tile = {8, 153}, .facing = Direction::NORTH},
    },
    {
        .name = "19.04 Victory Road: HM04 Strength over Water Pulse, into the cave",
        .input = {.checkpoint = "19.04"},
        .expectation = {.map = "VictoryRoad_1F", .tile = {11, 20}, .facing = Direction::NORTH},
    },
    {
        .name = "19.05 Victory Road's exit to Indigo Plateau: heal at its Center",
        .input = {.checkpoint = "19.05"},
        .expectation = {.map = "IndigoPlateau_PokemonCenter_1F", .tile = {13, 12}, .facing = Direction::NORTH},
    },
    {
        .name = "20.01 The Elite Four's first room: Lorelei",
        .input = {.checkpoint = "20.01"},
        .expectation = {.map = "PokemonLeague_LoreleisRoom", .tile = {6, 6}, .facing = Direction::NORTH},
    },
    {
        .name = "20.01a Indigo Plateau's Mart: Full Restores before the Elite Four",
        .input = {.checkpoint = "20.01a"},
        .expectation = {.map = "IndigoPlateau_PokemonCenter_1F", .tile = {2, 7}, .facing = Direction::WEST},
    },
    {
        .name = "20.02 The Elite Four: Bruno",
        .input = {.checkpoint = "20.02"},
        .expectation = {.map = "PokemonLeague_BrunosRoom", .tile = {6, 6}, .facing = Direction::NORTH},
    },
    {
        .name = "20.03 The Elite Four: Agatha",
        .input = {.checkpoint = "20.03"},
        .expectation = {.map = "PokemonLeague_AgathasRoom", .tile = {6, 6}, .facing = Direction::NORTH},
    },
    {
        .name = "20.04 The Elite Four: Lance",
        .input = {.checkpoint = "20.04"},
        .expectation = {.map = "PokemonLeague_LancesRoom", .tile = {6, 9}, .facing = Direction::NORTH},
    },
    {
        .name = "20.05 The Champion, then the Hall of Fame",
        .input = {.checkpoint = "20.05"},
        .expectation = {.map = "PokemonLeague_HallOfFame", .tile = {5, 4}, .facing = Direction::NORTH},
    },

};

//  The checkpoints with no endpoint case: no start or no declared end to
//  check, or a walk the generic walker can't play, each checked by its own
//  test or known red (a finding, not yet fixed).
const EndpointExemption ENDPOINT_EXEMPTIONS[] = {
    {.checkpoint = "01.01", .reason = "a new game: no start pose"},
    {.checkpoint = "04.01", .reason = "ends on LEGS taps (Route 3): no declared end (checkpoint_end_pose())"},
    {.checkpoint = "04.05", .reason = "its helix_gap step is a plain step the sim reads as a trainer's line, and no trainer is there"},
    {.checkpoint = "05.01", .reason = "ends on LEGS taps (Cerulean Gym): no declared end (checkpoint_end_pose())"},
    {.checkpoint = "06.02", .reason = "Vermilion Gym's Cut tree and trash cans: PokemonFRLG_AutoStoryVermilionGym_Tests.cpp"},
    {.checkpoint = "09.01", .reason = "the poster guard's step ends on (11, 3), not (11, 2): PokemonFRLG_AutoStoryHideout_Tests.cpp"},
    {.checkpoint = "12.03", .reason = "Route 21's Surf landing on the sand bar needs the Surf state the generic walk lacks: PokemonFRLG_AutoStorySurf_Tests.cpp"},
    {.checkpoint = "14.01", .reason = "ends on LEGS taps (the quiz machines): no declared end (checkpoint_end_pose())"},
    {.checkpoint = "14.02b", .reason = "the quiz doors' gates are shut on a fresh sim: PokemonFRLG_AutoStoryCinnabarGym_Tests.cpp"},
    {.checkpoint = "14.04b", .reason = "Celadon Gym's legs need the cut tree: PokemonFRLG_AutoStoryCeladonGym_Tests.cpp"},
    {.checkpoint = "16.02", .reason = "ends on LEGS taps (the pad pair): no declared end (checkpoint_end_pose())"},
    {.checkpoint = "16.03", .reason = "Silph Co.'s card-key doors and pads: PokemonFRLG_AutoStorySilph_Tests.cpp"},
    {.checkpoint = "16.05", .reason = "Silph Co.'s card-key doors and pads: PokemonFRLG_AutoStorySilph_Tests.cpp"},
    {.checkpoint = "16.06", .reason = "Silph Co.'s card-key doors and pads: PokemonFRLG_AutoStorySilph_Tests.cpp"},
    {.checkpoint = "19.03", .reason = "the soul guard's gate scene moves the player: PokemonFRLG_AutoStoryGates_Tests.cpp"},
    {.checkpoint = "19.04b", .reason = "Victory Road's boulders and floor switches: PokemonFRLG_AutoStoryBoulders_Tests.cpp"},
    {.checkpoint = "19.04c", .reason = "Victory Road's boulders and floor switches: PokemonFRLG_AutoStoryBoulders_Tests.cpp"},
    {.checkpoint = "19.04d", .reason = "Victory Road's boulders and floor switches: PokemonFRLG_AutoStoryBoulders_Tests.cpp"},
};

struct EndpointCoverageInput{
    std::span<const AutoStoryEndpointCase> cases;
    std::span<const EndpointExemption> exemptions;
};

struct EndpointCoverageExpectation{
    std::string_view description;
};

struct EndpointCoverageCase{
    std::string name;
    EndpointCoverageInput input;
    EndpointCoverageExpectation expectation;
};

//  Every checkpoint of the story table has an endpoint case or an
//  exemption, not both, and each names a checkpoint that exists.
UnitTestResult check_endpoint_coverage(const EndpointCoverageInput& input){
    std::set<std::string_view> covered;
    for (const AutoStoryEndpointCase& item : input.cases){
        if (find_row(item.input.checkpoint) == nullptr || !covered.insert(item.input.checkpoint).second){
            return std::format("Endpoint case {}: no such checkpoint, or a second case.", item.input.checkpoint);
        }
    }
    for (const EndpointExemption& exemption : input.exemptions){
        if (find_row(exemption.checkpoint) == nullptr || exemption.reason.empty() || !covered.insert(exemption.checkpoint).second){
            return std::format("Exemption {}: no such checkpoint, no reason, or it has a case too.", exemption.checkpoint);
        }
    }
    std::string missing;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (!covered.contains(row.id)){
            missing += std::format("{}{}", missing.empty() ? "" : ", ", row.id);
        }
    }
    if (!missing.empty()){
        return std::format("No endpoint case or exemption: {}.", missing);
    }
    return true;
}

class Test_AutoStoryEndpointCoverage : public UnitTest{
public:
    explicit Test_AutoStoryEndpointCoverage(EndpointCoverageCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Simulation: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_endpoint_coverage(m_case.input);
    }

private:
    EndpointCoverageCase m_case;
};

const EndpointCoverageCase ENDPOINT_COVERAGE_CASES[] = {
    {
        .name = "endpoint cases cover every checkpoint",
        .input = {.cases = AUTO_STORY_ENDPOINT_CASES, .exemptions = ENDPOINT_EXEMPTIONS},
        .expectation = {.description = "each checkpoint has one case or one exemption with a reason"},
    },
};

} // namespace


void add_tests_AutoStoryEndpoints(UnitTestDatabase& database){
    for (const AutoStoryEndpointCase& item : AUTO_STORY_ENDPOINT_CASES){
        database.add<Test_AutoStoryEndpoint>(item);
    }
    for (const EndpointCoverageCase& item : ENDPOINT_COVERAGE_CASES){
        database.add<Test_AutoStoryEndpointCoverage>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
