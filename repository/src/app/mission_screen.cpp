#include "app/mission_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <set>
#include <cstdio>

#include "app/fade.h"
#include "app/input.h"
#include "app/route.h"
#include "app/skill_screen.h"
#include "app/top_menu.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/costume/costume.h"
#include "game/member/member.h"
#include "game/profile/profile.h"
#include "game/text/messages.h"

namespace sif {
namespace {

constexpr const char* kRoot = "uil0001_MisLayout00";
// uil0003_MisplateLayout00 places plates _00 .. _06; _00 sits above the list frame and _06 below it (the game scrolls
// them through the frame's stencil), so the port fills the 5 inside: shortcut: whole-row steps, no smooth scroll
constexpr int kFirstPlate = 1, kPlates = 5;
// eve_mis_gauge00_01 fills u 162..862 of 1024
constexpr float kGaugeU0 = 162.0f / 1024.0f, kGaugeU1 = 862.0f / 1024.0f;

// The tabs (uip0006_missonPageTab00 label atlas eve_mis_type_tab_text00_00_T, rows of 1/8: Member, Group, Songs,
// Costumes) and the frame layout drawn for each active tab (uil0002_MisframeLayout00 places all four)
constexpr int kTabs = 4;
constexpr std::array<const char*, kTabs> kTabLayouts = {"uil0006_missipntabOshi00", "uil0007_missipntabCom00",
                                                        "uil0008_missipntabMusic00", "uil0009_missipntabDress00"};
// The tab of a condition kind: member (recommended table, 20-34), group (common, 0-19), songs (35-39), costumes (40-48)
int TabOf(int kind) { return kind >= 40 ? 3 : kind >= 35 ? 2 : kind >= 20 ? 0 : 1; }

void ApplyTabs(UiScreen& ui, int active, const std::array<int, 4>& claimable) {
    for (int v = 0; v < kTabs; ++v) {
        const std::string layout = std::string(kRoot) + "/uil0002_MisframeLayout00/" + kTabLayouts[size_t(v)];
        ui.HideInstance(layout, v != active);
        for (int k = 0; k < kTabs; ++k) {
            char holder[64];
            std::snprintf(holder, sizeof(holder), "/uil0005_missipntabAll00_%02d", k);
            const std::string tab = layout + holder + "/uip0006_missonPageTab00";
            ui.SetMaterialUv(tab, "M_tab_text00", 0.0f, 0.125f * float(k));
            ui.HideNode(tab, "missontab00_ON", k != active);
            ui.HideNode(tab, "missontab00_OFF", k == active);
            // box 0: the number of rewards ready to claim; box 1 sits under it (its use is not known; both drawn showed
            // the number twice), left empty
            ui.SetInstanceText(tab, 0, claimable[size_t(k)] ? std::to_string(claimable[size_t(k)]) : "");
            ui.SetInstanceText(tab, 1, "");
            ui.HideInstance(layout + holder + "/common_uip0017_New00", true);
        }
    }
}

std::string Holder(int i) {
    char s[128];
    std::snprintf(s, sizeof(s), "%s/uil0003_MisplateLayout00/uil0004_missionplateAll00_%02d", kRoot, i);
    return s;
}

struct Row {
    const mission::Mission* m = nullptr;
    std::string title, text;
    mission::Progress progress;
    const mission::Award* accolade = nullptr;
    const char* node = nullptr;  // the reward picture's node and material, "" path = none (Q1 §3, eboot FUN_00112270)
    const char* material = nullptr;
    std::string picture;
    bool item_frame = false;     // reward_icon_frame00 behind an item or booster
};

// The reward picture of a row, by reward type (Q1 §3). shortcut: the item frame keeps the layout's own
// (material_iconframe00_00; the eboot picks one per item, FUN_0028d7a0, not traced); frames show portrait.
void RewardPicture(Row& row, const MissionData& data, const std::string& lang, int member) {
    const mission::Mission& m = *row.m;
    const mission::Context* c = data.context;
    char s[128];
    switch (m.reward_type) {
        case 1: row.node = "reward_iconB00", row.material = "M_reward_iconB00", row.picture = SkillThumbPath(m.reward_id, lang, true); break;
        case 2:
            if (const costume::Item* i = c && c->costumes ? costume::FindItem(*c->costumes, m.reward_id) : nullptr) {
                row.node = "reward_iconA00", row.material = "M_reward_icon00", row.picture = costume::IconPath(*i);
                row.item_frame = true;
            }
            break;
        case 3:
            std::snprintf(s, sizeof(s), "ui/ui_reward_icon.pac/texture.pac/reward_%d", m.reward_id);
            row.node = "reward_iconA00", row.material = "M_reward_icon00", row.picture = s, row.item_frame = true;
            break;
        case 4:
            if (row.accolade) {
                std::snprintf(s, sizeof(s), "ui/ui_honorific_title_s.pac/texture.pac/title_plate_s_%04d", row.accolade->group);
                row.node = "reward_icon_title00", row.material = "reward_icon_title00_M", row.picture = s;
            }
            break;
        case 5:
            if (data.frames) {
                for (const profile::Frame& f : data.frames->frames) {
                    if (f.id != m.reward_id) continue;
                    std::snprintf(s, sizeof(s), "ui/ui_profile_card.pac/texture.pac/pfc_frame_000_%03d", f.member);
                    row.node = "reward_iconB00", row.material = "M_reward_iconB00", row.picture = s;
                }
            }
            break;
        case 6: row.node = "reward_icon_name00", row.material = "reward_icon_name00_M", row.picture = NamePlatePath(m.reward_id); break;
        case 7: {  // her member card in that costume (mbc_<(member x 1000 + costume) x 10>, as the live intro); 117/217 by group
            int dress = m.reward_id;
            if (dress == 117 && member::Group(member) == 2) dress = 217;
            if (dress == 217 && member::Group(member) != 2) dress = 117;
            row.node = "reward_iconB00", row.material = "M_reward_iconB00";
            row.picture = "ui/ui_member_card_thumbnail.pac/texture.pac/mbc_" + std::to_string((member * 1000 + dress) * 10);
            break;
        }
        default: break;
    }
}

void Apply(UiScreen& ui, const std::vector<Row>& rows, int top, int cursor, const std::set<int>& claimed) {
    for (int i : {0, 6}) ui.HideInstance(Holder(i), true);
    for (int i = 0; i < kPlates; ++i) {
        const std::string holder = Holder(kFirstPlate + i), plate = holder + "/uip0001_mission_plate00";
        const int r = top + i;
        ui.HideInstance(holder, r >= int(rows.size()));
        if (r >= int(rows.size())) continue;
        const Row& row = rows[size_t(r)];
        const bool done = row.progress.Done(), taken = claimed.contains(row.m->id), focused = r == cursor;
        // text boxes (probed): 0 title, 2 description, 3 progress, 4 reward count, 5 "press to claim"; 1, 6, 7 unused
        ui.SetInstanceText(plate, 0, row.title);
        ui.SetInstanceText(plate, 2, row.text);
        const int need = row.progress.need, have = std::clamp(row.progress.have, 0, need);
        // not tracked by the port yet: "-" and an empty gauge
        ui.SetInstanceText(plate, 3,
                           (row.progress.have < 0 ? std::string("-") : std::to_string(have)) + "/" + std::to_string(need));
        ui.SetInstanceText(plate, 4, "x" + std::to_string(std::max(1, row.m->reward_amount)));
        for (int b : {1, 5, 6}) ui.SetInstanceText(plate, b, "");
        ui.SetInstanceText(plate, 7, row.accolade ? row.accolade->name : "");  // the accolade's name on its plate
        ui.SetMaterialClip(plate, "M_misgauge00", kGaugeU0 + (kGaugeU1 - kGaugeU0) * float(have) / float(need));
        ui.HideNode(plate, "status_icon00", !taken);          // COMPLETED: claimed
        ui.HideNode(plate, "status_icon01", !done || taken);  // GET!: ready to claim
        ui.HideNode(plate, "mission_plate00_ON", !focused);
        ui.HideNode(plate, "mission_plate00_OFF", focused);
        ui.HideNode(plate, "mission_plate00_ef", !focused);
        const bool picture = row.node && ui.SetMaterialTexture(plate, row.material, row.picture);
        if (row.node && !picture) LogWarn("missions: no reward picture {}", row.picture);
        for (const char* node : {"reward_iconA00", "reward_iconB00", "reward_iconC00", "reward_icon_name00",
                                 "reward_icon_title00"}) {
            ui.HideNode(plate, node, !picture || std::string(node) != row.node);
        }
        ui.HideNode(plate, "reward_icon_frame00", !picture || !row.item_frame);
        ui.HideInstance(holder + "/common_uip0017_New00", true);
        ui.HideInstance(holder + "/common_uip0051_ButtonCircle00", true);
    }
}

}

bool RunMissions(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const MissionData& data,
                 long max_frames) {
    route::SetScreen("missions");
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("missions: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/50_event", lang, &error, &text)) {
        LogError("missions: {}", error);
        return true;
    }
    const int language = lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0;
    std::vector<live::DressInfo> dresses;
    if (!live::LoadDressList(vfs, language, dresses, &error)) LogWarn("missions: costumes: {}", error);
    Messages messages;
    if (!messages.Load(vfs, language, &error)) LogWarn("missions: messages: {}", error);
    std::array<std::vector<Row>, kTabs> tabs;
    const int member = data.save ? save::Current(*data.save) : 1;
    if (data.missions && data.save) {
        const mission::Context* ctx = data.context;
        for (const mission::Mission& m : data.missions->rows) {
            if (size_t(m.Kind()) >= data.missions->conditions.size()) continue;
            const auto& c = data.missions->conditions[size_t(m.Kind())];
            const int tab = TabOf(m.Kind());
            if (tab == 2 && data.owned && !data.owned->Song(m.p1)) continue;     // no add-on for the song
            if (tab == 3 && data.owned && !data.owned->Costume(m.p1)) continue;  // or the costume
            const live::SongInfo* song = nullptr;
            if (data.songs && c.p1_type == 4) {
                for (const auto& s : *data.songs) {
                    if (s.id == m.p1) song = &s;
                }
            }
            // her group's rows, her own, her group's songs, her costumes; then the show rule (series one step at a time)
            if (!mission::Fits(m, c, member, song ? (song->group == 0 ? 0 : 1) : -1)) continue;
            bool owned = true;
            if (m.Rule() == 2) owned = !data.owned || data.owned->Song(m.p1);
            if (m.Rule() == 3 && ctx && ctx->costumes) {  // costume 217 is checked as 117 (Q1)
                owned = costume::Owns(*ctx->costumes, *data.save, ctx->addons, m.p1 == 217 ? 117 : m.p1);
            }
            if (!mission::Shown(m, *data.missions, *data.save, owned)) continue;
            Row row{&m, {}, {}, mission::ProgressOf(m, *data.save, data.context)};
            // [p1]: the song's title, the costume's name, the member's short name
            std::string p1 = std::to_string(m.p1);
            if (song) p1 = song->title;
            for (const auto& d : dresses) {
                if (c.p1_type == 5 && d.id == m.p1) p1 = d.name;
            }
            if (c.p1_type == 3 && !messages.Get(member::ProfileOf(m.p1).short_name).empty()) {
                p1 = messages.Get(member::ProfileOf(m.p1).short_name);
            }
            row.title = mission::Fill(c.title[size_t(language)], m, p1);
            row.text = mission::Fill(c.text[size_t(language)], m, p1);
            if (m.reward_type == 4 && data.accolades) row.accolade = mission::FindAward(*data.accolades, m.reward_id);
            RewardPicture(row, data, lang, member);
            tabs[size_t(tab)].push_back(std::move(row));
        }
    }
    LogInfo("missions: member {}: {} member, {} group, {} song, {} costume", member, tabs[0].size(), tabs[1].size(),
            tabs[2].size(), tabs[3].size());
    int tab = 1, cursor = 0, top = 0;  // start on Group (the general missions)
    static const std::set<int> kNone;
    const std::set<int>& claimed = data.save ? data.save->claimed : kNone;
    const auto claimable = [&] {
        std::array<int, kTabs> n{};
        for (int k = 0; k < kTabs; ++k) {
            for (const Row& r : tabs[size_t(k)]) n[size_t(k)] += r.progress.Done() && !claimed.contains(r.m->id);
        }
        return n;
    };
    ApplyTabs(ui, tab, claimable());
    Apply(ui, tabs[size_t(tab)], top, cursor, claimed);
    SDL_SetWindowTitle(window, "SIF port - task list");
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type != SDL_EVENT_KEY_DOWN || fade.leaving) continue;
            const int n = int(tabs[size_t(tab)].size());
            const int old_tab = tab;
            switch (e.key.scancode) {
                case SDL_SCANCODE_ESCAPE: fade.Leave(ui); break;
                case SDL_SCANCODE_UP: cursor = std::max(0, cursor - 1); break;
                case SDL_SCANCODE_DOWN: cursor = std::min(std::max(0, n - 1), cursor + 1); break;
                case SDL_SCANCODE_PAGEUP: cursor = std::max(0, cursor - kPlates); break;
                case SDL_SCANCODE_PAGEDOWN: cursor = std::min(std::max(0, n - 1), cursor + kPlates); break;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_SPACE:
                    if (cursor < n && data.save && !e.key.repeat) {
                        const mission::Mission& m = *tabs[size_t(tab)][size_t(cursor)].m;
                        if (mission::Claim(m, *data.save, data.context)) {
                            LogInfo("missions: claimed {}: reward type {} id {} x{}", m.id, m.reward_type, m.reward_id,
                                    std::max(1, m.reward_amount));
                            ApplyTabs(ui, tab, claimable());
                        }
                    }
                    break;
                case SDL_SCANCODE_LEFT: tab = (tab + kTabs - 1) % kTabs; break;
                case SDL_SCANCODE_RIGHT: tab = (tab + 1) % kTabs; break;
                default: break;
            }
            if (tab != old_tab) cursor = top = 0, ApplyTabs(ui, tab, claimable());
            top = std::clamp(top, std::max(0, cursor - kPlates + 1), cursor);
            Apply(ui, tabs[size_t(tab)], top, cursor, claimed);
        }
        const uint64_t now = SDL_GetTicksNS();
        ui.Update(double(now - last) / 1e9);
        fade.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        ui.DrawLayout(renderer, kRoot, 1.0f);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    ui.Release(renderer);
    return open;
}

}
