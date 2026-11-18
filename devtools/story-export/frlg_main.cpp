/*  Offline evaluated FRLG table export. No console or profile initialization.
 *  cmake -S SerialPrograms -B build -DPA_FRLG_STORY_EXPORT=ON
 *  cmake --build build --target StoryCheckpointExport -j4
 *  build/StoryCheckpointExport 02.01 > frlg-parcel.native.json
 *  Format the captured JSON with PokemonSimulation's contract formatter.
 */
#include <iostream>
#include <string_view>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryExport.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTable.h"

int main(int argc, char** argv){
    if (argc != 2){
        std::cerr << "Usage: StoryCheckpointExport CHECKPOINT_ID\n";
        return 2;
    }
    try{
        for (const auto& row : PokemonAutomation::NintendoSwitch::PokemonFRLG::story_checkpoints()){
            if (row.id != std::string_view(argv[1])){
                continue;
            }
            auto result = PokemonAutomation::NintendoSwitch::PokemonFRLG::story_checkpoint_export(row);
            result.to_object_throw()["source_revision"] = PA_STORY_EXPORT_REVISION;
            std::cout << result.dump(-1) << '\n';
            return 0;
        }
        std::cerr << "Unknown checkpoint: " << argv[1] << '\n';
    }catch (const PokemonAutomation::Exception& error){
        std::cerr << error.to_str() << '\n';
    }
    return 1;
}
