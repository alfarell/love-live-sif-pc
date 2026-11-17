#include "app/profile_card.h"

#include <algorithm>
#include <cstdio>

#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/card/card.h"
#include "game/live/song.h"
#include "game/save/save.h"
#include "game/text/messages.h"

namespace sif {
namespace {

// The photo window inside the photo mesh (uip0019: photo00/01 is the whole 16:9 photo, 835.6 x 470.1; the stencils
// photo_stencil00/01 cut it): portrait keeps x -264.6..264.6 of -417.8..417.8 (all of its height), landscape keeps
// z -259..129 of -300..170.2 (all of its width)
constexpr float kPortraitU0 = (417.8f - 264.6f) / 835.6f, kPortraitU1 = 1.0f - kPortraitU0;
constexpr float kLandscapeV0 = (170.2f - 129.0f) / 470.1f, kLandscapeV1 = (170.2f + 259.0f) / 470.1f;

std::string Msg(const Messages* m, int id, const char* fallback) {
    if (!m) return fallback;
    const std::string& s = m->Get(id);
    return s.empty() ? std::string(fallback) : s;
}

}

ProfileCardArt MakeProfileCardArt(const save::ProfileCard& c, const ProfileCardTexts& t) {
    ProfileCardArt a;
    a.landscape = c.landscape;
    const int member = c.member_card / 10000, dress = c.member_card / 10 % 1000;
    a.frame_member = member > 0 ? member : 1;
    if (t.plate) {
        a.player = t.plate->player;
        for (const NameplateItem& i : t.plate->accolades) {
            if (i.id == c.accolade) a.accolade_plate = TitlePlatePath(i.plate);
        }
        for (const NameplateItem& i : t.plate->badges) {
            if (i.id == c.badge) a.badge_plate = BadgePlatePath(i.plate), a.badge_text = i.name;
        }
    }
    if (c.name_plate) a.name_plate = NamePlatePath(c.name_plate);
    if (t.songs) {
        for (const live::SongInfo& s : *t.songs) {
            if (s.id == c.live / 1000) a.song = s.title;
        }
    }
    if (t.dresses) {
        for (const live::DressInfo& d : *t.dresses) {
            if (d.id == dress) a.costume = d.name;
        }
    }
    // member names: messages 197-216 in member order (µ's 1-9, Aqours 11-19, Saint Snow 21-22)
    if (member > 0) a.member = Msg(t.messages, 197 + (member <= 9 ? member - 1 : member <= 19 ? member - 2 : member - 3), "");
    for (int k = 0; k < 3; ++k) {
        const card::SkillCard* s = t.cards ? card::FindSkill(*t.cards, c.skill[size_t(k)]) : nullptr;
        a.skills[size_t(k)] = s ? s->name[size_t(t.language)] + " Lv. " + std::to_string(std::max(1, c.skill_level[size_t(k)]))
                                : Msg(t.messages, 684, "Skill not set.");
    }
    return a;
}

void ApplyProfileCard(UiScreen& ui, const std::string& inst, const ProfileCardArt& a, const Texture* photo) {
    const char* on = a.landscape ? "layout01" : "layout00";
    const char* off = a.landscape ? "layout00" : "layout01";
    ui.HideNode(inst, off, true);
    ui.HideNode(inst, on, false);
    const std::string k = a.landscape ? "01" : "00";
    char frame[96];
    std::snprintf(frame, sizeof(frame), "ui/ui_profile_card.pac/texture.pac/pfc_frame%s_000_%03d", a.landscape ? "_h" : "",
                  a.frame_member);
    ui.SetMaterialTexture(inst, "profile_card" + k + "_M", frame);
    if (!a.accolade_plate.empty()) ui.SetMaterialTexture(inst, "title00_M", a.accolade_plate);
    if (!a.badge_plate.empty()) ui.SetMaterialTexture(inst, "badge00_M", a.badge_plate);
    if (!a.name_plate.empty()) ui.SetMaterialTexture(inst, "name00_M", a.name_plate);
    if (photo) ui.SetMaterialTexture(inst, "photo" + k, photo);  // by node: both layouts share photo00_M
    // without a photo the mesh's own picture would show at its full 16:9 size (the port draws no stencils): hidden
    ui.HideNode(inst, "photo" + k, !photo);
    // the text boxes: 0 song, 1 costume, 2 badge text, 3 player, 4 member, 5 Friendship level, 6-8 skills; the
    // landscape layout's are 9-17
    const int base = a.landscape ? 9 : 0, other = a.landscape ? 0 : 9;
    const std::array<std::string, 9> texts = {a.song,   a.costume,  a.badge_text,
                                              a.player, a.member,   std::to_string(a.level),
                                              a.skills[0], a.skills[1], a.skills[2]};
    for (int b = 0; b < 9; ++b) {
        ui.SetInstanceText(inst, base + b, texts[size_t(b)]);
        ui.SetInstanceText(inst, other + b, "");
    }
}

int MakeCardPhoto(Renderer& renderer, const std::vector<uint8_t>& rgba, uint32_t w, uint32_t h, bool landscape,
                  int crop, Texture& out) {
    // the window in photo pixels, and how far the photo may move along the free axis (x portrait, y landscape)
    const int x0 = landscape ? 0 : int(kPortraitU0 * float(w)), x1 = landscape ? int(w) : int(kPortraitU1 * float(w));
    const int y0 = landscape ? int(kLandscapeV0 * float(h)) : 0, y1 = landscape ? int(kLandscapeV1 * float(h)) : int(h);
    const int room = landscape ? y0 : x0;
    crop = std::clamp(crop, -room, room);
    std::vector<uint8_t> card(size_t(w) * h * 4, 0);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const int sx = landscape ? x : x + crop, sy = landscape ? y + crop : y;
            const uint8_t* s = &rgba[(size_t(sy) * w + size_t(sx)) * 4];
            uint8_t* d = &card[(size_t(y) * w + size_t(x)) * 4];
            std::copy_n(s, 3, d);
            d[3] = 255;
        }
    }
    renderer.Release(out);
    if (!renderer.CreateTexture(w, h, out, nullptr) || !renderer.UpdateTexture(out, card)) return crop;
    return crop;
}

}
