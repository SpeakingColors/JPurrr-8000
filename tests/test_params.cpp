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
