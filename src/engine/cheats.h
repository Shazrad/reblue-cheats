/**
 * @file    engine/cheats.h
 * @brief   Player-side cheat toggles: per-step pins, a battle-params scale,
 *          and per-fight reward multipliers.
 *
 * @license BSD 3-Clause License
 *          See LICENSE file in the project root for full license text.
 */
#pragma once

#include <atomic>
#include <mutex>
#include <utility>
#include <vector>

#include <rex/types.h>

#include "engine/chara.h"
#include "core/logging.h"
#include "engine/item_categories.h"

namespace bd::engine {

// Whether the cheat diagnostics are being logged. Off by default: the lines
// exist to prove a cheat fired, which matters while testing one and is noise
// afterwards. bd_cheat_diag turns them back on, from the console or a profile.
bool CheatDiagEnabled();


// Each setting is a cvar, so the config menu, the console and the profile's
// reblue.toml all reach the same value. Nothing here writes anything the
// engine layer does not already write for the console setters, so a pin is
// only ever a value the game could have stored itself.
//
// Three delivery routes, because the game commits three kinds of value at
// three different times:
//   Apply()             per logic step, for values the guest holds live
//   ScaleBattleParams() per stat recompute, for values the guest derives
//   the reward hooks    per battle, for values the guest awards once
class Cheats {
public:
  static Cheats &Get();

  // Adopts the current cvar values, registers a change callback per setting,
  // and subscribes to the battle edges the reward multipliers ride on. Called
  // from ReblueApp::OnPostInitLogging beside the other module Init()s.
  void Init();

  // Re-reads every setting from cvar storage, for the reset paths that write
  // storage without firing a change callback.
  void AdoptCvars();

  // Puts every cheat back to its compile-time default -- toggles off,
  // multipliers at 1 -- and re-reads them. The config menu's reset-all row
  // goes through here; bd_cheat_diag is left alone so it can still log the
  // reset that just happened.
  void ResetAll();

  bool Invincible() const { return invincible_; }
  bool SetInvincible(bool v);

  bool InfiniteMP() const { return infiniteMP_; }
  bool SetInfiniteMP(bool v);

  bool InfiniteGold() const { return infiniteGold_; }
  bool SetInfiniteGold(bool v);

  bool InfiniteMedals() const { return infiniteMedals_; }
  bool SetInfiniteMedals(bool v);

  bool InfiniteItems() const { return infiniteItems_; }
  bool SetInfiniteItems(bool v);

  // An action rather than a state: switching it on fills the inventory once,
  // then the row puts itself back to Off. Reads honestly in a menu whose other
  // rows are all settings.
  bool GiveAllItems() const { return giveAllItems_; }
  bool SetGiveAllItems(bool v);

  // One grant per category, same one-shot contract as GiveAllItems.
  bool Grant(ItemCategory c) const;
  bool SetGrant(ItemCategory c, bool v);

  bool UnlockAchievements() const { return unlockAchievements_; }
  bool SetUnlockAchievements(bool v);

  bool ResetAchievements() const { return resetAchievements_; }
  bool SetResetAchievements(bool v);

  bool StatusImmune() const { return statusImmune_; }
  bool SetStatusImmune(bool v);

  bool UnlockClasses() const { return unlockClasses_; }
  bool SetUnlockClasses(bool v);

  bool OneHitKill() const { return oneHitKill_; }
  bool SetOneHitKill(bool v);

  // Logic speed while a battle is up: 1 (off) to 4.
  i32 BattleSpeed() const { return battleSpeed_; }
  bool SetBattleSpeed(i32 v);

  // How many levels the Raise Party Level action adds (kLevelUpMax for the
  // game's max level). Only a choice: QueueLevelUp, from the menu's confirmed
  // action row, is what raises them, through the guest's own level-up path.
  static constexpr i32 kLevelUpMax = 999;
  i32 LevelUp() const { return levelUp_; }
  bool SetLevelUp(i32 v);
  void QueueLevelUp() { pendingLevelUp_ = levelUp_; }

