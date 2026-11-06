#pragma once
// UI text localization from message.pac: the ui2 packages' ui_text.bin tables keep Japanese defaults even in the
// `_en`/`_cn`/`_tw` packages, so a default is replaced by the message whose Japanese text is exactly that default.

#include <map>
#include <string>
#include <string_view>

#include "game/text/messages.h"

namespace sif {

class UiScreen;
class Vfs;

class Localizer {
public:
    // `lang`: "en", "cn", "tw" or "" (Japanese).
    bool Load(const Vfs& vfs, const std::string& lang, std::string* error = nullptr);
    // The selected language's message `id` (with Japanese fallback).
    const std::string& Text(int id) const { return messages_.Get(id); }
    // The localized message whose Japanese text is `japanese`; false when no message has that text.
    bool ForDefault(std::string_view japanese, std::string& out) const;
    size_t Size() const { return messages_.Size(); }

private:
    Messages messages_;
    std::map<std::string, int, std::less<>> by_japanese_;
};

// Convenience for a loaded screen: replaces every ui_text default the localizer recognizes. Returns false (leaving
// the screen's defaults in place) when message.pac cannot be read.
bool LocalizeScreen(UiScreen& ui, const Vfs& vfs, const std::string& lang, std::string* error = nullptr);

}
