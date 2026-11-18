/**
 * @file    engine/cheats.cpp
 * @license BSD 3-Clause, see LICENSE
 */
#include "engine/cheats.h"

#include <algorithm>
#include <cstddef>
#include <vector>
#include <iterator>
#include <string_view>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/ppc/stack.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "core/settings.h" // kCvarGroup, FormatCvar
#include "engine/battle_task.h"
#include "engine/chara.h"
#include "engine/events.h"
#include "engine/field_player_entity.h"
#include "engine/frame_clock.h"
#include "engine/game.h"
#include "engine/item_save_data.h"
#include "engine/achievements/achievements.h"
#include "engine/game_tables.h"

// The guest's own award-EXP entry point, at 0x823B3CD0. It adds to the running
// total at chara+0x1B04 and then walks its own level-up loop until the surplus
// is spent, refusing to pass the max level table[0] names. Handing it the bonus
// beats storing the total ourselves: the levels land on the battle that earned
// them instead of the one after, which is what a bare write leaves behind.
REX_IMPORT(__imp__Player_AddExp, GuestAddExp, u32(u32, u32));

// 0x823B3DD8: the cumulative EXP a chara needs to reach a level, with its
// per-character adjustment applied. Answers 0 for a level below 1 or past the
// max the level table names. Player_AddExp's own level-up loop asks it.
REX_IMPORT(__imp__Player_ExpForLevel, GuestExpForLevel, u32(u32, u32));

// The class counterparts. 0x823B4528 (chara, class, sp) adds SP to one class
// record and walks its own rank-up loop, applying each rank's stats, until the
// SP is spent or the max rank is reached. 0x823B4600 (class, rank) answers the
// cumulative SP that rank needs, 0 past the max.
REX_IMPORT(__imp__Player_AddClassSP, GuestAddClassSP, u32(u32, u32, u32));
REX_IMPORT(__imp__Class_SPForRank, GuestSPForRank, u32(u32, u32));

REXCVAR_DECLARE(bool, bd_cheat_diag);
REXCVAR_DECLARE(bool, bd_cheat_invincible);
REXCVAR_DECLARE(bool, bd_cheat_infinite_mp);
REXCVAR_DECLARE(bool, bd_cheat_infinite_gold);
REXCVAR_DECLARE(bool, bd_cheat_status_immune);
REXCVAR_DECLARE(bool, bd_cheat_unlock_classes);
REXCVAR_DECLARE(bool, bd_cheat_one_hit_kill);
REXCVAR_DECLARE(i32, bd_cheat_battle_speed);
REXCVAR_DECLARE(i32, bd_cheat_level_up);
REXCVAR_DECLARE(i32, bd_cheat_class_rank_up);
REXCVAR_DECLARE(i32, bd_cheat_attack_mult);
REXCVAR_DECLARE(i32, bd_cheat_magic_attack_mult);
REXCVAR_DECLARE(i32, bd_cheat_magic_defence_mult);
REXCVAR_DECLARE(i32, bd_cheat_agility_mult);
REXCVAR_DECLARE(i32, bd_cheat_medals_mult);
REXCVAR_DECLARE(bool, bd_cheat_infinite_medals);
REXCVAR_DECLARE(bool, bd_cheat_infinite_items);
REXCVAR_DECLARE(bool, bd_cheat_give_all_items);
REXCVAR_DECLARE(bool, bd_cheat_unlock_achievements);
REXCVAR_DECLARE(bool, bd_cheat_reset_achievements);
REXCVAR_DECLARE(bool, bd_cheat_give_heal);
REXCVAR_DECLARE(bool, bd_cheat_give_usable);
REXCVAR_DECLARE(bool, bd_cheat_give_spellbook);
REXCVAR_DECLARE(bool, bd_cheat_give_arm);
REXCVAR_DECLARE(bool, bd_cheat_give_finger);
REXCVAR_DECLARE(bool, bd_cheat_give_ear);
REXCVAR_DECLARE(bool, bd_cheat_give_neck);
REXCVAR_DECLARE(bool, bd_cheat_give_chest);
REXCVAR_DECLARE(bool, bd_cheat_give_valuable);

REXCVAR_DECLARE(i32, bd_cheat_defence_mult);
REXCVAR_DECLARE(i32, bd_cheat_stat_bonus);
REXCVAR_DECLARE(i32, bd_cheat_exp_mult);
REXCVAR_DECLARE(i32, bd_cheat_sp_mult);
REXCVAR_DECLARE(i32, bd_cheat_gold_mult);

REXCVAR_DEFINE_BOOL(bd_cheat_diag, false, kCvarGroup,
                    "Log a line whenever a cheat acts, naming what it changed. "
                    "For checking a cheat fires; off by default.");

REXCVAR_DEFINE_BOOL(bd_cheat_invincible, false, kCvarGroup,
                    "Refill every living party member's HP each logic step. A "
                    "member already knocked out stays down.");

REXCVAR_DEFINE_BOOL(bd_cheat_infinite_mp, false, kCvarGroup,
                    "Refill every living party member's MP each logic step.");

REXCVAR_DEFINE_BOOL(bd_cheat_infinite_gold, false, kCvarGroup,
                    "Hold gold at the 99,999,999 ceiling the game clamps to.");

REXCVAR_DEFINE_BOOL(bd_cheat_status_immune, false, kCvarGroup,
                    "Hold every status resistance at the immune threshold and "
                    "clear any ailment already applied.");

REXCVAR_DEFINE_BOOL(bd_cheat_unlock_classes, false, kCvarGroup,
                    "Keep all nine classes unlocked on every roster member.");

REXCVAR_DEFINE_BOOL(bd_cheat_one_hit_kill, false, kCvarGroup,
                    "Hold every living enemy at 1 HP while a battle is up, so "
                    "the next hit that lands kills it.");

REXCVAR_DEFINE_INT32(bd_cheat_battle_speed, 1, kCvarGroup,
                     "Run game logic this many times faster while a battle is "
                     "up (1-4). One logic tick runs per rendered frame at most, "
                     "so Nx needs at least 30*N fps. 1 leaves it alone.");

REXCVAR_DEFINE_INT32(bd_cheat_level_up, 1, kCvarGroup,
                     "How many levels the Raise Party Level action adds to "
                     "every roster member (999 = max level). Choosing it does "
                     "nothing by itself; the confirmed action row raises.");

REXCVAR_DEFINE_INT32(bd_cheat_class_rank_up, 1, kCvarGroup,
                     "How many ranks the Raise Class Ranks action adds to every "
                     "unlocked class of every roster member (999 = max rank). "
                     "Choosing it does nothing by itself.");

REXCVAR_DEFINE_INT32(bd_cheat_attack_mult, 1, kCvarGroup,
                     "Multiply physical attack as the guest finishes each stat "
                     "recompute. 1 leaves it alone.");

