#pragma once
// The game's message text (message/message.pac; spec in docs/formats/message.md): one package per language, each an
// ARC of 816 messages numbered by entry index (e.g. 6 "New Player", 9 "OK", 10 "Cancel").

#include <cstdint>
#include <string>
#include <vector>

namespace sif {

class Vfs;

// One per-character style record: u32 type, u32 value. Type 0 is a colour (ARGB), type 1 a size hint (9, 16 or 24
// matches the font size of the message), type 2 appears only in the kind-1 single-symbol messages.
struct MessageRecord {
    uint32_t type = 0;
    uint32_t value = 0;
};

// A character row: `flags` 0 reuses a shared record (or has none when it points at the text), 1 owns one record and
// 2 owns a pair of records. `records` is how many of `record` are present.
struct MessageRow {
    uint16_t flags = 0;
    uint8_t records = 0;
    MessageRecord record[2] = {};
};

// The message header and its per-character rows. `rows` counts the style rows, `characters` the text characters
// (line breaks have no row, so Japanese 814 is 16 rows for 17 characters) and `bytes` its UTF-8 length.
struct MessageFormat {
    uint32_t kind = 0;  // 0 text, 1 a single-symbol fragment
    uint16_t rows = 0, characters = 0, bytes = 0;
    std::vector<MessageRow> layout;
};

class Messages {
public:
    // `language`: 0 Japanese, 1 English, 2 simplified Chinese, 3 traditional Chinese (the package order). Japanese is
    // always read as well, so Get() can fall back to it for empty and "… Not Defined" entries.
    bool Load(const Vfs& vfs, int language, std::string* error = nullptr);
    // Message `id` in the selected language with its escapes resolved ("\n" a line break, "\uXXXX" the character).
    // Untranslated entries (empty, or ending in "Not Defined") fall back to Japanese. "" if there is none.
    const std::string& Get(int id) const;
    // The Japanese text of `id`, whatever language was selected; used to map a UI default to its message id.
    const std::string& Japanese(int id) const;
    // The header counts and per-row style records of `id`; empty (rows 0) out of range.
    const MessageFormat& Format(int id) const;
    size_t Size() const { return messages_.size(); }

private:
    struct Message {
        std::string text;
        MessageFormat format;
    };
    std::vector<Message> messages_;  // the selected language
    std::vector<Message> japanese_;
};

}
