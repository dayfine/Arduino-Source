/*  Build Commit
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The git commit the app was built from. The build writes its definition
 *  (cmake/BuildCommit.cmake) into BuildCommit.cpp in the build folder, and
 *  rewrites it only when the commit changes, so a new commit recompiles
 *  that one file.
 *
 */

#ifndef PokemonAutomation_BuildCommit_H
#define PokemonAutomation_BuildCommit_H

namespace PokemonAutomation{


//  "git describe" of the source tree: the commit's hash, with "-dirty" when
//  tracked files had changes, or "unknown" when git could not tell.
extern const char* const PROGRAM_COMMIT;


}
#endif
