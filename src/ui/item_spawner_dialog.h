/**
 * @file    ui/item_spawner_dialog.h
 * @brief   Overlay window for the Item Spawner cheat: pick any item by
 *          category or name, choose a count, and add it to the bag.
 *
 * @license BSD 3-Clause, see LICENSE
 */
#pragma once

#include <string>
#include <vector>

#include <rex/types.h>

#include <rex/ui/imgui_dialog.h>

#include "engine/engine.h"
#include "engine/virtual_buttons.h"

struct ImGuiIO;

namespace rex::ui {
class ImGuiDrawer;
} // namespace rex::ui

namespace bd::ui {

// Runs on the UI thread while the guest is parked at present, so reading item
// names and the bag here is safe. Gives are only queued: Cheats applies them
// from the game step.
class ItemSpawnerDialog final : public rex::ui::ImGuiDialog {
public:
  explicit ItemSpawnerDialog(rex::ui::ImGuiDrawer *drawer);

protected:
  void OnShow() override;
  void OnDraw(ImGuiIO &io) override;

private:
  struct Entry {
    u32 id;
    engine::ItemCategory category;
    std::string name;
    std::string lowerName; // for the search filter
  };

  void BuildList();
  u32 HeldCount(u32 id) const;

  engine::HostPointerClaim pointer_;
  engine::HostInputClaim input_;
  std::vector<Entry> entries_;
  bool namesLoaded_ = false;
  int category_ = 0; // 0 = all, else ItemCategory value
  char search_[64] = {};
  u32 selected_ = 0; // item id, 0 = none
  int quantity_ = 1;
  std::string lastGiven_;
  std::vector<u32> listed_; // ids the list currently shows
};

} // namespace bd::ui
