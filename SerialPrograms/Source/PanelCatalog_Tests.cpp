/*  Panel Catalog Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/Panels/PanelDescriptor.h"
#include "CommonFramework/Panels/PanelList.h"
#include "CommonFramework/Panels/PanelSession.h"
#include "PanelCatalog.h"
#include "PanelCatalog_Tests.h"

namespace PokemonAutomation{
namespace{


class FakePanelDescriptor final : public PanelDescriptor{
public:
    explicit FakePanelDescriptor(std::string identifier)
        : PanelDescriptor(
            identifier,
            /*category=*/"FakeCategory",
            /*display_name=*/identifier,
            /*doc_link=*/"",
            /*description=*/"",
            PanelQualityStanding::DEVELOPER,
            /*restore_defaults_button=*/false
        )
    {}

    virtual std::unique_ptr<PanelSession> make_panel() const override{
        return std::make_unique<PanelSession>(*this);
    }
};

//  One category: a divider, then the panel "Fake:Alpha".
class FakePanelList final : public PanelListDescriptor{
public:
    FakePanelList()
        : PanelListDescriptor("FakeList")
    {}

protected:
    virtual std::vector<PanelEntry> make_panels() const override{
        std::vector<PanelEntry> ret;
        ret.emplace_back("Divider");
        ret.emplace_back(std::make_unique<FakePanelDescriptor>("Fake:Alpha"));
        return ret;
    }
};

class DecliningPanelHolder final : public PanelHolder{
public:
    virtual bool report_new_panel_intent(const PanelDescriptor&) override{
        return false;
    }
    virtual void load_panel(
        std::shared_ptr<const PanelDescriptor>,
        std::unique_ptr<PanelSession>
    ) override{
        loaded = true;
    }
    virtual void on_busy() override{}
    virtual void on_idle() override{}

    bool loaded = false;
};


class FindPanelPastDividerTest final : public UnitTest{
public:
    FindPanelPastDividerTest()
        : UnitTest("PanelCatalog/FindPanel/past a divider")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::vector<std::unique_ptr<PanelListDescriptor>> lists;
        lists.emplace_back(std::make_unique<FakePanelList>());

        const std::optional<PanelMatch> match = find_panel(lists, "Fake:Alpha");
        if (!match){
            return "find_panel() did not find Fake:Alpha.";
        }
        if (match->category_index != 0){
            return "find_panel() returned the wrong category index.";
        }
        if (match->entry.descriptor == nullptr || match->entry.descriptor->identifier() != "Fake:Alpha"){
            return "find_panel() returned the wrong panel.";
        }
        return true;
    }
};

class LoadPanelSessionDeclinedTest final : public UnitTest{
public:
    LoadPanelSessionDeclinedTest()
        : UnitTest("PanelCatalog/LoadPanelSession/declined intent skips load")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        DecliningPanelHolder holder;
        const std::shared_ptr<const PanelDescriptor> descriptor =
            std::make_shared<FakePanelDescriptor>("Fake:Alpha");

        if (load_panel_session(descriptor, JsonValue(), holder)){
            return "load_panel_session() returned true after the holder declined.";
        }
        if (holder.loaded){
            return "load_panel_session() loaded a panel after the holder declined.";
        }
        return true;
    }
};


} // namespace

void add_tests_PanelCatalog(UnitTestDatabase& database){
    database.add<FindPanelPastDividerTest>();
    database.add<LoadPanelSessionDeclinedTest>();
}


} // namespace PokemonAutomation
