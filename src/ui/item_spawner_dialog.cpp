/**
 * @file    ui/item_spawner_dialog.cpp
 * @license BSD 3-Clause, see LICENSE
 */
#include "ui/item_spawner_dialog.h"

#include <algorithm>
#include <cctype>
#include <cfloat>

#include <fmt/format.h>
#include <imgui.h>

#include "core/i18n.h"
#include "engine/cheats.h"
#include "engine/game.h"
#include "engine/game_tables.h"
#include "engine/item_categories.h"
#include "engine/item_save_data.h"

namespace bd::ui {

namespace {

const char *T(const char *key) { return i18n::Text(key).c_str(); }

// Combo order and label keys. Index 0 is "All"; the rest are ItemCategory
// values in enum order, so the combo index is the category value.
constexpr const char *kCategoryKeys[] = {
    "item_spawner.cat.all",   "item_spawner.cat.heal",
    "item_spawner.cat.usable", "item_spawner.cat.spellbook",
    "item_spawner.cat.arm",   "item_spawner.cat.finger",
    "item_spawner.cat.ear",   "item_spawner.cat.neck",
    "item_spawner.cat.chest", "item_spawner.cat.valuable",
};

std::string Lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return s;
}

} // namespace

ItemSpawnerDialog::ItemSpawnerDialog(rex::ui::ImGuiDrawer *drawer)
    : rex::ui::ImGuiDialog(drawer) {}

void ItemSpawnerDialog::OnShow() { BuildList(); }

void ItemSpawnerDialog::BuildList() {
  entries_.clear();
  namesLoaded_ = false;
  auto &tables = engine::GameTables::Get();
  for (u32 id = 1; id <= engine::kItemCategoryMaxId; ++id) {
    if (!engine::Cheats::Pickable(id))
      continue;
    std::string name = tables.PhenomeName(id);
    if (!name.empty())
      namesLoaded_ = true;
    else
      name = fmt::format("#{}", id);
    entries_.push_back({id, engine::CategoryOf(id), name, Lower(name)});
  }
}

u32 ItemSpawnerDialog::HeldCount(u32 id) const {
  const auto inv = engine::Game::Get().ItemSaveData();
  if (!inv)
    return 0;
  const size_t slots = inv.SlotCount();
  for (size_t s = 0; s < slots; ++s) {
    const auto held = inv.At(s);
    if (held.id == id)
      return held.count;
  }
  return 0;
}

void ItemSpawnerDialog::OnDraw(ImGuiIO &) {
  // The item table may not be up when the window opens at the title; pick
  // the names up as soon as it is.
  if (!namesLoaded_ || entries_.empty())
    BuildList();

  const ImGuiViewport *vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing,
                          ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(620, 640), ImGuiCond_Appearing);
  bool open = true;
  if (!ImGui::Begin(T("item_spawner.title"), &open,
                    ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    if (!open)
      Close();
    return;
  }
  ImGui::SetWindowFontScale(1.35f);

  const bool ready = engine::Game::Get().IsReady();
  if (!ready) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.30f, 1.0f));
    ImGui::TextWrapped("%s", T("item_spawner.need_save"));
    ImGui::PopStyleColor();
    ImGui::Spacing();
  }

  // Category and search narrow the list together.
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::BeginCombo("##category", T(kCategoryKeys[category_]))) {
    for (int i = 0; i < static_cast<int>(std::size(kCategoryKeys)); ++i) {
      const bool chosen = i == category_;
      if (ImGui::Selectable(T(kCategoryKeys[i]), chosen))
        category_ = i;
      if (chosen)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::IsWindowAppearing())
    ImGui::SetKeyboardFocusHere();
  ImGui::InputTextWithHint("##search", T("item_spawner.search"), search_,
                           sizeof(search_));
  const std::string needle = Lower(search_);

  const float footer = ImGui::GetFrameHeightWithSpacing() * 3.2f;
  if (ImGui::BeginChild("##items", ImVec2(0, -footer), ImGuiChildFlags_Borders)) {
    int shown = 0;
    for (const auto &e : entries_) {
      if (category_ != 0 && static_cast<int>(e.category) != category_)
        continue;
      if (!needle.empty() && e.lowerName.find(needle) == std::string::npos)
        continue;
      ++shown;
      const u32 held = ready ? HeldCount(e.id) : 0;
      const std::string label =
          held ? fmt::format("{}   ({} {})##{}", e.name, T("item_spawner.held"),
                             held, e.id)
               : fmt::format("{}##{}", e.name, e.id);
      if (ImGui::Selectable(label.c_str(), selected_ == e.id,
                            ImGuiSelectableFlags_AllowDoubleClick)) {
        selected_ = e.id;
        // Double-click gives straight away, the way a list usually works.
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ready) {
          engine::Cheats::Get().QueueGive(e.id, static_cast<u32>(quantity_));
          lastGiven_ = fmt::format("{} x{}", e.name, quantity_);
        }
      }
    }
    if (shown == 0)
      ImGui::TextDisabled("%s", T("item_spawner.none"));
  }
  ImGui::EndChild();
  listed_.clear();
  for (const auto &e : entries_) {
    if (category_ != 0 && static_cast<int>(e.category) != category_)
      continue;
    if (!needle.empty() && e.lowerName.find(needle) == std::string::npos)
      continue;
    listed_.push_back(e.id);
  }

  // Quantity: typed, or one of the usual amounts.
  ImGui::TextUnformatted(T("item_spawner.quantity"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(140);
  ImGui::InputInt("##qty", &quantity_, 1, 10);
  quantity_ = std::clamp(quantity_, 1, 99);
  for (int amount : {1, 10, 99}) {
    ImGui::SameLine();
    if (ImGui::SmallButton(fmt::format("{}##q{}", amount, amount).c_str()))
      quantity_ = amount;
  }

  const Entry *pick = nullptr;
  for (const auto &e : entries_)
    if (e.id == selected_)
      pick = &e;

  ImGui::BeginDisabled(!pick || !ready);
  if (ImGui::Button(T("item_spawner.give"), ImVec2(160, 0)) && pick) {
    engine::Cheats::Get().QueueGive(pick->id, static_cast<u32>(quantity_));
    lastGiven_ = fmt::format("{} x{}", pick->name, quantity_);
  }
  ImGui::EndDisabled();
  // Everything the category and search currently show, at the chosen
  // quantity: "All categories" with an empty search is every item.
  ImGui::SameLine();
  ImGui::BeginDisabled(listed_.empty() || !ready);
  if (ImGui::Button(T("item_spawner.give_all"), ImVec2(220, 0))) {
    for (u32 id : listed_)
      engine::Cheats::Get().QueueGive(id, static_cast<u32>(quantity_));
    lastGiven_ = fmt::format("{} x{}", listed_.size(), quantity_);
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button(T("item_spawner.close"), ImVec2(160, 0)) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape))
    open = false;
  if (!lastGiven_.empty()) {
    ImGui::SameLine();
    ImGui::TextDisabled("%s %s", T("item_spawner.added"), lastGiven_.c_str());
  }

  ImGui::End();
  if (!open)
    Close();
}

} // namespace bd::ui
