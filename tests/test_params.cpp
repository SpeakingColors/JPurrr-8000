#include "test_util.h"
#include <string>
#include "jpurrr/params_gen.h"
using namespace jpurrr;
TEST(params_table) {
    CHECK(kNumParams == 285);
    for (int i = 0; i < kNumParams; ++i) {
        CHECK(kParams[i].min <= kParams[i].def && kParams[i].def <= kParams[i].max);
        CHECK(paramIndexById(kParams[i].id) == i);
    }
}
#include "jpurrr/patch.h"
TEST(factory_presets) {
    CHECK(factoryCount() == 51);
    for (int i = 0; i < factoryCount(); ++i) {
        const Patch& p = factoryPatch(i);
        CHECK(!p.name.empty());
        for (int k = 0; k < kNumParams; ++k) CHECK(p.v[k] >= kParams[k].min && p.v[k] <= kParams[k].max);
    }
    CHECK(factoryPatch(0).name == "Anthem Cat");
}

TEST(params_individual_trigger_standard_settings) {
    // JP-8000 manual p.92 (standard settings): Indv Trig Switch OFF, Trig Destination FILTER, Trig Source CH 15, Trig Source Note ALL.
    CHECK(kParams[paramIndexById("its")].def == 0);
    CHECK(kParams[paramIndexById("itd")].def == 0);
    CHECK(kParams[paramIndexById("itc")].def == 14);          // channel 15, the options list is 1..16
    CHECK(kParams[paramIndexById("itn")].def == 128);         // ALL
    CHECK(std::string(kOptionText[kParams[paramIndexById("itc")].optStart + 14]) == "15");
    CHECK(std::string(kOptionText[kParams[paramIndexById("itd")].optStart + 0]) == "FILTER");
    CHECK(std::string(kOptionText[kParams[paramIndexById("itn")].optStart + 128]) == "ALL");
}