  // The same pair for class ranks: how many ranks Raise Class Ranks adds to
  // every unlocked class of every roster member (kLevelUpMax for max rank),
  // through the guest's own class SP award and its rank-up loop.
  i32 ClassRankUp() const { return classRankUp_; }
  bool SetClassRankUp(i32 v);
  void QueueClassRankUp() { pendingClassRankUp_ = classRankUp_; }

  // Item spawner. The overlay runs on the UI thread, so it only queues: the
  // gives land from Apply, on the game step, like every other write here.
  void QueueGive(u32 itemId, u32 count);
  // Whether an id is one the spawner offers: a record exists and the
  // category sheet classifies it, which leaves out "Worthless Junk".
  static bool Pickable(u32 id);
  // The config menu's row asks for the window; the overlay hook, on the UI
  // thread, takes the request and opens it.
  void RequestItemSpawner() { spawnerRequested_.store(true); }
  bool ItemSpawnerRequested() const { return spawnerRequested_.load(); }
  bool TakeItemSpawnerRequest() { return spawnerRequested_.exchange(false); }

  // 1 leaves the derived stat alone. Applied where the guest finishes the
  // recompute, so a change lands on the next recalc rather than immediately.
  // One per stat: the physical and magic sides move independently so each can
  // be judged on its own.
  i32 AttackMult() const { return attackMult_; }
  bool SetAttackMult(i32 v);

  i32 MagicAttackMult() const { return magicAttackMult_; }
  bool SetMagicAttackMult(i32 v);

  i32 DefenceMult() const { return defenceMult_; }
  bool SetDefenceMult(i32 v);

  i32 MagicDefenceMult() const { return magicDefenceMult_; }
  bool SetMagicDefenceMult(i32 v);

  i32 AgilityMult() const { return agilityMult_; }
  bool SetAgilityMult(i32 v);

  // Flat addition to every permanent stat bonus. 0 is off.
  i32 StatBonus() const { return statBonus_; }
  bool SetStatBonus(i32 v);

  // 1 leaves the award alone.
  i32 ExpMult() const { return expMult_; }
  bool SetExpMult(i32 v);

  i32 SpMult() const { return spMult_; }
  bool SetSpMult(i32 v);

  i32 GoldMult() const { return goldMult_; }
  bool SetGoldMult(i32 v);

  i32 MedalsMult() const { return medalsMult_; }
  bool SetMedalsMult(i32 v);

  // Called from the bdMainGameStep hook after the guest step, so a pin is the
  // last write of the frame rather than something the step then spends.
  void Apply();

  // Called from the Player_CalcBattleParams hook with the destination params
  // block the guest was handed in r4. Runs after the original, which rebuilds
  // every field of that block, so scaling here never compounds across calls.
  void ScaleBattleParams(u32 paramsEA);

  // EXP/SP per Fight, applied inside the game's own battle result. The
  // result step (0x8223F3B8, battle manager in r3) copies the fight's EXP
  // and SP into the result window and then awards them per member through
  // Player_AdjustExpAward / Player_AdjustSpAward. Begin/EndResultStep bracket
  // that call: the awards are scaled only inside it, and the window's
  // numbers once per battle, so the screen shows what is actually paid.
  void BeginResultStep() { inResultStep_ = true; }
  void EndResultStep(u32 battleEA);
  u32 ScaleExpAward(u32 amount) const;
  u32 ScaleSpAward(u32 amount) const;
  u32 ScaleGoldAward(u32 amount) const;

