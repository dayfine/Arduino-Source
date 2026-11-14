#ifndef PokemonAutomation_PokemonSV_IsolatedHuntSandwichOption_H
#define PokemonAutomation_PokemonSV_IsolatedHuntSandwichOption_H

#include "PokemonSV_SandwichMakerOption.h"
#include "VideoGames/PokemonSV/Programs/ShinyHunting/PokemonSV_IsolatedHuntPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

class IsolatedHuntSandwichOption : public SandwichMakerOption{
public:
    IsolatedHuntSandwichOption(OCR::LanguageOCROption* language);
    virtual void load_json(const JsonValue& json) override;
    void configure_for(IsolatedEncounterType type);

    BooleanCheckBoxOption USE_HERBA;
};

}
}
}
#endif
