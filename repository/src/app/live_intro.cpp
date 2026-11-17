#include "app/live_intro.h"

#include <SDL3/SDL.h>

#include <array>
#include <cstdio>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "app/route.h"
#include "app/skill_screen.h"
#include "engine/core/log.h"
#include "engine/formats/jpeg.h"
#include "engine/fs/vfs.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/profile/profile.h"
#include "game/text/messages.h"

namespace sif {

bool RunLiveIntro(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const LiveIntro& in, long max_frames) {
    route::SetScreen("live_intro");
    UiText text;
    std::string error;
    if (!text.Load(vfs, in.lang, &error)) LogWarn("live intro: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/90_ingame", in.lang, &error, &text)) {
        LogError("live intro: {}", error);
        return true;
    }
    const auto msg = [&](int id, const char* fallback) {
        const std::string s = in.messages ? in.messages->Get(id) : std::string();
        return s.empty() ? std::string(fallback) : s;
    };
    const std::string root = "uil0010_LiveStart00", list = root + "/uil0011_ProcList00";
    ui.TextBoxesByName("uip0056_UserProc00");
    ui.SetInstanceText(root + "/uip0051_SkillListTitle00", 0, in.song);
    ui.HideInstance(root + "/uil0020_SkillList00", true);           // the automatic skills' list (not seen in the intro)
    ui.HideInstance(list + "/uil0013_SetProcList00", true);         // "Profile Card in Use": received cards, none offline

    // the player's panel: her member card, the three skill cards (card backs for empty slots) and the nameplate
    const std::string player = list + "/uil0012_PlayerCardList00", card = player + "/uip0054_PlayerCard00";
    if (in.member_card) {
        ui.SetMaterialTexture(card, "member_card00_00",
                              "ui/ui_member_card_thumbnail.pac/texture.pac/mbc_" + std::to_string(in.member_card));
    }
    for (int k = 0; k < 3; ++k) {
        const bool set = in.skills[size_t(k)] != 0;
        const std::string n = "0" + std::to_string(k);
        ui.HideNode(card, "skill_card" + n, !set);
        ui.HideNode(card, "blank_card" + n, set);
        // shortcut: the "Lv.n" label above a set card (skill_lv0N) is not filled; the level is logged
        ui.HideNode(card, "skill_lv" + n, true);
        if (set) ui.SetMaterialTexture(card, "skill_card" + n + "_M", SkillThumbPath(in.skills[size_t(k)], in.lang, false));
    }
    if (in.plate) ShowNameplate(ui, *in.plate, player + "/unused_title", player + "/ctg03_uil0005_NameSet00");

    // Profile Card Support: the two NPC cards with "(NPC Profile Card)" above each and their (empty) skill rows. Each
    // picture is a whole rendered card, proca_preset/<photo> in the dump with the language's _en/_cn/_tw variant
    // (Japanese without), decoded with stb_image
    const std::string support = list + "/uil0014_UserProcList00", proc = support + "/uip0056_UserProc00";
    // ui_text's boxes: 0 the header, 1 and 2 the labels; the part's meshes run _text01, _text00, _text02
    static constexpr int kLabelBox[2] = {1, 2};
    ui.SetInstanceText(proc, 0, msg(814, "Profile Card Support"));
    // proc00/proc01 are the left and right cards; proc02/proc03 lie on them (not seen in use)
    std::array<Texture, 2> pictures;
    for (int k = 0; k < 4; ++k) ui.HideNode(proc, "proc0" + std::to_string(k), true);
    for (int k = 0; k < 2; ++k) {
        const profile::Preset* p = in.support[size_t(k)];
        if (!p || p->photo.size() < 4) continue;
        std::string file = p->photo;
        if (!in.lang.empty()) file.insert(file.size() - 4, "_" + in.lang);
        std::vector<uint8_t> jpg, rgba;
        uint32_t w = 0, h = 0;
        if (!vfs.Read("proca_preset/" + file, jpg, &error) || !jpeg::Decode(jpg, rgba, w, h, &error) ||
            !renderer.CreateTexture(w, h, pictures[size_t(k)], &error) || !renderer.UpdateTexture(pictures[size_t(k)], rgba)) {
            LogWarn("live intro: {}: {}", file, error);
            continue;
        }
        const std::string node = "proc0" + std::to_string(k);
        ui.SetMaterialTexture(proc, node, &pictures[size_t(k)]);
        ui.HideNode(proc, node, false);
    }
    for (int k = 0; k < 2; ++k) {
        const profile::Preset* p = in.support[size_t(k)];
        ui.SetInstanceText(proc, kLabelBox[k], p ? msg(815, "(NPC Profile Card)") : "");
        char rows[128];
        std::snprintf(rows, sizeof(rows), "%s/ctg80_uil0002_SkillSet00_s_%02d", support.c_str(), k);
        ui.HideInstance(rows, !p);
        for (int r = 0; r < 3 && p; ++r) {
            char row[160];
            std::snprintf(row, sizeof(row), "%s/uip0005_SkillParts00_S_%02d", rows, r);
            ui.SetInstanceText(row, 0, msg(684, "Skill not set."));  // the presets set no skill cards
            ui.SetInstanceText(row, 1, "");
        }
    }
    ui.HideInstance(support, !in.support[0]);
    LogInfo("live intro: '{}', member card {}, skills {} {} {}, support {} and {}", in.song, in.member_card, in.skills[0],
            in.skills[1], in.skills[2], in.support[0] ? in.support[0]->player[1] : "-",
            in.support[1] ? in.support[1]->player[1] : "-");
    bool open = true;
    ScreenFade fade;
    const uint64_t start = SDL_GetTicksNS();
    uint64_t last = start;
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && !fade.leaving &&
                (e.key.scancode == SDL_SCANCODE_RETURN || e.key.scancode == SDL_SCANCODE_SPACE)) {
                fade.Leave(ui);
            }
        }
        const uint64_t now = SDL_GetTicksNS();
        if (!fade.leaving && double(now - start) / 1e6 > kLiveIntroMs) fade.Leave(ui);
        ui.Update(double(now - last) / 1e9);
        fade.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        ui.DrawLayout(renderer, root, 1.0f);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    ui.Release(renderer);
    for (Texture& t : pictures) renderer.Release(t);
    return open;
}

}
