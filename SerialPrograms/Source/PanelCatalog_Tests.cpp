/*  Panel Catalog Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/Panels/PanelDescriptor.h"
#include "CommonFramework/Panels/PanelList.h"
#include "CommonFramework/Panels/PanelSession.h"
#include "PanelCatalog.h"
#include "PanelCatalog_Tests.h"

namespace PokemonAutomation{
namespace{

class FakePanelSession final : public PanelSession{
public:
    explicit FakePanelSession(const PanelDescriptor& descriptor)
        : PanelSession(descriptor)
    {}

    virtual void load_json(const JsonValue& json) override{
        const JsonObject* obj = json.to_object();
        if (obj != nullptr){
            if (const std::string* val = obj->get_string("key")){
                loaded_value = *val;
            }
        }
    }

    std::string loaded_value;
};

class FakePanelDescriptor final : public PanelDescriptor{
public:
    FakePanelDescriptor(std::string identifier, std::string display_name)
        : PanelDescriptor(
            std::move(identifier),
            "FakeCategory",
            std::move(display_name),
            "",
            "",
            PanelQualityStanding::DEVELOPER,
            false
        )
    {}

    virtual std::unique_ptr<PanelSession> make_panel() const override{
        return std::make_unique<FakePanelSession>(*this);
    }
};

class FakePanelListDescriptor final : public PanelListDescriptor{
public:
    FakePanelListDescriptor(
        std::string name,
        std::vector<std::pair<std::string, std::string>> entries
    )
        : PanelListDescriptor(std::move(name))
        , m_entries(std::move(entries))
    {}

protected:
    virtual std::vector<PanelEntry> make_panels() const override{
        std::vector<PanelEntry> ret;
        for (const auto& [display_name, id] : m_entries){
            if (id.empty()){
                ret.emplace_back(display_name);
            }else{
                ret.emplace_back(std::make_unique<FakePanelDescriptor>(id, display_name));
            }
        }
        return ret;
    }

private:
    std::vector<std::pair<std::string, std::string>> m_entries;
};

class FakePanelHolder final : public PanelHolder{
public:
    explicit FakePanelHolder(bool accept_intent)
        : m_accept_intent(accept_intent)
    {}

    virtual bool report_new_panel_intent(const PanelDescriptor&) override{
        intent_called = true;
        return m_accept_intent;
    }

    virtual void load_panel(
        std::shared_ptr<const PanelDescriptor> descriptor,
        std::unique_ptr<PanelSession> panel
    ) override{
        loaded_descriptor = std::move(descriptor);
        loaded_panel = std::move(panel);
    }

    virtual void on_busy() override{}
    virtual void on_idle() override{}

    bool intent_called = false;
    std::shared_ptr<const PanelDescriptor> loaded_descriptor;
    std::unique_ptr<PanelSession> loaded_panel;

private:
    bool m_accept_intent;
};

PanelCatalog make_test_catalog(){
    std::vector<std::unique_ptr<PanelListDescriptor>> lists;
    lists.push_back(std::make_unique<FakePanelListDescriptor>(
        "CatA",
        std::vector<std::pair<std::string, std::string>>{
            {"Divider", ""},
            {"Alpha", "CatA:Alpha"},
        }
    ));
    lists.push_back(std::make_unique<FakePanelListDescriptor>(
        "CatB",
        std::vector<std::pair<std::string, std::string>>{
            {"Beta", "CatB:Beta"},
        }
    ));
    return PanelCatalog(std::move(lists));
}

struct CatalogLookupInput{
    std::string_view name;
    std::string_view identifier;
};

struct CatalogLookupExpectation{
    bool found;
    size_t category_index = 0;
    std::string_view display_name = "";
};

struct CatalogLookupCase{
    CatalogLookupInput input;
    CatalogLookupExpectation expectation;
};

const CatalogLookupCase CATALOG_LOOKUP_CASES[] = {
    {.input = {.name = "first category after divider", .identifier = "CatA:Alpha"},
     .expectation = {.found = true, .category_index = 0, .display_name = "Alpha"}},
    {.input = {.name = "second category", .identifier = "CatB:Beta"},
     .expectation = {.found = true, .category_index = 1, .display_name = "Beta"}},
    {.input = {.name = "missing identifier", .identifier = "CatB:Missing"},
     .expectation = {.found = false}},
};

UnitTestResult check_catalog_lookup(const CatalogLookupCase& test){
    const PanelCatalog catalog = make_test_catalog();
    const std::optional<PanelCatalogMatch> match = catalog.find_panel(test.input.identifier);
    if (match.has_value() != test.expectation.found){
        return "Unexpected find_panel() presence.";
    }
    if (!match){
        return true;
    }
    if (match->category_index != test.expectation.category_index ||
        match->display_name != test.expectation.display_name ||
        match->descriptor == nullptr ||
        match->descriptor->identifier() != test.input.identifier)
    {
        return "Catalog match fields did not match expected values.";
    }
    return true;
}

struct LoadSessionInput{
    std::string_view name;
    bool has_descriptor;
    bool accept_intent;
    std::string_view json_value;
};

struct LoadSessionExpectation{
    bool result;
    bool loaded;
    std::string_view loaded_value = "";
};

struct LoadSessionCase{
    LoadSessionInput input;
    LoadSessionExpectation expectation;
};

const LoadSessionCase LOAD_SESSION_CASES[] = {
    {.input = {.name = "loads panel and json", .has_descriptor = true, .accept_intent = true, .json_value = "abc"},
     .expectation = {.result = true, .loaded = true, .loaded_value = "abc"}},
    {.input = {.name = "declined intent skips load", .has_descriptor = true, .accept_intent = false, .json_value = "abc"},
     .expectation = {.result = false, .loaded = false}},
    {.input = {.name = "null descriptor returns false", .has_descriptor = false, .accept_intent = true, .json_value = "abc"},
     .expectation = {.result = false, .loaded = false}},
};

UnitTestResult check_load_session(const LoadSessionCase& test){
    FakePanelHolder holder(test.input.accept_intent);
    std::shared_ptr<const PanelDescriptor> descriptor;
    if (test.input.has_descriptor){
        descriptor = std::make_shared<FakePanelDescriptor>("Fake:Id", "Fake");
    }
    JsonObject obj;
    obj["key"] = std::string(test.input.json_value);
    const bool ok = load_panel_session(holder, descriptor, obj);
    if (ok != test.expectation.result){
        return "Unexpected load_panel_session() return value.";
    }
    const bool is_loaded = holder.loaded_panel != nullptr && holder.loaded_descriptor != nullptr;
    if (is_loaded != test.expectation.loaded){
        return "Unexpected holder load state.";
    }
    if (is_loaded){
        const auto* fake = dynamic_cast<const FakePanelSession*>(holder.loaded_panel.get());
        if (fake == nullptr || fake->loaded_value != test.expectation.loaded_value){
            return "Loaded JSON value did not match.";
        }
    }
    return true;
}

class CatalogLookupTest final : public UnitTest{
public:
    explicit CatalogLookupTest(const CatalogLookupCase& test)
        : UnitTest("PanelCatalog/Lookup/" + std::string(test.input.name))
        , m_test(test)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_catalog_lookup(m_test);
    }

private:
    const CatalogLookupCase& m_test;
};

class LoadSessionTest final : public UnitTest{
public:
    explicit LoadSessionTest(const LoadSessionCase& test)
        : UnitTest("PanelCatalog/LoadSession/" + std::string(test.input.name))
        , m_test(test)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_load_session(m_test);
    }

private:
    const LoadSessionCase& m_test;
};

class DuplicateCategoryTest final : public UnitTest{
public:
    DuplicateCategoryTest()
        : UnitTest("PanelCatalog/DuplicateCategoryThrows")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::vector<std::unique_ptr<PanelListDescriptor>> lists;
        lists.push_back(std::make_unique<FakePanelListDescriptor>("Dup", std::vector<std::pair<std::string, std::string>>{}));
        lists.push_back(std::make_unique<FakePanelListDescriptor>("Dup", std::vector<std::pair<std::string, std::string>>{}));
        try{
            PanelCatalog catalog(std::move(lists));
        }catch (const InternalProgramError&){
            return true;
        }
        return "Expected InternalProgramError on duplicate category.";
    }
};

} // namespace

void add_tests_PanelCatalog(UnitTestDatabase& database){
    for (const CatalogLookupCase& test : CATALOG_LOOKUP_CASES){
        database.add<CatalogLookupTest>(test);
    }
    for (const LoadSessionCase& test : LOAD_SESSION_CASES){
        database.add<LoadSessionTest>(test);
    }
    database.add<DuplicateCategoryTest>();
}

} // namespace PokemonAutomation
