#pragma once
// The game's message text (message/message.pac; spec in docs/formats/message.md): one package per language, each an
// ARC of 816 messages numbered by entry index (e.g. 6 "New Player", 9 "OK", 10 "Cancel").

#include <string>
#include <vector>

namespace sif {

class Vfs;

class Messages {
public:
    // `language`: 0 Japanese, 1 English, 2 simplified Chinese, 3 traditional Chinese (the package order).
    bool Load(const Vfs& vfs, int language, std::string* error = nullptr);
    // Message `id` with its escapes resolved ("\n" a line break, "\uXXXX" the character); "" if there is none.
    const std::string& Get(int id) const;
    size_t Size() const { return texts_.size(); }

private:
    std::vector<std::string> texts_;
};

}