REXCVAR_DEFINE_INT32(bd_cheat_magic_attack_mult, 1, kCvarGroup,
                     "Multiply magic attack as the guest finishes each stat "
                     "recompute. 1 leaves it alone.");

REXCVAR_DEFINE_INT32(bd_cheat_defence_mult, 1, kCvarGroup,
                     "Multiply physical defense as the guest finishes each "
                     "stat recompute. 1 leaves it alone.");

REXCVAR_DEFINE_INT32(bd_cheat_magic_defence_mult, 1, kCvarGroup,
                     "Multiply magic defense as the guest finishes each stat "
                     "recompute. 1 leaves it alone.");

REXCVAR_DEFINE_INT32(bd_cheat_agility_mult, 1, kCvarGroup,
                     "Multiply agility, which drives turn order, as the guest "
                     "finishes each stat recompute. 1 leaves it alone.");

REXCVAR_DEFINE_INT32(bd_cheat_stat_bonus, 0, kCvarGroup,
                     "Bonus applied to max HP/MP, attack, magic attack, "
                     "defense, magic defense and agility at each stat "
                     "recompute, through the game's own formula and caps. "
                     "Never written to the save. 0 is off.");

REXCVAR_DEFINE_INT32(bd_cheat_exp_mult, 1, kCvarGroup,
                     "Multiply the EXP a battle awards. 1 leaves it alone.");

REXCVAR_DEFINE_INT32(bd_cheat_sp_mult, 1, kCvarGroup,
                     "Multiply the class SP a battle awards. 1 leaves it "
                     "alone.");

REXCVAR_DEFINE_INT32(bd_cheat_gold_mult, 1, kCvarGroup,
                     "Multiply the gold a battle awards. 1 leaves it alone.");

REXCVAR_DEFINE_INT32(bd_cheat_medals_mult, 1, kCvarGroup,
                     "Multiply the ancient medals a battle awards. 1 leaves "
                     "them alone.");

REXCVAR_DEFINE_BOOL(bd_cheat_infinite_medals, false, kCvarGroup,
                    "Hold ancient medals at the 9999 ceiling the game clamps "
                    "to.");

REXCVAR_DEFINE_BOOL(bd_cheat_infinite_items, false, kCvarGroup,
                    "Hold every occupied inventory slot at a full stack, so "
                    "nothing is spent by using or selling it. Key items "
                    "(Valuables) are left alone.");

REXCVAR_DEFINE_BOOL(bd_cheat_give_all_items, false, kCvarGroup,
                    "Fill the inventory with one full stack of every item the "
                    "record table knows. Runs once, then clears itself.");

REXCVAR_DEFINE_BOOL(bd_cheat_unlock_achievements, false, kCvarGroup,
                    "Award every re:Blue achievement through the same path a "
                    "condition would. Runs once, then clears itself.");

REXCVAR_DEFINE_BOOL(bd_cheat_reset_achievements, false, kCvarGroup,
                    "Re-lock every achievement by clearing the profile's "
                    "unlock store. Runs once, then clears itself.");

REXCVAR_DEFINE_BOOL(bd_cheat_give_heal, false, kCvarGroup,
                    "Grant one of every heal record. Runs once, then clears "
                    "itself.");
REXCVAR_DEFINE_BOOL(bd_cheat_give_usable, false, kCvarGroup,
                    "Grant one of every usable record. Runs once, then clears "
                    "itself.");
REXCVAR_DEFINE_BOOL(bd_cheat_give_spellbook, false, kCvarGroup,
                    "Grant one of every spellbook record. Runs once, then clears "
                    "itself.");
REXCVAR_DEFINE_BOOL(bd_cheat_give_arm, false, kCvarGroup,
                    "Grant one of every arm record. Runs once, then clears "
                    "itself.");
REXCVAR_DEFINE_BOOL(bd_cheat_give_finger, false, kCvarGroup,
                    "Grant one of every finger record. Runs once, then clears "
                    "itself.");
REXCVAR_DEFINE_BOOL(bd_cheat_give_ear, false, kCvarGroup,
                    "Grant one of every ear record. Runs once, then clears "
                    "itself.");
REXCVAR_DEFINE_BOOL(bd_cheat_give_neck, false, kCvarGroup,
                    "Grant one of every neck record. Runs once, then clears "
                    "itself.");
REXCVAR_DEFINE_BOOL(bd_cheat_give_chest, false, kCvarGroup,
                    "Grant one of every chest record. Runs once, then clears "
                    "itself.");
REXCVAR_DEFINE_BOOL(bd_cheat_give_valuable, false, kCvarGroup,
                    "Grant one of every valuable record. Runs once, then clears "
                    "itself.");

