/**
 * @file    engine/cheats_hooks.cpp
 * @brief   The stat-recompute hook the attack and defence multipliers ride on.
 *
 * @license BSD 3-Clause License
 */
#include <rex/hook.h>
#include <rex/ppc.h>
#include <rex/types.h>

#include "core/memory_helpers.h"
#include "engine/cheats.h"

REX_EXTERN(__imp__Player_CalcBattleParams);

// r4 is the destination CharaBattleParams_t, not the chara: the body holds it
// in r23 and every one of its nine stores targets that register, while r3 (the
// chara, held in r29) is only ever read from. Scaling r3's own params block
// instead is wrong wherever the guest was handed a different destination --
// the accessory screen previews an equip by computing into a scratch block,
// and a hook that scaled the live one would multiply it again on every cursor
// move, with nothing to rewrite it in between.
//
// Read before the call: a guest function is free to clobber its argument
// registers, and this one does. Scaling after it returns cannot compound,
// because the call rebuilds every field of the destination first.
//
// The stat bonus rides r3 instead: it is lent to the chara's permanent bonus
// slots for the length of the call, so the guest adds it with its own formula
// and caps, and is taken back before anything can save it.
REX_HOOK_RAW(Player_CalcBattleParams) {
  auto &cheats = bd::engine::Cheats::Get();
  const u32 params = ctx.r4.u32;
  const auto loan = cheats.LendStatBonus(ctx.r3.u32);
  __imp__Player_CalcBattleParams(ctx, base);
  cheats.ReturnStatBonus(loan);
  cheats.ScaleBattleParams(params);
}

REX_EXTERN(__imp__BattleResult_Step);
REX_EXTERN(__imp__Player_AdjustExpAward);
REX_EXTERN(__imp__Player_AdjustSpAward);
REX_EXTERN(__imp__bdAddGold);

// The battle result. Bracketed so the award hooks below only scale what this
// step pays out, and so the result window can be scaled after its copy.
REX_HOOK_RAW(BattleResult_Step) {
  auto &cheats = bd::engine::Cheats::Get();
  const u32 battle = ctx.r3.u32;
  cheats.BeginResultStep();
  __imp__BattleResult_Step(ctx, base);
  cheats.EndResultStep(battle);
}

// (chara, amount) -> the amount after the chara's own bonuses, which the
// result step then hands to Player_AddExp / Player_AddClassSP.
REX_HOOK_RAW(Player_AdjustExpAward) {
  __imp__Player_AdjustExpAward(ctx, base);
  ctx.r3.u64 = bd::engine::Cheats::Get().ScaleExpAward(ctx.r3.u32);
}

REX_HOOK_RAW(Player_AdjustSpAward) {
  __imp__Player_AdjustSpAward(ctx, base);
  ctx.r3.u64 = bd::engine::Cheats::Get().ScaleSpAward(ctx.r3.u32);
}

// (wallet, amount): the battle result's one gold payment. Scaled only while
// the result step runs, so shops, chests and scripts pay what they always did.
REX_HOOK_RAW(bdAddGold) {
  ctx.r4.u64 = bd::engine::Cheats::Get().ScaleGoldAward(ctx.r4.u32);
  __imp__bdAddGold(ctx, base);
}

REX_EXTERN(__imp__bdBattleChargeUpdate);
REX_EXTERN(__imp__bdBattleActionChargeTime);

namespace {

// Battle manager fields the charge step owns.
constexpr u32 kChargeStateOff = 408;  // 4 while the gauge is up
constexpr u32 kChargeRatioOff = 416;  // gauge position, red zone units
constexpr u32 kChargeAmountOff = 420; // ratio * charge time, banked on release
constexpr u32 kZoneCentreOff = 440;   // red zone centre, picked per action
constexpr u32 kChargeCtlOff = 560;    // the gauge the window draws (+396)
constexpr u32 kChargeUserOff = 576;
constexpr u32 kChargeActionOff = 584;
constexpr u32 kCtlRatioOff = 396;
constexpr u32 kStateCharging = 4;

// Puts the gauge on its red zone centre. The release branch judges
// |ratio - centre| < width from the values the last tick left behind, so
// pinning before the call makes even a first-tick tap land, and pinning after
// keeps the drawn gauge there instead of sweeping. Both are recomputed from the
// untouched raw position every held tick, so turning the cheat off mid-charge
// just lets the gauge carry on. The amount is the guest's own ratio * charge
// time, which the original reads from the same call.
void PinChargeToZone(PPCContext &ctx, u8 *base, u32 mgr) {
  using bd::mem::load;
  using bd::mem::store;
  if (load<u32>(mgr + kChargeStateOff) != kStateCharging)
    return;
  const u32 ctl = load<u32>(mgr + kChargeCtlOff);
  const u32 user = load<u32>(mgr + kChargeUserOff);
  const u32 action = load<u32>(mgr + kChargeActionOff);
  if (!ctl || !user || !action)
    return;
  const f32 centre = load<f32>(mgr + kZoneCentreOff);
  ctx.r3.u64 = action;
  ctx.r4.u64 = user;
  __imp__bdBattleActionChargeTime(ctx, base);
  const i32 time = ctx.r3.s32;
  store<f32>(mgr + kChargeRatioOff, centre);
  store<f32>(ctl + kCtlRatioOff, centre);
  store<i32>(mgr + kChargeAmountOff, static_cast<i32>(f32(time) * centre));
}

} // namespace

// (battle manager) -> next scene state. The charge gauge's per-tick step.
REX_HOOK_RAW(bdBattleChargeUpdate) {
  if (!bd::engine::Cheats::Get().PerfectCharge()) {
    __imp__bdBattleChargeUpdate(ctx, base);
    return;
  }
  const u32 mgr = ctx.r3.u32;
  PinChargeToZone(ctx, base, mgr);
  ctx.r3.u64 = mgr;
  __imp__bdBattleChargeUpdate(ctx, base);
  const u64 ret = ctx.r3.u64;
  PinChargeToZone(ctx, base, mgr);
  ctx.r3.u64 = ret;
}
