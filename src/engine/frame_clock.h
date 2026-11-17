/**
 * @file    engine/frame_clock.h
 * @brief   Fixed 30Hz logic tick clock for the fps unlock.
 *
 * @copyright Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *            All rights reserved.
 * @license   BSD 3-Clause License
 *            See LICENSE file in the project root for full license text.
 */
#pragma once

#include <rex/types.h>

namespace bd::engine {

void Advance();

bool InterpolationActive();

bool TickDue();

float Alpha();

u64 TickCount();

double TicksPerSecond();

// Multiplies the logic tick rate (1 = the game's 30Hz). Used by the battle
// speed cheat; capped by the frame rate, since one tick runs per frame at most.
void SetLogicSpeed(double mult);
double LogicSpeed();

double FrameTime();

double FrameDelta();

void PublishRenderClock();
void BindRenderThread();
bool IsRenderThread();

} // namespace bd::engine