  // The stat bonus is lent to one recompute rather than written into the
  // save: LendStatBonus raises the chara's permanent bonus slots so the
  // guest's own formula and caps apply it, and ReturnStatBonus puts the saved
  // values back once Player_CalcBattleParams returns. Turning the cheat off
  // therefore leaves the save as it was, and the next recompute drops it.
  struct BonusLoan {
    u32 charaEA = 0;
    u32 saved[kPermanentBonusCount] = {};
  };
  BonusLoan LendStatBonus(u32 charaEA);
  void ReturnStatBonus(const BonusLoan &loan);

private:
  Cheats() = default;

  void AdoptInvincible();
  void AdoptInfiniteMP();
  void AdoptInfiniteGold();
  void AdoptStatusImmune();
  void AdoptUnlockClasses();
  void AdoptOneHitKill();
  void AdoptBattleSpeed();
  void AdoptLevelUp();
  void RaiseLevels(i32 levels);
  void AdoptClassRankUp();
  void RaiseClassRanks(i32 ranks);
  void GiveItem(u32 itemId, u32 count);
  void AdoptAttackMult();
  void AdoptMagicAttackMult();
  void AdoptDefenceMult();
  void AdoptMagicDefenceMult();
  void AdoptAgilityMult();
  void AdoptStatBonus();
  void AdoptExpMult();
  void AdoptSpMult();
  void AdoptGoldMult();
  void AdoptMedalsMult();
  void AdoptInfiniteMedals();
  void AdoptInfiniteItems();
  void AdoptGiveAllItems();
  void AdoptGrants();
  void AdoptUnlockAchievements();
  void AdoptResetAchievements();
  // Every non-grant cheat cvar this module owns, paired with the Adopt that
  // re-reads it into member state. One table drives Init's change callbacks,
  // AdoptCvars' re-read and ResetAll's list, so a new cheat is added here once;
  // its REXCVAR_DEFINE stays in cheats.cpp where the storage lives. The grant
  // cvars ride on kGrantEntries with AdoptGrants.
  struct CvarEntry {
    const char *name;
    void (Cheats::*adopt)();
  };
  static constexpr CvarEntry kCvars[] = {
      {"bd_cheat_invincible", &Cheats::AdoptInvincible},
      {"bd_cheat_infinite_mp", &Cheats::AdoptInfiniteMP},
      {"bd_cheat_infinite_gold", &Cheats::AdoptInfiniteGold},
      {"bd_cheat_status_immune", &Cheats::AdoptStatusImmune},
      {"bd_cheat_unlock_classes", &Cheats::AdoptUnlockClasses},
      {"bd_cheat_one_hit_kill", &Cheats::AdoptOneHitKill},
      {"bd_cheat_battle_speed", &Cheats::AdoptBattleSpeed},
      {"bd_cheat_level_up", &Cheats::AdoptLevelUp},
      {"bd_cheat_class_rank_up", &Cheats::AdoptClassRankUp},
      {"bd_cheat_attack_mult", &Cheats::AdoptAttackMult},
      {"bd_cheat_magic_attack_mult", &Cheats::AdoptMagicAttackMult},
      {"bd_cheat_defence_mult", &Cheats::AdoptDefenceMult},
      {"bd_cheat_magic_defence_mult", &Cheats::AdoptMagicDefenceMult},
      {"bd_cheat_agility_mult", &Cheats::AdoptAgilityMult},
      {"bd_cheat_stat_bonus", &Cheats::AdoptStatBonus},
      {"bd_cheat_exp_mult", &Cheats::AdoptExpMult},
      {"bd_cheat_sp_mult", &Cheats::AdoptSpMult},
      {"bd_cheat_gold_mult", &Cheats::AdoptGoldMult},
      {"bd_cheat_medals_mult", &Cheats::AdoptMedalsMult},
      {"bd_cheat_infinite_medals", &Cheats::AdoptInfiniteMedals},
      {"bd_cheat_infinite_items", &Cheats::AdoptInfiniteItems},
      {"bd_cheat_give_all_items", &Cheats::AdoptGiveAllItems},
      {"bd_cheat_unlock_achievements", &Cheats::AdoptUnlockAchievements},
      {"bd_cheat_reset_achievements", &Cheats::AdoptResetAchievements},
  };
  // kNone fills with everything; any other value restricts to that category.
  void FillInventory(ItemCategory only);

