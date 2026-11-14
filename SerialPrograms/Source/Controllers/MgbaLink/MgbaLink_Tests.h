/*  MgbaLink Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Controllers_MgbaLink_Tests_H
#define PokemonAutomation_Controllers_MgbaLink_Tests_H

#include "Common/Cpp/Concurrency/ThreadPool.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace MgbaLink{


//  Reply parsing, line framing, the frame clock, and a connection against a
//  fake bridge on 127.0.0.1 (sockets run on "thread_pool").
void add_tests(UnitTestDatabase& database, ThreadPool& thread_pool);


}
}
#endif
