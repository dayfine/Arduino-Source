/*  Known Failing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon_KnownFailing.h"

namespace PokemonAutomation{
namespace Pokemon{


KnownFailing::KnownFailing(std::map<std::string, std::string> fixes)
    : m_fixes(std::make_move_iterator(fixes.begin()), std::make_move_iterator(fixes.end()))
{}

UnitTestResult KnownFailing::run(Logger& logger, std::string_view name, const std::function<UnitTestResult()>& body) const{
    UnitTestResult result;
    try{
        result = body();
    }catch (Exception& e){
        result = std::format("{}: {}", e.name(), e.message());
    }
    const auto known = m_fixes.find(name);
    if (known == m_fixes.end()){
        return result;
    }
    if (result.result == UnitTestResult::PASSED){
        return std::format("on KNOWN_FAILING ({}) but passes now: take it off the list.", known->second);
    }
    logger.log(std::format("Known failing until {}: {}", known->second, result.message));
    return true;
}

std::vector<std::string> KnownFailing::stale(const std::set<std::string>& names) const{
    std::vector<std::string> out;
    for (const auto& [name, fix] : m_fixes){
        if (!names.contains(name)){
            out.push_back(name);
        }
    }
    return out;
}



} // namespace Pokemon



} // namespace PokemonAutomation
