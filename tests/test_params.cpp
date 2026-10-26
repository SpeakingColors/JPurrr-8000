#include "test_util.h"
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
