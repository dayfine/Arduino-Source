#include <iostream>
#include <QCoreApplication>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "VideoGames/PokemonSV/PokemonSV_Tests.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_PlaceNameBannerReader.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_TerariumPlazaReturn.h"
namespace PokemonAutomation{bool USE_QT_UI=false;}

class NullLogger : public PokemonAutomation::Logger{
public:
    virtual void log(const std::string&, PokemonAutomation::Color) override{}
};
class StdLogger : public PokemonAutomation::Logger{
public:
    virtual void log(const std::string& s, PokemonAutomation::Color) override{std::cout<<s<<'\n';}
};
int main(int argc, char** argv){
    QCoreApplication app(argc, argv);
    using namespace PokemonAutomation;
    if (argc >= 3 && std::string(argv[1]) == "--banner"){
        StdLogger logger;
        NintendoSwitch::PokemonSV::PlaceNameBannerReader reader;
        for (int i = 2; i < argc; i++){
            ImageRGB32 image(argv[i]);
            std::cout << argv[i] << '\n';
            for (const std::string& t : reader.read(logger, Language::ChineseTraditional, image)){
                std::cout << "  match=" << NintendoSwitch::PokemonSV::place_name_banner_matches("中心廣場", t) << '\n';
            }
        }
        return 0;
    }
    UnitTestDatabase database;
    NintendoSwitch::PokemonSV::add_tests(database);
    NullLogger logger;
    CancellableHolder<CancellableScope> scope;
    size_t passed=0,failed=0;
    for(const auto& [name,test]:database){
        if(name.find("PokemonSV::BBQ")!=0 && name.find("PokemonSV::TeraEncounterAction")!=0 && name.find("PokemonSV::BlueberryPoints")!=0)continue;
        try{
            const auto result=test->run(logger,scope);
            if(result.result==UnitTestResult::PASSED){passed++;std::cout<<"PASS "<<name<<'\n';}
            else{failed++;std::cerr<<"FAIL "<<name<<": "<<result.message<<'\n';}
        }catch(const std::exception& e){failed++;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}
    }
    std::cout<<passed<<" passed, "<<failed<<" failed\n";
    return failed||passed==0;
}
