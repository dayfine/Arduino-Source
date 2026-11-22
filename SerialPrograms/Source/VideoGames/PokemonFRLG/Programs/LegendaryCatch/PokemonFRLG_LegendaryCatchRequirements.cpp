/*  Legendary Catch Requirements
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cctype>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_LegendaryCatchRequirements.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

namespace{

std::string to_lower_ascii(std::string s){
    for (char& ch : s){
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return s;
}

std::string join_species(const std::vector<std::string>& species){
    std::string out;
    for (size_t i = 0; i < species.size(); i++){
        if (i > 0){
            out += " / ";
        }
        out += species[i];
    }
    return out;
}

void merge_party_req(
    std::vector<PartyMemberRequirement>& dst,
    const PartyMemberRequirement& req
){
    for (PartyMemberRequirement& existing : dst){
        if (existing.accepted_species == req.accepted_species){
            existing.min_level = std::max(existing.min_level, req.min_level);
            existing.min_speed = std::max(existing.min_speed, req.min_speed);
            existing.required = existing.required || req.required;
            return;
        }
    }
    dst.push_back(req);
}

void add_bag_sum(
    std::vector<BagItemRequirement>& dst,
    BagPocket pocket,
    const std::string& slug,
    const std::string& display_name,
    unsigned count
){
    if (count == 0){
        return;
    }
    for (BagItemRequirement& existing : dst){
        if (existing.item_slug == slug){
            existing.min_count += count;
            return;
        }
    }
    dst.push_back({pocket, slug, display_name, count});
}

void set_bag_max(
    std::vector<BagItemRequirement>& dst,
    BagPocket pocket,
    const std::string& slug,
    const std::string& display_name,
    unsigned count
){
    if (count == 0){
        return;
    }
    for (BagItemRequirement& existing : dst){
        if (existing.item_slug == slug){
            existing.min_count = std::max(existing.min_count, count);
            return;
        }
    }
    dst.push_back({pocket, slug, display_name, count});
}

}  // namespace


LegendaryCatchRequirements compute_legendary_catch_requirements(
    const std::vector<LegendaryTarget>& targets,
    unsigned luxury_balls_per_legendary,
    bool include_type_walls
){
    LegendaryCatchRequirements reqs;
    if (targets.empty()){
        return reqs;
    }

    bool need_trapper = false;
    bool need_roamer_smoke_ball = false;

    for (LegendaryTarget target : targets){
        //  Every legendary uses Parasect (Spore + False Swipe).
        unsigned parasect_level = 50;
        unsigned repel_count = 0;

        switch (target){
        case LegendaryTarget::Articuno:
            parasect_level = 55;
            repel_count = 5;
            break;
        case LegendaryTarget::Zapdos:
            parasect_level = 55;
            repel_count = 3;
            if (include_type_walls){
                merge_party_req(reqs.party_members, {
                    {"steelix", "rhydon", "golem"},
                    55, 0, true,
                    "Ground-type wall for Zapdos"
                });
            }
            break;
        case LegendaryTarget::Moltres:
            parasect_level = 55;
            repel_count = 5;
            if (include_type_walls){
                merge_party_req(reqs.party_members, {
                    {"arcanine", "houndoom", "ninetales"},
                    55, 0, true,
                    "Flash Fire wall for Moltres/Ho-oh"
                });
            }
            break;
        case LegendaryTarget::Deoxys:
            parasect_level = 50;
            break;
        case LegendaryTarget::Lugia:
            parasect_level = 70;
            need_trapper = true;
            merge_party_req(reqs.party_members, {
                {"haunter", "gengar"},
                70, 0, true,
                "Taunt / Mean Look trapper"
            });
            break;
        case LegendaryTarget::HoOh:
            parasect_level = 70;
            need_trapper = true;
            merge_party_req(reqs.party_members, {
                {"haunter", "gengar"},
                70, 0, true,
                "Taunt / Mean Look trapper"
            });
            if (include_type_walls){
                merge_party_req(reqs.party_members, {
                    {"arcanine", "houndoom", "ninetales"},
                    70, 0, true,
                    "Flash Fire wall for Moltres/Ho-oh"
                });
            }
            break;
        case LegendaryTarget::Mewtwo:
            parasect_level = 70;
            repel_count = 5;
            need_trapper = true;
            //  Lv. 70 Mewtwo (Base 130 Spe) reaches up to 207 Speed.
            merge_party_req(reqs.party_members, {
                {"haunter", "gengar"},
                90, 208, true,
                "Taunt / Mean Look trapper"
            });
            if (include_type_walls){
                merge_party_req(reqs.party_members, {
                    {"tyranitar", "houndoom"},
                    70, 0, true,
                    "Dark-type wall for Mewtwo"
                });
            }
            break;
        case LegendaryTarget::Raikou:
            parasect_level = 70;
            repel_count = 5;
            need_trapper = true;
            need_roamer_smoke_ball = true;
            //  Lv. 50 Raikou (Base 115 Spe) reaches up to 136 Speed.
            merge_party_req(reqs.party_members, {
                {"haunter", "gengar"},
                70, 137, true,
                "Taunt / Mean Look trapper"
            });
            break;
        case LegendaryTarget::Entei:
            parasect_level = 70;
            repel_count = 5;
            need_trapper = true;
            need_roamer_smoke_ball = true;
            //  Lv. 50 Entei (Base 100 Spe) reaches up to 121 Speed.
            merge_party_req(reqs.party_members, {
                {"haunter", "gengar"},
                70, 122, true,
                "Taunt / Mean Look trapper"
            });
            break;
        case LegendaryTarget::Suicune:
            parasect_level = 70;
            repel_count = 5;
            need_trapper = true;
            need_roamer_smoke_ball = true;
            merge_party_req(reqs.party_members, {
                {"haunter", "gengar"},
                70, 0, true,
                "Taunt / Mean Look trapper"
            });
            break;
        }

        merge_party_req(reqs.party_members, {
            {"parasect"},
            parasect_level, 0, true,
            "Spore + False Swipe catcher"
        });

        add_bag_sum(reqs.bag_items, BagPocket::PokeBalls, "luxury-ball", "Luxury Ball", luxury_balls_per_legendary);
        add_bag_sum(reqs.bag_items, BagPocket::Items, "max-repel", "Max Repel", repel_count);
    }

    set_bag_max(reqs.bag_items, BagPocket::Items, "leftovers", "Leftovers", need_trapper ? 2 : 1);
    if (need_roamer_smoke_ball){
        set_bag_max(reqs.bag_items, BagPocket::Items, "smoke-ball", "Smoke Ball", 1);
    }

    return reqs;
}


std::vector<std::string> check_party_requirements(
    const LegendaryCatchRequirements& requirements,
    const std::vector<ObservedPartyMember>& party
){
    std::vector<std::string> errors;
    for (const PartyMemberRequirement& req : requirements.party_members){
        bool found_species = false;
        bool satisfied = false;
        unsigned best_level = 0;
        unsigned best_speed = 0;

        for (const ObservedPartyMember& member : party){
            std::string species = to_lower_ascii(member.species);
            bool matches = std::find(
                req.accepted_species.begin(), req.accepted_species.end(), species
            ) != req.accepted_species.end();
            if (!matches){
                continue;
            }
            found_species = true;
            best_level = std::max(best_level, member.level);
            best_speed = std::max(best_speed, member.speed);
            if (member.level >= req.min_level && member.speed >= req.min_speed){
                satisfied = true;
                break;
            }
        }

        if (!satisfied){
            std::string names = join_species(req.accepted_species);
            if (!found_species){
                errors.push_back(
                    "Missing party member: " + names + " (" + req.role_description + ")"
                );
            }else{
                std::string msg = names + " does not meet stats (" + req.role_description + "): ";
                if (best_level < req.min_level){
                    msg += "need Lv. " + std::to_string(req.min_level) + "+ (found Lv. " + std::to_string(best_level) + ")";
                }
                if (best_speed < req.min_speed){
                    if (best_level < req.min_level){
                        msg += ", ";
                    }
                    msg += "need Speed " + std::to_string(req.min_speed) + "+ (found " + std::to_string(best_speed) + ")";
                }
                errors.push_back(msg);
            }
        }
    }
    return errors;
}


std::vector<std::string> check_bag_requirements(
    const LegendaryCatchRequirements& requirements,
    const std::map<std::string, unsigned>& bag_counts
){
    std::vector<std::string> errors;
    for (const BagItemRequirement& req : requirements.bag_items){
        auto it = bag_counts.find(req.item_slug);
        unsigned have = (it == bag_counts.end()) ? 0 : it->second;
        if (have < req.min_count){
            errors.push_back(
                "Need at least " + std::to_string(req.min_count) + "x " + req.display_name +
                " in Bag (found " + std::to_string(have) + ")."
            );
        }
    }
    return errors;
}


namespace{

class Test_LegendaryCatchRequirements : public UnitTest{
public:
    Test_LegendaryCatchRequirements()
        : UnitTest("PokemonFRLG::LegendaryCatchRequirements")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        //  1. OR/MAX party + SUM/MAX bag for Articuno + Raikou + Mewtwo
        LegendaryCatchRequirements reqs = compute_legendary_catch_requirements(
            {LegendaryTarget::Articuno, LegendaryTarget::Raikou, LegendaryTarget::Mewtwo},
            50,
            false
        );

        //  Expect Parasect Lv70+, Haunter/Gengar Lv90+ Spe208+
        if (reqs.party_members.size() != 2){
            return "Expected 2 party requirements, got " + std::to_string(reqs.party_members.size());
        }

        std::vector<ObservedPartyMember> good_party = {
            {"parasect", 75, 80},
            {"haunter", 92, 210},
        };
        if (!check_party_requirements(reqs, good_party).empty()){
            return "Expected good_party to satisfy requirements.";
        }

        std::vector<ObservedPartyMember> slow_haunter = {
            {"parasect", 75, 80},
            {"haunter", 90, 190},
        };
        if (check_party_requirements(reqs, slow_haunter).size() != 1){
            return "Expected slow_haunter to fail Speed >= 208 check.";
        }

        //  Expect 150 Luxury Balls (sum), 15 Max Repels (sum), 2 Leftovers (max), 1 Smoke Ball (max)
        std::map<std::string, unsigned> good_bag = {
            {"luxury-ball", 150},
            {"max-repel", 15},
            {"leftovers", 2},
            {"smoke-ball", 1},
        };
        if (!check_bag_requirements(reqs, good_bag).empty()){
            return "Expected good_bag to satisfy requirements.";
        }

        std::map<std::string, unsigned> short_balls = {
            {"luxury-ball", 120},
            {"max-repel", 15},
            {"leftovers", 2},
            {"smoke-ball", 1},
        };
        if (check_bag_requirements(reqs, short_balls).size() != 1){
            return "Expected short_balls to fail Luxury Ball count check.";
        }

        return true;
    }
};

}  // namespace

void add_tests_LegendaryCatchRequirements(UnitTestDatabase& database){
    database.add<Test_LegendaryCatchRequirements>();
}


}
}
}
