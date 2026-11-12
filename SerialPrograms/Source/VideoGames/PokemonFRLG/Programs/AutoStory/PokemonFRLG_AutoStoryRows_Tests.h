/*  AutoStory Row Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Registration of the tests that run story rows on the overworld sim,
 *  one file per stretch of the story.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRows_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRows_Tests_H

#include <string_view>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Rows 05.10 and 06.01 on the overworld sim (PokemonFRLG_AutoStoryVermilion_Tests.cpp).
void add_tests_AutoStoryVermilion(UnitTestDatabase& database);
//  Rows 06.02 to 06.04 on the overworld sim (PokemonFRLG_AutoStoryVermilionGym_Tests.cpp).
void add_tests_AutoStoryVermilionGym(UnitTestDatabase& database);
//  Rows from 07.01 on on the overworld sim (PokemonFRLG_AutoStoryCeladon_Tests.cpp),
//  with add_tests_AutoStoryLavender().
void add_tests_AutoStoryCeladon(UnitTestDatabase& database);
//  Rows from 08.01 on on the overworld sim (PokemonFRLG_AutoStoryLavender_Tests.cpp).
void add_tests_AutoStoryLavender(UnitTestDatabase& database);
//  Rows 12.01 and 12.02, Fuchsia Gym, on the overworld sim (PokemonFRLG_AutoStoryFuchsiaGym_Tests.cpp).
void add_tests_AutoStoryFuchsiaGym(UnitTestDatabase& database);

//  Whether checkpoint `id` was authored before the rows that lead up to it:
//  it starts from its own declared pose, not where the row before it in
//  the table ends. 12.01 starts from Fly's landing in Fuchsia, after the
//  Safari Zone (11.02); segments 10 and 11 are authored separately. An id leaves
//  this list once the rows before it are in.
inline bool starts_after_unauthored_rows(std::string_view id){
    return id == "12.01";
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