namespace bd::engine {
namespace {

// What ItemSaveData::SetGold clamps to, so pinning here writes what the guest
// would have stored rather than a value it has to correct.
constexpr u32 kGoldMax = 99999999u;

// What ItemSaveData::SetAt clamps a stack to, so pinning here writes a count
// the guest would have stored itself.
constexpr u32 kItemStackMax = 99u;

// Ceiling on the id sweep for "give all". Well past the ids the shipped item
// table uses, and the walk stops early once the inventory is full anyway.
constexpr u32 kMaxItemId = 4095u;

// Logic steps a fired grant stays lit before disarming: about half a second at
// the fixed 30Hz simulation rate. Long enough to see, short enough that the
// row is plainly a button rather than a setting.
constexpr u32 kGrantLitSteps = 15;

// Each category grant is its own cvar so the menu has a row to bind to.
struct GrantEntry {
  ItemCategory cat;
  const char *cvar;
};
constexpr GrantEntry kGrantEntries[] = {
    {ItemCategory::kHeal, "bd_cheat_give_heal"},
    {ItemCategory::kUsable, "bd_cheat_give_usable"},
    {ItemCategory::kSpellbook, "bd_cheat_give_spellbook"},
    {ItemCategory::kArm, "bd_cheat_give_arm"},
    {ItemCategory::kFinger, "bd_cheat_give_finger"},
    {ItemCategory::kEar, "bd_cheat_give_ear"},
    {ItemCategory::kNeck, "bd_cheat_give_neck"},
    {ItemCategory::kChest, "bd_cheat_give_chest"},
    {ItemCategory::kValuable, "bd_cheat_give_valuable"},
};

// EXP and class SP are read back as signed by the screens that print them, so
// a top-up allowed to run to the u32 ceiling shows up as a negative number.
// This sits far above anything a playthrough reaches and an order of magnitude
// below where the sign bit turns over.
constexpr u32 kProgressMax = 99999999u;

// Every class bit set.
constexpr u32 kAllClasses = (1u << kCharaClassCount) - 1u;

// Steps the reward award keeps retrying after the battle-end edge, at the
// fixed 30Hz simulation rate. Two seconds covers a guest that banks its
// rewards after the edge without reaching far enough to catch a chest opened
// on the way out.
constexpr u32 kSettleSteps = 60;

// Multipliers are offered as a short menu, but a console write can put
// anything in the cvar, so both ends are held to something survivable.
constexpr i32 kMultMin = 1;
constexpr i32 kMultMax = 100;
constexpr i32 kStatBonusMax = 99999;
constexpr i32 kBattleSpeedMax = 4;

// The level table the guest's EXP functions read: its first word is the max
// level. Both Player_AddExp and Player_ExpForLevel load it from here.
constexpr u32 kLevelTablePtrEA = 0x82DC9B44;

// The max class rank, as a plain word: Player_AddClassSP and Class_SPForRank
// both compare against it.
constexpr u32 kMaxClassRankEA = 0x8276FCB4;

// The five derived stats of the battle-params block this file scales, at the
// offsets Player_CalcBattleParams writes them into it. chara.cpp carries the
// full layout; only these fields are named here so offsetof stays honest
// without a second copy of it.
struct ScaledStats_t {
  /* 0x00 */ u8 _pad00[0x38];
  /* 0x38 */ be_u32 attack;
  /* 0x3C */ be_u32 defense;
  /* 0x40 */ u8 _pad40[0x08];
  /* 0x48 */ be_u32 magicAttack;
  /* 0x4C */ be_u32 magicDefense;
  /* 0x50 */ u8 _pad50[0x04];
  /* 0x54 */ be_u32 agility;
};
static_assert(offsetof(ScaledStats_t, attack) == 0x38);
static_assert(offsetof(ScaledStats_t, defense) == 0x3C);
static_assert(offsetof(ScaledStats_t, magicAttack) == 0x48);
static_assert(offsetof(ScaledStats_t, magicDefense) == 0x4C);
static_assert(offsetof(ScaledStats_t, agility) == 0x54);

// A derived stat, at its offset inside the params block the guest just wrote.
// Reads back what it computed and stores the product.
//
// The ceiling is far above any value the game produces and far below the point
// a u32 wraps, so a stat the menus print as signed can never come out negative
// even if this is handed a block it did not expect.
constexpr u32 kStatCeiling = 999999u;

bool ScaleStat(u32 paramsEA, size_t field, i32 mult) {
  const u32 va = paramsEA + static_cast<u32>(field);
  const u32 cur = mem::try_load<u32>(va);
  if (cur == 0)
    return false;
  const u64 scaled = static_cast<u64>(cur) * static_cast<u64>(mult);
  return mem::try_store<u32>(
      va, static_cast<u32>(std::min<u64>(scaled, u64(kStatCeiling))));
}

// now + (mult-1)*(now-before), saturated. The guest has already paid the
// base award, so only the remainder is owed.
u32 TopUp(u32 before, u32 now, i32 mult, u32 ceiling) {
  if (mult <= 1 || now <= before)
    return now;
  const u64 extra =
      static_cast<u64>(now - before) * static_cast<u64>(mult - 1);
  return static_cast<u32>(std::min<u64>(static_cast<u64>(now) + extra,
                                        static_cast<u64>(ceiling)));
}

} // namespace

bool CheatDiagEnabled() { return REXCVAR_GET(bd_cheat_diag); }

Cheats &Cheats::Get() {
  static Cheats c;
  return c;
}

// ---- cvar adoption ----

void Cheats::AdoptInvincible() {
  invincible_ = REXCVAR_GET(bd_cheat_invincible);
  toldInvincible_ = false;
}
void Cheats::AdoptInfiniteMP() {
  infiniteMP_ = REXCVAR_GET(bd_cheat_infinite_mp);
  toldInfiniteMP_ = false;
}
void Cheats::AdoptInfiniteGold() {
  infiniteGold_ = REXCVAR_GET(bd_cheat_infinite_gold);
  toldInfiniteGold_ = false;
}
void Cheats::AdoptStatusImmune() {
  statusImmune_ = REXCVAR_GET(bd_cheat_status_immune);
  toldStatusImmune_ = false;
}
void Cheats::AdoptUnlockClasses() {
  unlockClasses_ = REXCVAR_GET(bd_cheat_unlock_classes);
  toldUnlockClasses_ = false;
}
void Cheats::AdoptOneHitKill() {
  oneHitKill_ = REXCVAR_GET(bd_cheat_one_hit_kill);
}
void Cheats::AdoptBattleSpeed() {
  battleSpeed_ =
      std::clamp(REXCVAR_GET(bd_cheat_battle_speed), kMultMin, kBattleSpeedMax);
}
void Cheats::AdoptLevelUp() {
  levelUp_ = std::clamp(REXCVAR_GET(bd_cheat_level_up), 1, kLevelUpMax);
}
bool Cheats::SetLevelUp(i32 v) {
  return rex::cvar::SetFlagByName("bd_cheat_level_up", FormatCvar(v));
}
void Cheats::AdoptClassRankUp() {
  classRankUp_ = std::clamp(REXCVAR_GET(bd_cheat_class_rank_up), 1, kLevelUpMax);
}
bool Cheats::SetClassRankUp(i32 v) {
  return rex::cvar::SetFlagByName("bd_cheat_class_rank_up", FormatCvar(v));
}
bool Cheats::Pickable(u32 id) {
  // The same test the category grants use: a record exists and the sheet
  // classifies it, which leaves out the unused "Worthless Junk" entries.
  return id >= 1 && id <= kItemCategoryMaxId &&
         CategoryOf(id) != ItemCategory::kNone &&
         GameTables::Get().PhenomeRecord(id) != 0;
}
void Cheats::QueueGive(u32 itemId, u32 count) {
  if (!Pickable(itemId) || count == 0)
    return;
  std::lock_guard lock(giveMutex_);
  pendingGives_.emplace_back(itemId, count);
}
void Cheats::AdoptAttackMult() {
  attackMult_ = std::clamp(REXCVAR_GET(bd_cheat_attack_mult), kMultMin, kMultMax);
}
void Cheats::AdoptMagicAttackMult() {
  magicAttackMult_ =
      std::clamp(REXCVAR_GET(bd_cheat_magic_attack_mult), kMultMin, kMultMax);
}
void Cheats::AdoptDefenceMult() {
  defenceMult_ =
      std::clamp(REXCVAR_GET(bd_cheat_defence_mult), kMultMin, kMultMax);
}
void Cheats::AdoptMagicDefenceMult() {
  magicDefenceMult_ =
      std::clamp(REXCVAR_GET(bd_cheat_magic_defence_mult), kMultMin, kMultMax);
}
void Cheats::AdoptAgilityMult() {
  agilityMult_ =
      std::clamp(REXCVAR_GET(bd_cheat_agility_mult), kMultMin, kMultMax);
}
void Cheats::AdoptStatBonus() {
  statBonus_ = std::clamp(REXCVAR_GET(bd_cheat_stat_bonus), 0, kStatBonusMax);
  toldStatBonus_ = false;
}
void Cheats::AdoptExpMult() {
  expMult_ = std::clamp(REXCVAR_GET(bd_cheat_exp_mult), kMultMin, kMultMax);
}
void Cheats::AdoptSpMult() {
  spMult_ = std::clamp(REXCVAR_GET(bd_cheat_sp_mult), kMultMin, kMultMax);
}
void Cheats::AdoptMedalsMult() {
  medalsMult_ =
      std::clamp(REXCVAR_GET(bd_cheat_medals_mult), kMultMin, kMultMax);
}
void Cheats::AdoptInfiniteMedals() {
  infiniteMedals_ = REXCVAR_GET(bd_cheat_infinite_medals);
  toldInfiniteMedals_ = false;
}
void Cheats::AdoptInfiniteItems() {
  infiniteItems_ = REXCVAR_GET(bd_cheat_infinite_items);
  toldInfiniteItems_ = false;
}
void Cheats::AdoptGiveAllItems() {
  giveAllItems_ = REXCVAR_GET(bd_cheat_give_all_items);
  // Mirrors the setting rather than only arming on true, so turning it off --
  // by hand or from ResetAll -- cancels a queued fill. A lit bit means the
  // fill is already queued or running and its cvar stays on until disarm, so
  // re-mirroring must not queue it again. Armed here, run from Apply: the
  // setting can change at the title, long before there is any save data to
  // write into.
  pendingGiveAll_ = giveAllItems_ && !(litGrants_ & (1u << 31));
}
void Cheats::AdoptGrants() {
  // Mirrors the settings rather than only arming on true, so turning a grant
  // off -- by hand or from ResetAll -- cancels its queued fill. A lit bit
  // means that category already has a fill queued or running and its cvar
  // stays on until disarm, so re-mirroring it must not queue the fill again:
  // pressing two rows quickly would otherwise fire the first one twice. Armed
  // here, run from Apply, for the same reason GiveAllItems is: the row can be
  // set at the title with no save data to write into yet.
  u32 pending = 0;
  for (u32 i = 0; i < std::size(kGrantEntries); ++i) {
    if (rex::cvar::GetFlagByName(kGrantEntries[i].cvar) == "true")
      pending |= (1u << i);
  }
  pendingGrants_ = pending & ~litGrants_;
}

bool Cheats::Grant(ItemCategory c) const {
  for (const auto &e : kGrantEntries)
    if (e.cat == c)
      return rex::cvar::GetFlagByName(e.cvar) == "true";
  return false;
}

bool Cheats::SetGrant(ItemCategory c, bool v) {
  for (const auto &e : kGrantEntries)
    if (e.cat == c)
      return rex::cvar::SetFlagByName(e.cvar, FormatCvar(v));
  return false;
}

void Cheats::AdoptUnlockAchievements() {
  unlockAchievements_ = REXCVAR_GET(bd_cheat_unlock_achievements);
  // Mirrors the setting so turning it off cancels a queued award; a lit bit
  // means the award is already queued or running and its cvar stays on until
  // disarm, so re-mirroring must not queue it again.
  pendingAchievements_ = unlockAchievements_ && !(litGrants_ & (1u << 30));
}

bool Cheats::SetUnlockAchievements(bool v) {
  return rex::cvar::SetFlagByName("bd_cheat_unlock_achievements",
                                  FormatCvar(v));
}

void Cheats::AdoptResetAchievements() {
  resetAchievements_ = REXCVAR_GET(bd_cheat_reset_achievements);
  // Mirrors the setting so turning it off cancels a queued wipe; a lit bit
  // means the wipe is already queued or running and its cvar stays on until
  // disarm, so re-mirroring must not queue it again.
  pendingResetAchv_ = resetAchievements_ && !(litGrants_ & (1u << 29));
}

bool Cheats::SetResetAchievements(bool v) {
  return rex::cvar::SetFlagByName("bd_cheat_reset_achievements", FormatCvar(v));
}

void Cheats::AdoptGoldMult() {
  goldMult_ = std::clamp(REXCVAR_GET(bd_cheat_gold_mult), kMultMin, kMultMax);
}

bool Cheats::SetInvincible(bool v) {
  return rex::cvar::SetFlagByName("bd_cheat_invincible", FormatCvar(v));
}
bool Cheats::SetInfiniteMP(bool v) {
  return rex::cvar::SetFlagByName("bd_cheat_infinite_mp", FormatCvar(v));
}
bool Cheats::SetInfiniteGold(bool v) {
  return rex::cvar::SetFlagByName("bd_cheat_infinite_gold", FormatCvar(v));
}
bool Cheats::SetStatusImmune(bool v) {
  return rex::cvar::SetFlagByName("bd_cheat_status_immune", FormatCvar(v));
}
bool Cheats::SetUnlockClasses(bool v) {
  return rex::cvar::SetFlagByName("bd_cheat_unlock_classes", FormatCvar(v));
}
bool Cheats::SetOneHitKill(bool v) {
  return rex::cvar::SetFlagByName("bd_cheat_one_hit_kill", FormatCvar(v));
}
bool Cheats::SetBattleSpeed(i32 v) {
  return rex::cvar::SetFlagByName("bd_cheat_battle_speed", FormatCvar(v));
}
bool Cheats::SetAttackMult(i32 v) {
  return rex::cvar::SetFlagByName("bd_cheat_attack_mult", FormatCvar(v));
}
bool Cheats::SetMagicAttackMult(i32 v) {
  return rex::cvar::SetFlagByName("bd_cheat_magic_attack_mult", FormatCvar(v));
}
bool Cheats::SetDefenceMult(i32 v) {
  return rex::cvar::SetFlagByName("bd_cheat_defence_mult", FormatCvar(v));
}
bool Cheats::SetMagicDefenceMult(i32 v) {
  return rex::cvar::SetFlagByName("bd_cheat_magic_defence_mult", FormatCvar(v));
}
bool Cheats::SetAgilityMult(i32 v) {
  return rex::cvar::SetFlagByName("bd_cheat_agility_mult", FormatCvar(v));
}
bool Cheats::SetStatBonus(i32 v) {
  return rex::cvar::SetFlagByName("bd_cheat_stat_bonus", FormatCvar(v));
}
bool Cheats::SetExpMult(i32 v) {
  return rex::cvar::SetFlagByName("bd_cheat_exp_mult", FormatCvar(v));
}
bool Cheats::SetSpMult(i32 v) {
  return rex::cvar::SetFlagByName("bd_cheat_sp_mult", FormatCvar(v));
}
bool Cheats::SetGoldMult(i32 v) {
  return rex::cvar::SetFlagByName("bd_cheat_gold_mult", FormatCvar(v));
}
bool Cheats::SetMedalsMult(i32 v) {
  return rex::cvar::SetFlagByName("bd_cheat_medals_mult", FormatCvar(v));
}
bool Cheats::SetInfiniteMedals(bool v) {
  return rex::cvar::SetFlagByName("bd_cheat_infinite_medals", FormatCvar(v));
}
bool Cheats::SetInfiniteItems(bool v) {
  return rex::cvar::SetFlagByName("bd_cheat_infinite_items", FormatCvar(v));
}
bool Cheats::SetGiveAllItems(bool v) {
  return rex::cvar::SetFlagByName("bd_cheat_give_all_items", FormatCvar(v));
}

void Cheats::AdoptCvars() {
  for (const auto &entry : kCvars)
    (this->*entry.adopt)();
  AdoptGrants();
}

void Cheats::ResetAll() {
  // Every cheat cvar back to its default, so a value written from the console
  // is cleared along with the menu's. bd_cheat_diag stays put: it logs this
  // reset rather than being part of it. The names come from kCvars and
  // kGrantEntries -- the same tables Init registers callbacks on.
  for (const auto &entry : kCvars)
    rex::cvar::ResetToDefault(entry.name);
  for (const auto &grant : kGrantEntries)
    rex::cvar::ResetToDefault(grant.cvar);

  // A queued one-shot is cancelled with its setting: the Adopt mirror below
  // clears the pending flags, and a button mid-flash disarms now rather than
  // finishing it after the reset.
  litGrants_ = 0;
  litSteps_ = 0;

  // ResetToDefault writes storage without firing the change callbacks, so
  // re-read what it stored.
  AdoptCvars();
  BD_CHEAT_DIAG("[cheat-diag] reset_all: {} cvars back to defaults",
                std::size(kCvars) + std::size(kGrantEntries));
}

void Cheats::Init() {
  AdoptCvars();

  auto reg = [](const char *name, void (Cheats::*adopt)()) {
    rex::cvar::RegisterChangeCallback(
        name, [adopt](std::string_view, std::string_view) {
          (Cheats::Get().*adopt)();
        });
  };
  for (const auto &entry : kCvars)
    reg(entry.name, entry.adopt);
  for (const auto &grant : kGrantEntries)
    reg(grant.cvar, &Cheats::AdoptGrants);

  // Subscriptions are permanent and callback-scoped, so the handles the bus
  // hands over are not kept: the snapshot holds scalars and character
  // addresses.
  Events::Subscribe<BattleStarted>(
      [](const BattleStarted &) { Cheats::Get().OnBattleStarted(); });
  Events::Subscribe<BattleEnded>(
      [](const BattleEnded &) { Cheats::Get().OnBattleEnded(); });
}

// ---- per-step pins ----

bool Cheats::AnyPin() const {
  return invincible_ || infiniteMP_ || infiniteGold_ || statusImmune_ ||
         unlockClasses_ || oneHitKill_ || infiniteMedals_ || infiniteItems_;
}

void Cheats::Apply() {
  const auto &g = Game::Get();
  // A loader slot mid-state is a half-built save: the party walk cannot tell a
  // stale node from a live one there, and every accessor below would be
  // reading whatever the heap last held.
  if (!g.IsReady() || g.IsLoading()) {
    SetLogicSpeed(1.0);
    return;
  }

  // Re-decided every step from whether a battle manager answered this step,
  // so leaving a battle any way at all -- won, fled, game over -- drops back
  // to normal speed on the next step without relying on an edge event.
  SetLogicSpeed(battleSpeed_ > 1 && g.BattleTask() ? double(battleSpeed_) : 1.0);

  // The settle window runs even with every pin off, because a reward
  // multiplier is not a pin and owes its remainder regardless. It always runs
  // out rather than stopping at the first payment: EXP, SP and gold can bank
  // on different frames, and each payment moves its own snapshot, so a later
  // one pays from the updated value instead of being missed -- or doubled.
  if (settleSteps_ > 0) {
    --settleSteps_;
    AwardRewards();
    if (settleSteps_ == 0)
      armed_ = false;
  }

  // Ahead of the AnyPin gate: this is a one-shot action, not a pin, so it must
  // still run on a frame where every pin happens to be off. Sitting below that
  // early return is why selecting it alone did nothing and left the row On.
  if (pendingAchievements_) {
    pendingAchievements_ = false;
    Achievements::UnlockAll();
    litGrants_ |= (1u << 30);
    litSteps_ = kGrantLitSteps;
  }

  if (pendingResetAchv_) {
    pendingResetAchv_ = false;
    Achievements::LockAll();
    litGrants_ |= (1u << 29);
    litSteps_ = kGrantLitSteps;
  }

  if (pendingLevelUp_) {
    const i32 levels = pendingLevelUp_;
    pendingLevelUp_ = 0;
    RaiseLevels(levels);
  }

  {
    std::vector<std::pair<u32, u32>> gives;
    {
      std::lock_guard lock(giveMutex_);
      gives.swap(pendingGives_);
    }
    for (const auto &[id, count] : gives)
      GiveItem(id, count);
  }

  if (pendingClassRankUp_) {
    const i32 ranks = pendingClassRankUp_;
    pendingClassRankUp_ = 0;
    RaiseClassRanks(ranks);
  }

  if (pendingGiveAll_) {
    pendingGiveAll_ = false;
    FillInventory(ItemCategory::kNone);
    litGrants_ |= (1u << 31);
    litSteps_ = kGrantLitSteps;
  }

  if (pendingGrants_) {
    const u32 pending = pendingGrants_;
    pendingGrants_ = 0;
    for (u32 i = 0; i < std::size(kGrantEntries); ++i) {
      if (!(pending & (1u << i)))
        continue;
      FillInventory(kGrantEntries[i].cat);
      litGrants_ |= (1u << i);
    }
    litSteps_ = kGrantLitSteps;
  }

  // The disarm the press has been waiting on. Deferred rather than immediate
  // so the button reads as pressed for a beat; still automatic, because a row
  // left on would fire again from the profile on the next launch and overwrite
  // the bag every time the game starts.
  if (litSteps_ > 0 && --litSteps_ == 0 && litGrants_) {
    for (u32 i = 0; i < std::size(kGrantEntries); ++i)
      if (litGrants_ & (1u << i))
        SetGrant(kGrantEntries[i].cat, false);
    if (litGrants_ & (1u << 31))
      SetGiveAllItems(false);
    if (litGrants_ & (1u << 30))
      SetUnlockAchievements(false);
    if (litGrants_ & (1u << 29))
      SetResetAchievements(false);
    // The turn-offs above re-mirror the cvars that are still on; drop any bit
    // that is lit so a row that just disarmed cannot queue itself again.
    pendingGrants_ &= ~litGrants_;
    litGrants_ = 0;
  }

  if (!AnyPin())
    return;

  if (invincible_ || infiniteMP_ || statusImmune_) {
    const auto party = g.FieldPlayerEntity().Party();
    if (party) {
      const size_t n = party.Size();
      for (size_t i = 0; i < n; ++i) {
        Player c = party.At(i).Chara();
        // Reviving is the game's own flow. Topping a knocked-out member's HP
        // up from here leaves the KO status bit set behind it, and the two
        // disagreeing is worse than the member staying down.
        if (!c || !c.IsAlive())
          continue;
        if (invincible_ && c.HP() < c.MaxHP()) {
          const u32 from = c.HP();
          if (c.SetHP(c.MaxHP()) && !toldInvincible_) {
            toldInvincible_ = true;
            BD_CHEAT_DIAG("[cheat-diag] invincible: slot {} HP {} -> {}", c.SlotId(),
                    from, c.MaxHP());
          }
        }
        if (infiniteMP_ && c.MP() < c.MaxMP()) {
          const u32 from = c.MP();
          if (c.SetMP(c.MaxMP()) && !toldInfiniteMP_) {
            toldInfiniteMP_ = true;
            BD_CHEAT_DIAG("[cheat-diag] infinite_mp: slot {} MP {} -> {}", c.SlotId(),
                    from, c.MaxMP());
          }
        }
        if (statusImmune_) {
          for (u32 r = 0; r < kCharaResistCount; ++r)
            c.SetStatusResist(static_cast<CharaResist>(r), kResistImmune);
          // Resistance stops the next application; it does nothing about one
          // already on the books, and the two turn counters tick under their
          // own status bits rather than under the flag word.
          if (c.StatusFlags() != 0) {
            const u32 had = c.StatusFlags();
            c.SetStatusFlags(0);
            if (!toldStatusImmune_) {
              toldStatusImmune_ = true;
              BD_CHEAT_DIAG("[cheat-diag] status_immune: slot {} cleared 0x{:X}",
                      c.SlotId(), had);
            }
          }
          if (c.ParalyzeTurns() != 0)
            c.SetParalyzeTurns(0);
          if (c.StunTurns() != 0)
            c.SetStunTurns(0);
        }
      }
    }
  }

  // The stat bonus is not a pin: LendStatBonus applies it per recompute, so
  // nothing of it is ever left in the save.
  if (unlockClasses_) {
    const auto roster = g.FieldPlayerEntity().Roster();
    if (roster) {
      const size_t n = roster.Size();
      for (size_t i = 0; i < n; ++i) {
        Player c = roster.At(i).Chara();
        if (!c)
          continue;
        if (c.UnlockedClasses() != kAllClasses) {
          const u32 had = c.UnlockedClasses();
          if (c.SetUnlockedClasses(kAllClasses) && !toldUnlockClasses_) {
            toldUnlockClasses_ = true;
            BD_CHEAT_DIAG("[cheat-diag] unlock_classes: slot {} 0x{:X} -> 0x{:X}",
                    c.SlotId(), had, kAllClasses);
          }
        }
      }
    }
  }

  if (oneHitKill_) {
    const auto b = g.BattleTask();
    // The manager handle is captured by the battle scene update once per step
    // and only trusted inside the step that produced it.
    if (b) {
      const size_t n = b.EnemyCount();
      for (size_t i = 0; i < n; ++i) {
        Enemy e = b.EnemyAt(i).Chara();
        // 1 rather than 0: the guest's own death path is what clears the
        // actor, awards the battle and ends the encounter, so the kill has to
        // come from a hit landing rather than from this write.
        if (e && e.IsAlive() && e.HP() > 1)
          e.SetHP(1);
      }
    }
  }

  if (infiniteItems_) {
    auto inv = g.ItemSaveData();
    if (inv) {
      // Topped up every step rather than restored on use, so a stack never
      // reaches zero in the first place: the guest clears a slot's id the
      // moment its count runs out, and a cleared slot has nothing left to
      // identify the item by.
      const size_t slots = inv.SlotCount();
      for (size_t s = 0; s < slots; ++s) {
        const auto it = inv.At(s);
        if (it.id == 0 || it.count >= kItemStackMax)
          continue;
        // Key items are left alone: story checks can expect exactly one, and
        // nothing about them is spent by use anyway.
        if (CategoryOf(it.id) == ItemCategory::kValuable)
          continue;
        const u32 had = it.count;
        if (inv.SetAt(s, it.id, kItemStackMax) && !toldInfiniteItems_) {
          toldInfiniteItems_ = true;
          BD_CHEAT_DIAG("[cheat-diag] infinite_items: slot {} item {} {} -> {}", s,
                  it.id, had, kItemStackMax);
        }
      }
    }
  }

  if (infiniteMedals_) {
    auto inv = g.ItemSaveData();
    if (inv && inv.Medals() < ItemSaveData::kMedalsMax) {
      const u32 from = inv.Medals();
      if (inv.SetMedals(ItemSaveData::kMedalsMax) && !toldInfiniteMedals_) {
        toldInfiniteMedals_ = true;
        BD_CHEAT_DIAG("[cheat-diag] infinite_medals: {} -> {}", from,
                ItemSaveData::kMedalsMax);
      }
    }
  }

  if (infiniteGold_) {
    auto inv = g.ItemSaveData();
    if (inv && inv.Gold() < kGoldMax) {
      const u32 from = inv.Gold();
      if (inv.SetGold(kGoldMax) && !toldInfiniteGold_) {
        toldInfiniteGold_ = true;
        BD_CHEAT_DIAG("[cheat-diag] infinite_gold: {} -> {}", from, kGoldMax);
      }
    }
  }
}

// ---- levels ----

void Cheats::RaiseLevels(i32 levels) {
  const auto &g = Game::Get();
  const auto roster = g.FieldPlayerEntity().Roster();
  BD_CHEAT_DIAG("[cheat-diag] level_up: requested +{}", levels);
  if (!roster || levels <= 0) {
    BD_CHEAT_DIAG("[cheat-diag] level_up: no roster yet, skipped");
    return;
  }
  const u32 tablePtr = mem::try_load<u32>(kLevelTablePtrEA);
  const u32 maxLevel = tablePtr ? mem::try_load<u32>(tablePtr) : 0;
  // A sane table names a max in the low hundreds at most; anything else means
  // the pointer is not what this expects, so do nothing rather than guess.
  if (maxLevel == 0 || maxLevel > 255) {
    BD_CHEAT_DIAG("[cheat-diag] level_up: level table at 0x{:08X} reads max {}, "
                  "skipped",
                  tablePtr, maxLevel);
    return;
  }
  const size_t n = roster.Size();
  for (size_t i = 0; i < n; ++i) {
    Player c = roster.At(i).Chara();
    if (!c)
      continue;
    const u32 from = c.Level();
    if (from >= maxLevel)
      continue;
    const u32 target =
        levels >= kLevelUpMax
            ? maxLevel
            : std::min<u32>(maxLevel, from + static_cast<u32>(levels));
    u32 need = 0;
    {
      rex::ppc::stack_guard guard;
      need = GuestExpForLevel(c.Address(), target);
    }
    const u32 have = c.Exp();
    if (need <= have) {
      BD_CHEAT_DIAG("[cheat-diag] level_up: slot {} level {} target {} needs {} "
                    "exp, has {}: nothing to add",
                    c.SlotId(), from, target, need, have);
      continue;
    }
    {
      rex::ppc::stack_guard guard;
      GuestAddExp(c.Address(), need - have);
    }
    BD_CHEAT_DIAG("[cheat-diag] level_up: slot {} level {} -> {} (exp {} -> {})",
                  c.SlotId(), from, c.Level(), have, c.Exp());
  }
}

void Cheats::RaiseClassRanks(i32 ranks) {
  const auto &g = Game::Get();
  const auto roster = g.FieldPlayerEntity().Roster();
  BD_CHEAT_DIAG("[cheat-diag] class_rank_up: requested +{}", ranks);
  if (!roster || ranks <= 0)
    return;
  const u32 maxRank = mem::try_load<u32>(kMaxClassRankEA);
  if (maxRank == 0 || maxRank > 255) {
    BD_CHEAT_DIAG("[cheat-diag] class_rank_up: max rank reads {}, skipped",
                  maxRank);
    return;
  }
  const size_t n = roster.Size();
  for (size_t i = 0; i < n; ++i) {
    Player c = roster.At(i).Chara();
    if (!c)
      continue;
    for (u32 k = 0; k < kCharaClassCount; ++k) {
      const auto cls = static_cast<CharaClass>(k);
      // A locked class is left for the game (or Unlock All Classes) to open.
      if (!c.IsClassUnlocked(cls))
        continue;
      const u32 from = c.ClassRank(cls);
      if (from >= maxRank)
        continue;
      const u32 target =
          ranks >= kLevelUpMax
              ? maxRank
              : std::min<u32>(maxRank, from + static_cast<u32>(ranks));
      u32 need = 0;
      {
        rex::ppc::stack_guard guard;
        need = GuestSPForRank(k, target);
      }
      const u32 have = c.ClassSP(cls);
      if (need <= have)
        continue;
      {
        rex::ppc::stack_guard guard;
        GuestAddClassSP(c.Address(), k, need - have);
      }
      BD_CHEAT_DIAG("[cheat-diag] class_rank_up: slot {} class {} rank {} -> {} "
                    "(sp {} -> {})",
                    c.SlotId(), k, from, c.ClassRank(cls), have, c.ClassSP(cls));
    }
  }
}

// ---- derived stats ----

// Walks the id space rather than a list: the record table is the only account
// of which ids exist, and ItemRecord answers zero for the gaps. Stops at the
// last slot, so a table larger than the inventory fills it and no more.
void Cheats::GiveItem(u32 itemId, u32 count) {
  auto inv = Game::Get().ItemSaveData();
  if (!inv || !Pickable(itemId))
    return;
  // Onto the stack already holding it, as the game credits a pickup;
  // otherwise into the first empty slot.
  const size_t slots = inv.SlotCount();
  size_t target = slots;
  for (size_t s = 0; s < slots; ++s) {
    const auto held = inv.At(s);
    if (held.id == itemId) {
      target = s;
      break;
    }
    if (held.id == 0 && target == slots)
      target = s;
  }
  if (target == slots) {
    BD_CHEAT_DIAG("[cheat-diag] give_item: {} x{}: bag full", itemId, count);
    return;
  }
  const auto held = inv.At(target);
  const u32 had = held.id == itemId ? held.count : 0;
  const u32 want = std::min<u32>(had + count, kItemStackMax);
  inv.SetAt(target, itemId, want);
  BD_CHEAT_DIAG("[cheat-diag] give_item: {} '{}' slot {} {} -> {}", itemId,
                GameTables::Get().PhenomeName(itemId), target, had, want);
}

void Cheats::FillInventory(ItemCategory only) {
  auto inv = Game::Get().ItemSaveData();
  if (!inv)
    return;
  const size_t slots = inv.SlotCount();
  auto &tables = GameTables::Get();

  // Adds rather than replaces. Writing from slot 0 meant a second grant sat on
  // top of the first, which is not what a row that says "+1" should do to a
  // bag you already filled.
  std::vector<bool> present(kItemCategoryMaxId + 2, false);
  std::vector<size_t> freeSlots;
  for (size_t s = 0; s < slots; ++s) {
    const auto held = inv.At(s);
    if (held.id == 0)
      freeSlots.push_back(s);
    else if (held.id <= kItemCategoryMaxId)
      present[held.id] = true;
  }

  size_t next = 0;
  u32 added = 0;
  u32 had = 0;
  bool full = false;
  for (u32 id = 1; id <= kMaxItemId; ++id) {
    if (!tables.PhenomeRecord(id))
      continue;
    const ItemCategory cat = CategoryOf(id);
    // kNone covers both ids the sheet never classified and the ones it marks
    // abolished, which are the entries that read "Worthless Junk" in game.
    if (cat == ItemCategory::kNone)
      continue;
    if (only != ItemCategory::kNone && cat != only)
      continue;
    if (id <= kItemCategoryMaxId && present[id]) {
      ++had;
      continue;
    }
    if (next >= freeSlots.size()) {
      full = true;
      break;
    }
    // One each, not a full stack: the row says "+1" and should mean it.
    // Infinite Items is the separate switch for holding stacks at 99.
    if (inv.SetAt(freeSlots[next], id, 1)) {
      ++next;
      ++added;
    }
  }

  BD_CHEAT_DIAG("[cheat-diag] give_items(cat={}): +{} new, {} already held, {} free "
          "slot(s) left{}",
          static_cast<u32>(only), added, had,
          freeSlots.size() - next, full ? " [FULL: no free slots]" : "");
}

Cheats::BonusLoan Cheats::LendStatBonus(u32 charaEA) {
  BonusLoan loan;
  if (statBonus_ <= 0 || !charaEA)
    return loan;
  Player c(charaEA);
  if (!c)
    return loan;
  const u32 bonus = static_cast<u32>(statBonus_);
  loan.charaEA = charaEA;
  for (u32 b = 0; b < kPermanentBonusCount; ++b) {
    const auto which = static_cast<PermanentBonus>(b);
    loan.saved[b] = c.PermanentBonus(which);
    // Held at the bonus rather than added to it, as before: a slot already
    // above it from the game's own items keeps its own value.
    if (loan.saved[b] < bonus)
      c.SetPermanentBonus(which, bonus);
  }
  if (!toldStatBonus_) {
    toldStatBonus_ = true;
    BD_CHEAT_DIAG("[cheat-diag] stat_bonus: lent +{} to chara 0x{:08X} for one "
                  "recompute",
                  bonus, charaEA);
  }
  return loan;
}

void Cheats::ReturnStatBonus(const BonusLoan &loan) {
  if (!loan.charaEA)
    return;
  Player c(loan.charaEA);
  for (u32 b = 0; b < kPermanentBonusCount; ++b)
    c.SetPermanentBonus(static_cast<PermanentBonus>(b), loan.saved[b]);
}

void Cheats::ScaleBattleParams(u32 paramsEA) {
  if (!paramsEA)
    return;
  if (attackMult_ > 1)
    ScaleStat(paramsEA, offsetof(ScaledStats_t, attack), attackMult_);
  if (magicAttackMult_ > 1)
    ScaleStat(paramsEA, offsetof(ScaledStats_t, magicAttack),
              magicAttackMult_);
  if (defenceMult_ > 1)
    ScaleStat(paramsEA, offsetof(ScaledStats_t, defense), defenceMult_);
  if (magicDefenceMult_ > 1)
    ScaleStat(paramsEA, offsetof(ScaledStats_t, magicDefense),
              magicDefenceMult_);
  if (agilityMult_ > 1)
    ScaleStat(paramsEA, offsetof(ScaledStats_t, agility), agilityMult_);
}

// ---- per-fight rewards ----

namespace {
// The battle manager's per-fight EXP and SP, and the result window it copies
// them into: BattleResult_Step stores +272/+276 into window+740/+744 just
// before it awards them.
constexpr u32 kBattleExpOff = 272;
constexpr u32 kBattleSpOff = 276;
constexpr u32 kBattleResultWindowOff = 248;
constexpr u32 kWindowExpOff = 740;
constexpr u32 kWindowSpOff = 744;
constexpr u32 kBattleGoldOff = 268;
constexpr u32 kWindowGoldOff = 752;

u32 Scaled(u32 amount, i32 mult) {
  if (mult <= 1 || static_cast<i32>(amount) <= 0)
    return amount;
  return static_cast<u32>(std::min<u64>(static_cast<u64>(amount) * u64(mult),
                                        u64(kProgressMax)));
}
} // namespace

u32 Cheats::ScaleExpAward(u32 amount) const {
  return inResultStep_ ? Scaled(amount, expMult_) : amount;
}

u32 Cheats::ScaleSpAward(u32 amount) const {
  return inResultStep_ ? Scaled(amount, spMult_) : amount;
}

u32 Cheats::ScaleGoldAward(u32 amount) const {
  if (!inResultStep_ || goldMult_ <= 1 || static_cast<i32>(amount) <= 0)
    return amount;
  return static_cast<u32>(std::min<u64>(static_cast<u64>(amount) * u64(goldMult_),
                                        u64(kGoldMax)));
}

void Cheats::EndResultStep(u32 battleEA) {
  // Cleared on every way out, so an award outside the result is never scaled.
  struct Clear {
    bool &flag;
    ~Clear() { flag = false; }
  } clear{inResultStep_};
  if ((expMult_ <= 1 && spMult_ <= 1 && goldMult_ <= 1) || !battleEA)
    return;
  const u32 window = mem::try_load<u32>(battleEA + kBattleResultWindowOff);
  if (!window || resultWindowScaled_ == window)
    return;
  const u32 exp = mem::try_load<u32>(window + kWindowExpOff);
  const u32 sp = mem::try_load<u32>(window + kWindowSpOff);
  const u32 gold = mem::try_load<u32>(window + kWindowGoldOff);
  // Nothing copied in yet: the step has not reached the result.
  if (static_cast<i32>(exp) <= 0 && static_cast<i32>(sp) <= 0 &&
      static_cast<i32>(gold) <= 0)
    return;
  // The copy and the window were made this step from the manager's totals,
  // which the awards also read, so the scaled screen matches what is paid.
  if (exp == mem::try_load<u32>(battleEA + kBattleExpOff))
    mem::try_store<u32>(window + kWindowExpOff, Scaled(exp, expMult_));
  if (sp == mem::try_load<u32>(battleEA + kBattleSpOff))
    mem::try_store<u32>(window + kWindowSpOff, Scaled(sp, spMult_));
  if (gold == mem::try_load<u32>(battleEA + kBattleGoldOff))
    mem::try_store<u32>(window + kWindowGoldOff, ScaleGoldAward(gold));
  resultWindowScaled_ = window;
  BD_CHEAT_DIAG("[cheat-diag] result window: exp {} -> {}, sp {} -> {}, gold {} "
                "-> {}",
                exp, mem::try_load<u32>(window + kWindowExpOff), sp,
                mem::try_load<u32>(window + kWindowSpOff), gold,
                mem::try_load<u32>(window + kWindowGoldOff));
}

void Cheats::OnBattleStarted() {
  resultWindowScaled_ = 0;
  armed_ = false;
  settleSteps_ = 0;
  preBattle_.clear();

  if (medalsMult_ <= 1)
    return;

  const auto &g = Game::Get();
  if (!g.IsReady())
    return;

  const auto roster = g.FieldPlayerEntity().Roster();
  if (roster) {
    const size_t n = roster.Size();
    preBattle_.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      const Player c = roster.At(i).Chara();
      if (!c)
        continue;
      MemberSnapshot m;
      m.charaEA = c.Address();
      m.exp = c.Exp();
      // Every class, not just the active one: accessories in this game can
      // spread SP across classes, and a per-class diff catches that for free.
      for (u32 k = 0; k < kCharaClassCount; ++k)
        m.sp[k] = c.ClassSP(static_cast<CharaClass>(k));
      preBattle_.push_back(m);
    }
  }
  preGold_ = g.ItemSaveData().Gold();
  preMedals_ = g.ItemSaveData().Medals();
  armed_ = true;
}

