#include "app/event_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>

#include "app/fade.h"
#include "app/input.h"
#include "app/navigator.h"
#include "app/route.h"
#include "app/skill_screen.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/card/card.h"
#include "game/costume/costume.h"
#include "game/event/event.h"
#include "game/member/member.h"
#include "game/save/save.h"
#include "game/text/messages.h"

namespace sif {
namespace {

constexpr const char* kList = "uil0011_eve_bd_Layout01";
constexpr const char* kPage = "uil0010_eve_bd_Layout00";

int Language(const std::string& lang) { return lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0; }

std::string Fmt(const char* format, int value) {
    char s[160];
    std::snprintf(s, sizeof(s), format, value);
    return s;
}

// "Jul 25th 0:00 to Aug 1st 23:59" (the game's English; shortcut: the other languages' date order is not checked)
std::string Duration(event::Date a, event::Date b) {
    static constexpr const char* kMonth[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const auto day = [](int d) {
        const char* suffix = d % 10 == 1 && d != 11 ? "st" : d % 10 == 2 && d != 12 ? "nd" : d % 10 == 3 && d != 13 ? "rd" : "th";
        return std::to_string(d) + suffix;
    };
    return std::string(kMonth[(a.month + 11) % 12]) + " " + day(a.day) + " 0:00 to " + kMonth[(b.month + 11) % 12] + " " +
           day(b.day) + " 23:59";
}

std::string PlateSet(int index) { return std::string(kList) + Fmt("/uil0012_eve_bd_member_plateSet00_%02d", index); }

// The window shown for her: the running one, else this year's
void Shown(const event::Birthday& b, event::Date today, event::Date& start, event::Date& end) {
    if (!event::Active(b, today, &start, &end)) event::Window(b, today.year, start, end);
}

}

bool RunBirthdayEvents(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang,
                       const EventSetup& setup, long max_frames) {
    route::SetScreen("events");
    if (!setup.events || !setup.save) return true;
    save::SaveData& save = *setup.save;
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("events: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/50_event", lang, &error, &text)) {
        LogError("events: {}", error);
        return true;
    }
    Messages messages;
    if (!messages.Load(vfs, Language(lang), &error)) LogWarn("events: messages: {}", error);
    const auto box = [&](const std::string& inst, int b, const std::string& s) { ui.SetInstanceText(inst, b, s); };

    const int mode = save.mode == 1 ? 1 : 0, first = mode == 0 ? 0 : 9, count = mode == 0 ? 9 : 11;
    int focus = std::max(0, member::Index(save::Current(save)) - first);
    if (focus >= count) focus = 0;
    bool page = false;
    Navigator nav;  // her figure on the page (the game draws her in 3D on the left, as the costume screen)
    const auto member_of = [&](int k) { return member::kAll[size_t(first + k)]; };
    const auto state = [&](int m, const event::Birthday& b) {  // her points and flag for the running event (else none)
        const int year = event::EventYear(b, event::Today());
        const auto it = save.birthdays.find(m);
        save::BirthdayState s;
        if (it != save.birthdays.end() && (year == 0 || it->second.year == year)) s = it->second;
        return std::pair(year, s);
    };

