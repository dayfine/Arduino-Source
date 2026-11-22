/*  BDSP RNG Targets
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include <string_view>
#include "Common/Cpp/Exceptions.h"
#include "PokemonBDSP_RngTargets.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace Pokemon;


namespace{

BdspStaticTemplate starter_template(){
    BdspStaticTemplate ret;
    ret.species = "starter";
    ret.level = 5;
    ret.guaranteed_ivs = 0;
    ret.ability_kind = 3;
    ret.gender_ratio = 31;      //  Seven males to one female.
    return ret;
}

BdspStaticTemplate legendary_template(std::string_view species, uint8_t level){
    BdspStaticTemplate ret;
    ret.species = std::string(species);
    ret.level = level;
    ret.guaranteed_ivs = 3;
    ret.ability_kind = 3;
    ret.gender_ratio = 255;     //  Genderless.
    return ret;
}

BdspStaticTemplate cresselia_template(){
    BdspStaticTemplate ret = legendary_template("cresselia", 50);
    ret.gender_ratio = 254;     //  Always female.
    ret.roamer = true;
    return ret;
}

const BdspRngTargetInfo STARTER_TARGET{
    "starter", "Starter",
    starter_template(),
    BdspTimelineContext{
        /*npcs*/                          0,
        /*pokemon_models*/                2,
        /*white_delay_seconds*/           0.0,
        /*advance_delay*/                 41,
        /*advance_delay_2*/               48,
        /*advance_delay_2_after_events*/  10,
        /*plus_one_on_menu_close*/        false,
        /*advances_after_accept*/         65,
    },
    /*observation_npcs*/ 2,
    /*has_timeline*/ true,
};
const BdspRngTargetInfo DIALGA_TARGET{
    "dialga", "Dialga", legendary_template("dialga", 47),
    BdspTimelineContext{1, 1, 2.0, 0, 0, 10, false}, 1, true,
};
const BdspRngTargetInfo PALKIA_TARGET{
    "palkia", "Palkia", legendary_template("palkia", 47),
    BdspTimelineContext{1, 1, 2.0, 0, 0, 10, false}, 1, true,
};
const BdspRngTargetInfo GIRATINA_TARGET{
    "giratina", "Giratina", legendary_template("giratina", 47),
    BdspTimelineContext{1, 1, 2.0, 2, 0, 10, false}, 1, true,
};
const BdspRngTargetInfo REGIROCK_TARGET{
    "regirock", "Regirock", legendary_template("regirock", 30),
    BdspTimelineContext{1, 1, 3.0, 3, 0, 10, false}, 1, true,
};
const BdspRngTargetInfo REGICE_TARGET{
    "regice", "Regice", legendary_template("regice", 30),
    BdspTimelineContext{1, 1, 3.0, 3, 0, 10, false}, 1, true,
};
const BdspRngTargetInfo REGISTEEL_TARGET{
    "registeel", "Registeel", legendary_template("registeel", 30),
    BdspTimelineContext{1, 1, 3.0, 3, 0, 10, false}, 1, true,
};
const BdspRngTargetInfo CRESSELIA_TARGET{
    "cresselia", "Cresselia", cresselia_template(),
    BdspTimelineContext{}, 1, false,
};
const BdspRngTargetInfo TRAINER_ID_TARGET{
    "trainer-id", "Trainer ID / SID", BdspStaticTemplate{},
    BdspTimelineContext{}, 0, false,
};

const BdspRngTargetInfo& target_table(BdspRngTarget target){
    switch (target){
    case BdspRngTarget::Starter:    return STARTER_TARGET;
    case BdspRngTarget::Dialga:     return DIALGA_TARGET;
    case BdspRngTarget::Palkia:     return PALKIA_TARGET;
    case BdspRngTarget::Giratina:   return GIRATINA_TARGET;
    case BdspRngTarget::Regirock:   return REGIROCK_TARGET;
    case BdspRngTarget::Regice:     return REGICE_TARGET;
    case BdspRngTarget::Registeel:  return REGISTEEL_TARGET;
    case BdspRngTarget::Cresselia:  return CRESSELIA_TARGET;
    case BdspRngTarget::TrainerId:  return TRAINER_ID_TARGET;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown RNG target.");
}

} // namespace

const BdspRngTargetInfo& bdsp_rng_target_info(BdspRngTarget target){
    return target_table(target);
}


const EnumDropdownDatabase<BdspRngTarget>& BdspRngTarget_Database(){
    static const EnumDropdownDatabase<BdspRngTarget> database{
        {BdspRngTarget::Starter,    "starter",      "Starter"},
        {BdspRngTarget::Dialga,     "dialga",       "Dialga"},
        {BdspRngTarget::Palkia,     "palkia",       "Palkia"},
        {BdspRngTarget::Giratina,   "giratina",     "Giratina"},
        {BdspRngTarget::Regirock,   "regirock",     "Regirock"},
        {BdspRngTarget::Regice,     "regice",       "Regice"},
        {BdspRngTarget::Registeel,  "registeel",    "Registeel"},
        {BdspRngTarget::Cresselia,  "cresselia",    "Cresselia"},
        {BdspRngTarget::TrainerId,  "trainer-id",   "Trainer ID / SID"},
    };
    return database;
}




} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
