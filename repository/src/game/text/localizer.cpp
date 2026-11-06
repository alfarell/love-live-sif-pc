#include "game/text/localizer.h"

#include <utility>

#include "engine/fs/vfs.h"
#include "engine/ui/ui_screen.h"

namespace sif {
namespace {

int LanguageIndex(const std::string& lang) {
    return lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0;
}

}

bool Localizer::Load(const Vfs& vfs, const std::string& lang, std::string* error) {
    by_japanese_.clear();
    if (!messages_.Load(vfs, LanguageIndex(lang), error)) return false;
    for (int i = 0; i < int(messages_.Size()); ++i) {
        const std::string& japanese = messages_.Japanese(i);
        if (!japanese.empty()) by_japanese_.try_emplace(japanese, i);
    }
    return true;
}

bool Localizer::ForDefault(std::string_view japanese, std::string& out) const {
    const auto it = by_japanese_.find(japanese);
    if (it == by_japanese_.end()) return false;
    const std::string& text = messages_.Get(it->second);
    if (text.empty()) return false;
    out = text;
    return true;
}

bool LocalizeScreen(UiScreen& ui, const Vfs& vfs, const std::string& lang, std::string* error) {
    Localizer localizer;
    if (!localizer.Load(vfs, lang, error)) return false;
    ui.LocalizeTexts([&](std::string_view japanese, std::string& out) { return localizer.ForDefault(japanese, out); });
    return true;
}

}