void Cheats::OnBattleEnded() {
  if (!armed_)
    return;
  // The award usually lands before this edge, which publishes from the battle
  // camera destructor. Pay what is already banked and open the settle window
  // either way: EXP, SP and gold can land on different frames, so a payment at
  // the edge must not close it -- each one moves its own snapshot, so nothing
  // gets paid twice.
  AwardRewards();
  settleSteps_ = kSettleSteps;
}

bool Cheats::AwardRewards() {
  if (!armed_)
    return false;

  const auto &g = Game::Get();
  if (!g.IsReady() || g.IsLoading())
    return false;

  bool paid = false;

  // EXP and SP are not topped up here: they are scaled inside the game's
  // own battle result (ScaleExpAward / ScaleSpAward), so the result screen
  // shows them and the guest's level and rank loops pay them out.

  if (medalsMult_ > 1) {
    auto inv = g.ItemSaveData();
    if (inv) {
      const u32 now = inv.Medals();
      const u32 want =
          TopUp(preMedals_, now, medalsMult_, ItemSaveData::kMedalsMax);
      if (want != now && inv.SetMedals(want)) {
        BD_CHEAT_DIAG("[cheat-diag] medals snap={} now={} -> after={}", preMedals_,
                now, want);
        preMedals_ = want;
        paid = true;
      }
    }
  }

  // Gold is scaled inside the battle result (ScaleGoldAward) like EXP.

  // armed_ stays set until the settle window runs out in Apply(): a value can
  // still bank on a later frame, and its snapshot has moved with each payment.
  return paid;
}

} // namespace bd::engine
