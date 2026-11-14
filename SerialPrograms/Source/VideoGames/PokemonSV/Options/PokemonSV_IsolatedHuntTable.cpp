#include "Common/Cpp/Json/JsonValue.h"
#include "PokemonSV_IsolatedHuntTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{

const StringSelectDatabase& targets(){
    static const StringSelectDatabase database([]{
        StringSelectDatabase entries;
        entries.add_entry(StringSelectEntry("charcadet", "Charcadet"));
        entries.add_entry(StringSelectEntry("chespin", "Chespin"));
        return entries;
    }());
    return database;
}

const EnumDropdownDatabase<IsolatedEncounterType>& types(){
    static const EnumDropdownDatabase<IsolatedEncounterType> database{
        {IsolatedEncounterType::FIRE, "fire", "Fire"},
        {IsolatedEncounterType::GRASS, "grass", "Grass"},
    };
    return database;
}

const EnumDropdownDatabase<IsolatedHuntLocation>& locations(){
    static const EnumDropdownDatabase<IsolatedHuntLocation> database{
        {IsolatedHuntLocation::WEST_PROVINCE_ONE_CENTRAL, "west-province-one-central", "West Province One Central"},
        {IsolatedHuntLocation::CHARGESTONE_CAVE, "chargestone-cave", "Chargestone Cavern"},
    };
    return database;
}

const EnumDropdownDatabase<IsolatedShinyAction>& actions(){
    static const EnumDropdownDatabase<IsolatedShinyAction> database{
        {IsolatedShinyAction::STOP, "stop", "Stop"},
        {IsolatedShinyAction::CATCH, "catch", "Catch"},
        {IsolatedShinyAction::CATCH_AND_SAVE, "catch-and-save", "Catch and save"},
    };
    return database;
}

}

IsolatedHuntRow::IsolatedHuntRow(EditableTableOption& parent)
    : EditableTableRow(parent)
    , enabled(LockMode::LOCK_WHILE_RUNNING, true)
    , pokemon(targets(), LockMode::LOCK_WHILE_RUNNING, "charcadet")
    , type(types(), LockMode::LOCK_WHILE_RUNNING, IsolatedEncounterType::FIRE)
    , location(locations(), LockMode::LOCK_WHILE_RUNNING, IsolatedHuntLocation::WEST_PROVINCE_ONE_CENTRAL)
    , shiny_action(actions(), LockMode::LOCK_WHILE_RUNNING, IsolatedShinyAction::STOP)
    , ball(LockMode::LOCK_WHILE_RUNNING, "poke-ball")
    , ball_limit(LockMode::LOCK_WHILE_RUNNING, 40, 1, 999)
{
    PA_ADD_OPTION(enabled);
    PA_ADD_OPTION(pokemon);
    PA_ADD_OPTION(type);
    PA_ADD_OPTION(location);
    PA_ADD_OPTION(shiny_action);
    PA_ADD_OPTION(ball);
    PA_ADD_OPTION(ball_limit);
}

std::unique_ptr<EditableTableRow> IsolatedHuntRow::clone() const{
    auto row = std::make_unique<IsolatedHuntRow>(parent());
    row->load_json(to_json());
    return row;
}

IsolatedHuntRequest IsolatedHuntRow::snapshot() const{
    return {
        .pokemon = pokemon.slug(), .type = type, .location = location,
        .ball = ball.slug(), .ball_limit = ball_limit, .enabled = enabled,
        .shiny_action = shiny_action,
    };
}

IsolatedHuntTable::IsolatedHuntTable()
    : EditableTableOption_t<IsolatedHuntRow>(
        "<b>Hunts:</b><br>Choose a compatible target/type/location. Routes are experimental; verify the actual spawn patch.",
        LockMode::LOCK_WHILE_RUNNING, make_defaults()
    )
{}

std::vector<std::string> IsolatedHuntTable::make_header() const{
    return {"Enabled", "Pokemon", "Encounter Type", "Location", "Shiny Action", "Ball", "Ball Limit"};
}

std::vector<IsolatedHuntRequest> IsolatedHuntTable::snapshot() const{
    return EditableTableOption_t<IsolatedHuntRow>::snapshot<IsolatedHuntRequest>();
}

std::vector<std::unique_ptr<EditableTableRow>> IsolatedHuntTable::make_defaults(){
    std::vector<std::unique_ptr<EditableTableRow>> rows;
    rows.push_back(std::make_unique<IsolatedHuntRow>(*this));
    return rows;
}

}
}
}
