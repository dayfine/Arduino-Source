#include <QApplication>
#include <iostream>
#include "CommonFramework/StaticGlobals.h"
namespace PokemonAutomation{ bool USE_QT_UI = true; }
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_MarkerNavigation.h"
using namespace PokemonAutomation;
using namespace PokemonAutomation::NintendoSwitch::PokemonSV;
int main(int argc, char** argv){
    QApplication app(argc, argv);
    struct Case{ const char* file; DestinationMarkerState expected; };
    const Case cases[]{
        {.file = "uphill-marker-heading.png", .expected = DestinationMarkerState::PRESENT},
        {.file = "uphill-lower-corridor.png", .expected = DestinationMarkerState::MISSING},
        {.file = "uphill-gastly-interruption.png", .expected = DestinationMarkerState::MISSING},
        {.file = "uphill-user-completed-endpoint.png", .expected = DestinationMarkerState::MISSING},
    };
    for (const auto& test : cases){
        ImageRGB32 screenshot(std::string("/Users/difan/Projects/Arduino-Source-agents/pokemon_sv/runs/isolated-2026-10-24/evidence/") + test.file);
        const auto actual = read_destination_marker(screenshot);
        std::cout << test.file << ": expected=" << (int)test.expected << ", read=" << (int)actual << std::endl;
        const bool ahead = destination_marker_ahead(screenshot);
        std::cout << "  bearing gate=" << ahead << std::endl;
        if (actual != test.expected || ahead != (test.expected == DestinationMarkerState::PRESENT)){ return 1; }
    }
    return 0;
}
