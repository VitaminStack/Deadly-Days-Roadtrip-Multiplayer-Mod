#pragma once
#include <span>
#include "Net/Transport.h"

namespace Sync
{
void TickProgress(std::span<const Net::ControlMessage> Messages);
void TickDesync(std::span<const Net::ControlMessage> Messages);
void TickReward(std::span<const Net::ControlMessage> Messages);
// Ein einziger Queue-Leser. Jede Familie erhaelt dasselbe unveraenderte Paketfenster.
template<class... Families>
void DispatchControls(std::span<const Net::ControlMessage> Messages, Families&&... Family)
{
    (Family(Messages), ...);
}
}
