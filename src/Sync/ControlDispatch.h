#pragma once
#include <span>
#include "Net/Transport.h"

namespace Sync
{
void TickProgress(std::span<const Net::ControlMessage> Messages);
void TickDesync(std::span<const Net::ControlMessage> Messages);
// Ein einziger Queue-Leser. Jede Familie erhaelt dasselbe unveraenderte Paketfenster.
template<class ProgressFn, class DesyncFn>
void DispatchControls(std::span<const Net::ControlMessage> Messages, ProgressFn&& Progress, DesyncFn&& Desync)
{
    Progress(Messages);
    Desync(Messages);
}
}
