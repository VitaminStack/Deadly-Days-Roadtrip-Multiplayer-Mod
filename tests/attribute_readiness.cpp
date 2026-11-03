#include "../research/spikes/a-connect/AttributeReadiness.h"
#include <cassert>
#include <limits>

int main()
{
    using namespace Spike::AttributeReadiness;
    uintptr_t Characters[]{1, 2, 0};
    bool Ready[]{false, true, false};
    const auto At = [&](int32_t i) { return Characters[i]; };
    const auto Has = [&](uintptr_t Character) { return Ready[Character]; };
    assert(!Complete(3, 3, At, Has)); // No partial aggregate when only P1 is ready.
    Ready[2] = true;
    assert(Complete(3, 3, At, Has)); // Same roster resumes when replication finishes.
    assert(Complete(0, 0, At, Has));
    assert(!Complete(-1, 0, At, Has));
    assert(!Complete(3, 2, At, Has));
    assert(!Complete(4097, 4097, At, Has));
    assert(PendingValue(-1.0f) == 0.0f);
    assert(PendingValue(1.5f) == 1.5f);
    assert(PendingValue(std::numeric_limits<float>::quiet_NaN()) == 0.0f);
    assert(PendingValue(std::numeric_limits<float>::infinity()) == 0.0f);
}