  bool AnyPin() const;

  // What one roster member held when the battle opened. charaEA is the
  // character body itself, which is what the guest's exp award writes into.
  struct MemberSnapshot {
    u32 charaEA = 0;
    u32 exp = 0;
    u32 sp[kCharaClassCount] = {};
  };

  void OnBattleStarted();
  void OnBattleEnded();
  // Pays the multiplied remainder on top of what the guest already awarded.
  // Each payment moves its own snapshot, so a value that banks later in the
  // settle window pays from the updated number rather than twice. False when
  // it found nothing to pay this step.
  bool AwardRewards();

  bool invincible_ = false;
  bool infiniteMP_ = false;
  bool infiniteGold_ = false;
  bool statusImmune_ = false;
  bool unlockClasses_ = false;
  bool oneHitKill_ = false;
  bool inResultStep_ = false;
  u32 resultWindowScaled_ = 0; // result window already scaled this battle
  i32 battleSpeed_ = 1;
  i32 levelUp_ = 0;
  i32 pendingLevelUp_ = 0;
  i32 classRankUp_ = 1;
  i32 pendingClassRankUp_ = 0;
  std::mutex giveMutex_;
  std::vector<std::pair<u32, u32>> pendingGives_; // (item id, count)
  std::atomic<bool> spawnerRequested_{false};
  i32 attackMult_ = 1;
  i32 magicAttackMult_ = 1;
  i32 defenceMult_ = 1;
  i32 magicDefenceMult_ = 1;
  i32 agilityMult_ = 1;
  i32 statBonus_ = 0;
  i32 expMult_ = 1;
  i32 spMult_ = 1;
  i32 goldMult_ = 1;
  i32 medalsMult_ = 1;
  bool infiniteMedals_ = false;
  bool infiniteItems_ = false;
  bool giveAllItems_ = false;
  bool pendingGiveAll_ = false;
  u32 pendingGrants_ = 0; // bit per ItemCategory
  bool unlockAchievements_ = false;
  bool pendingAchievements_ = false;
  bool resetAchievements_ = false;
  bool pendingResetAchv_ = false;
  // A fired grant holds its button lit for a moment before disarming, so the
  // press is visible. Bit 31 is the give-all row; the rest match kGrantEntries.
  u32 litGrants_ = 0;
  u32 litSteps_ = 0;

  // A pin acts every logic step, so it reports only the first time it bites
  // and re-arms when its setting is touched. Enough to prove it fired without
  // filling the console the way a per-step line would.
  bool toldInvincible_ = false;
  bool toldInfiniteMP_ = false;
  bool toldStatusImmune_ = false;
  bool toldUnlockClasses_ = false;
  bool toldStatBonus_ = false;
  bool toldInfiniteGold_ = false;
  bool toldInfiniteMedals_ = false;
  bool toldInfiniteItems_ = false;

  std::vector<MemberSnapshot> preBattle_;
  u32 preGold_ = 0;
  u32 preMedals_ = 0;
  bool armed_ = false;
  // Steps left to keep retrying the award after the battle-end edge. The edge
  // publishes from the battle camera destructor, and whether the guest has
  // banked the rewards by then is not something the hook can know, so a short
  // window covers both orderings. It always runs out rather than stopping at
  // the first payment: EXP, SP and gold can bank on different frames.
  u32 settleSteps_ = 0;
};

} // namespace bd::engine

// Guarded so an argument with a cost -- a table lookup, a string build -- is
// not paid when the diagnostics are off.
#define BD_CHEAT_DIAG(...)                                                     \
  do {                                                                         \
    if (::bd::engine::CheatDiagEnabled())                                      \
      BD_INFO(__VA_ARGS__);                                                    \
  } while (0)