    const auto show = [&] {
        const event::Date today = event::Today();
        ui.HideInstance(kList, page);
        ui.HideInstance(kPage, !page);
        for (const char* bg : {"common_uip0100_ComBG_Ms00", "common_uip0300_ComBG_EFF_Ms00", "common_uip0400_ComTopBG_Ms00"}) {
            ui.HideInstance(std::string(kList) + "/" + bg, mode != 0);
        }
        for (const char* bg : {"common_uip0110_ComBG_Aqours00", "common_uip0310_ComBG_EFF_Aqours00", "common_uip0410_ComTopBG_Aqours00"}) {
            ui.HideInstance(std::string(kList) + "/" + bg, mode != 1);
        }
        ui.SetMaterialUv(std::string(kList) + "/uip0004_missionTop00_missionTop00", "M_missionTop00", 0, 0.125f);  // "Events being held"
        box(std::string(kList) + "/uip0019_eve_bd_text00", 0, messages.Get(768));
        box(std::string(kList) + "/uip0019_eve_bd_text00", 1, messages.Get(769));
        for (int i = 0; i < 20; ++i) {
            const bool mine = i >= first && i < first + count;
            ui.HideInstance(PlateSet(i), !mine);
            if (!mine) continue;
            const int m = member::kAll[size_t(i)];
            const event::Birthday* b = event::Find(*setup.events, m);
            const std::string plate = PlateSet(i) + "/uip0017_eve_bd_member_plate00";
            ui.HideNode(plate, "eve_bd_member_plate_ef00", i - first != focus);
            // her "HAPPY BIRTHDAY!" banner: the top menu's event picture (the part's own is a placeholder; the
            // ui_bd_event thumbnails are other banners). shortcut: the plate's per-member colour and mark are not set
            ui.SetMaterialTexture(plate, "M_eve_bd_member_banner00", Fmt("ui/ui_bd_banner_top.pac/texture.pac/top_event_bd00_00_%02d", m));
            event::Date s, e;
            if (b) Shown(*b, today, s, e);
            // the boxes (probed): 0 "Event Duration", 1 the dates; 2 and 3 are a second state drawn at the same place
            box(plate, 0, messages.Get(767));
            box(plate, 1, b ? Duration(s, e) : std::string());
            box(plate, 2, ""), box(plate, 3, "");
            const auto [year, st] = b ? state(m, *b) : std::pair(0, save::BirthdayState{});
            ui.HideInstance(PlateSet(i) + "/uip0016_eve_bd_held", year == 0);  // "Now Open!"
            ui.HideInstance(PlateSet(i) + "/common_uip0023_com_clear_text00", !b || st.points < b->goal);  // "Achieved!"
        }
        if (!page) return;
        const int m = member_of(focus);
        const event::Birthday* b = event::Find(*setup.events, m);
        if (!b) return;
        const auto [year, st] = state(m, *b);
        // "<name> Birthday event": a 20-row atlas in member order
        ui.SetMaterialUv(std::string(kPage) + "/uip0014_eve_bd_title_text00", "M_bd_title_text00", 0, 0.05f * float(member::Index(m)));
        const bool reached = st.points >= b->goal, can_claim = year && reached && !st.received;
        box(std::string(kPage) + "/uip0013_eve_bd_status00", 0, messages.Get(year ? 770 : 771));
        const std::string gauge = std::string(kPage) + "/uip0012_eve_bd_gauge00";
        box(gauge, 0, Fmt(messages.Get(772).c_str(), st.points));
        box(gauge, 1, messages.Get(773));
        box(gauge, 2, reached ? std::string() : Fmt(messages.Get(774).c_str(), b->goal - st.points));
        // the gauge quad (x -512 .. 512) under its stencil (-451 .. 451): the fill runs over U 0.06 .. 0.94
        ui.SetMaterialClip(gauge, "M_eve_bd_gauge00", 0.06f + 0.88f * std::clamp(float(st.points) / float(b->goal), 0.0f, 1.0f));
        const std::string get = std::string(kPage) + "/uip0010_eve_bd_get_plate00";
        for (size_t k = 0; k < 2; ++k) {
            const bool has = k < b->skill_cards.size();
            ui.HideNode(get, k ? "reward_card01" : "reward_card00", !has);
            if (has) ui.SetMaterialTexture(get, k ? "M_reward_card01" : "M_reward_card00", SkillThumbPath(b->skill_cards[k], lang, false));
        }
        // the boxes (probed): 0 "Rewards", 1 the note under them; 2-7 unused here
        for (int k = 0; k < 8; ++k) box(get, k, k == 0 ? messages.Get(660) : k == 1 ? messages.Get(st.received ? 776 : 775) : "");
        // the key-item icons (uil0017) are not on the game's page (N2 e8.png: the two cards and the medal): hidden
        ui.HideInstance(std::string(kPage) + "/uil0017_eve_bd_get_material00", true);
        const std::string clear = std::string(kPage) + "/uip0018_eve_bd_clear_text00";
        ui.HideNode(clear, "eve_bd_clear_text00_clear", !reached);
        ui.HideNode(clear, "eve_bd_clear_text00_unclear", reached);
        const std::string button = std::string(kPage) + "/uip0011_eve_bd_button00";
        ui.HideInstance(button, !can_claim);
        ui.HideNode(button, "eve_bd_button00_off", true);
        ui.HideNode(button, "eve_bd_button_text00", true);
        ui.HideInstance(std::string(kPage) + "/uip0003_missionChara00", true);  // the 2D chara image: she is drawn in 3D
    };
    const auto open_page = [&] {
        const int m = member_of(focus);
        const auto w = save.wear.find(m);
        if (!nav.Load(renderer, vfs, m, w == save.wear.end() ? -1 : w->second, save.special.contains(m), &error)) {
            LogWarn("events: member {}: {}", m, error);
        }
        route::SetScreen("event_page");
    };
    show();
    SDL_SetWindowTitle(window, "SIF port - events");
    LogInfo("events: {} events of mode {}, today {}-{:02d}-{:02d}", count, mode, event::Today().year, event::Today().month,
            event::Today().day);
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat || fade.leaving) continue;
            const int m = member_of(focus);
            const event::Birthday* b = event::Find(*setup.events, m);
            switch (e.key.scancode) {
                case SDL_SCANCODE_LEFT:
                    if (!page) focus = (focus + count - 1) % count;
                    break;
                case SDL_SCANCODE_RIGHT:
                    if (!page) focus = (focus + 1) % count;
                    break;
                case SDL_SCANCODE_UP:
                    if (!page) focus = std::max(0, focus - (focus >= 9 ? 5 : 4));  // rows of 4 and 5 (Aqours: 4, 5, 2)
                    break;
                case SDL_SCANCODE_DOWN:
                    if (!page) focus = std::min(count - 1, focus + (focus < 4 ? 4 : 5));
                    break;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_SPACE:
                    if (!page) {
                        page = true;
                        open_page();
                        LogInfo("events: member {} page", m);
                    } else if (b) {  // Receive rewards
                        auto [year, st] = state(m, *b);
                        if (year && st.points >= b->goal && !st.received) {
                            save::BirthdayState& kept = save.birthdays[m];
                            kept.received = true;
                            for (const int c : b->skill_cards) {
                                if (setup.cards) card::AddSkill(*setup.cards, save, c);
                            }
                            for (const int item : b->items) {  // shortcut: one of each key item (the amount is not traced)
                                int& have = save.owned[{costume::kItemReward, item}];
                                have = std::min(have + 1, 9999);
                                save::CountCollected(save, 1, item, 1);  // mission kinds 8, 11
                            }
                            LogInfo("events: member {} rewards received: {} skill cards, {} key items", m, b->skill_cards.size(),
                                    b->items.size());
                        }
                    }
                    break;
                case SDL_SCANCODE_ESCAPE:
                case SDL_SCANCODE_BACKSPACE:
                    if (page) {
                        page = false;
                        nav.Release(renderer);
                        route::SetScreen("events");
                    } else {
                        fade.Leave(ui);
                    }
                    break;
                default: break;
            }
            show();
        }
        const uint64_t now = SDL_GetTicksNS();
        ui.Update(double(now - last) / 1e9);
        if (page) nav.Update(double(now - last) / 1e9);
        fade.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        if (page) {
            ui.DrawLayout(renderer, "uil0018_eve_bd_BGLayout00", 1.0f);  // the page's background
            ui.DrawLayout(renderer, kPage, 1.0f);
            renderer.Use3D();
            nav.Draw(renderer, Navigator::CostumeCamera(float(renderer.Width()) / float(std::max(1, renderer.Height()))));
        } else {
            ui.DrawLayout(renderer, kList, 1.0f);
        }
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    nav.Release(renderer);
    ui.Release(renderer);
    return open;
}

}
