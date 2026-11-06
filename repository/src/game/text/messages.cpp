#include "game/text/messages.h"

#include <cstdint>
#include <cstring>
#include <span>

#include "engine/fs/pac.h"
#include "engine/fs/vfs.h"

namespace sif {
namespace {

bool Fail(std::string* error, std::string text) {
    if (error) *error = std::move(text);
    return false;
}

uint32_t U32(std::span<const uint8_t> d, size_t off) {
    uint32_t v = 0;
    if (off + 4 <= d.size()) std::memcpy(&v, &d[off], 4);
    return v;
}

uint16_t U16(std::span<const uint8_t> d, size_t off) {
    uint16_t v = 0;
    if (off + 2 <= d.size()) std::memcpy(&v, &d[off], 2);
    return v;
}

// The blocks of an ARC here are u32 size, u32 4, then `size` bytes of content (the generic ARC reader takes the 4 for
// a header length and starts 4 bytes early, cutting each content short). message.pac is an ARC of the language
// packages; a language package's content is an ARC of the messages.
bool Blocks(std::span<const uint8_t> arc, std::vector<std::span<const uint8_t>>& out, std::string* error) {
    out.clear();
    std::vector<pac::ArcEntry> entries;
    if (!pac::ParseArc(arc, entries, error)) return false;
    for (const pac::ArcEntry& e : entries) {
        const size_t size = U32(arc, e.block);
        if (e.block + 8 + size > arc.size()) return Fail(error, "message block outside its archive");
        out.push_back(arc.subspan(e.block + 8, size));
    }
    return true;
}

void AppendUtf8(std::string& s, uint32_t c) {
    if (c < 0x80) {
        s += char(c);
    } else if (c < 0x800) {
        s += char(0xC0 | (c >> 6));
        s += char(0x80 | (c & 0x3F));
    } else {
        s += char(0xE0 | (c >> 12));
        s += char(0x80 | ((c >> 6) & 0x3F));
        s += char(0x80 | (c & 0x3F));
    }
}

// The texts keep "\n" and "　" as written in the source
std::string Unescape(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '\\' && i + 1 < in.size() && in[i + 1] == 'n') {
            out += '\n';
            ++i;
        } else if (in[i] == '\\' && i + 5 < in.size() && in[i + 1] == 'u') {
            char* end = nullptr;
            const std::string hex = in.substr(i + 2, 4);
            const unsigned long c = std::strtoul(hex.c_str(), &end, 16);
            if (end == hex.c_str() + 4) {
                AppendUtf8(out, uint32_t(c));
                i += 5;
            } else {
                out += in[i];
            }
        } else {
            out += in[i];
        }
    }
    return out;
}

bool Untranslated(const std::string& text) {
    return text.empty() || text.ends_with("Not Defined");
}

}  // namespace

bool Messages::Load(const Vfs& vfs, int language, std::string* error) {
    messages_.clear();
    japanese_.clear();
    std::vector<uint8_t> file;
    if (!vfs.Read("message/message.pac", file, error)) return false;
    std::vector<std::span<const uint8_t>> languages;
    if (!Blocks(file, languages, error)) return false;
    if (language < 0 || size_t(language) >= languages.size()) return Fail(error, "no such message language");

    // a message: u32 kind, u16 text offset, u16 rows, u16 characters, u16 UTF-8 bytes, u32 0, then per-character style
    // rows and records, then the text (NUL-terminated UTF-8) at the text offset
    const auto parse = [&](std::span<const uint8_t> package, std::vector<Message>& out) {
        out.clear();
        std::vector<std::span<const uint8_t>> raw;
        if (!Blocks(package, raw, error)) return false;
        out.reserve(raw.size());
        for (const std::span<const uint8_t> m : raw) {
            Message msg;
            if (m.size() >= 16) {
                MessageFormat& f = msg.format;
                f.kind = U32(m, 0);
                const size_t text_offset = U16(m, 4);
                f.rows = U16(m, 6);
                f.characters = U16(m, 8);
                f.bytes = U16(m, 10);
                if (text_offset < m.size()) {
                    const char* s = reinterpret_cast<const char*>(&m[text_offset]);
                    msg.text = Unescape(std::string(s, strnlen(s, m.size() - text_offset)));
                }
                // Each row is (u16 flags, u16 offset); the offset is relative to the row and points at one 8-byte
                // record, or at a 16-byte pair when the flags are 2. Rows that point at the text have no record.
                if (f.rows <= (m.size() - 16) / 4) {
                    f.layout.resize(f.rows);
                    for (size_t k = 0; k < f.rows; ++k) {
                        const size_t row = 16 + 4 * k;
                        MessageRow& r = f.layout[k];
                        r.flags = U16(m, row);
                        const size_t target = row + U16(m, row + 2);
                        if (target >= text_offset || target + 8 > m.size()) continue;
                        r.record[0] = {U32(m, target), U32(m, target + 4)};
                        r.records = 1;
                        if (r.flags == 2 && target + 16 <= text_offset) {
                            r.record[1] = {U32(m, target + 8), U32(m, target + 12)};
                            r.records = 2;
                        }
                    }
                }
            }
            out.push_back(std::move(msg));
        }
        return true;
    };

    if (!parse(languages[0], japanese_)) return false;
    if (language == 0) {
        messages_ = japanese_;
    } else if (!parse(languages[size_t(language)], messages_)) {
        return false;
    }
    return true;
}

const std::string& Messages::Get(int id) const {
    static const std::string none;
    if (id < 0 || size_t(id) >= messages_.size()) return none;
    const std::string& text = messages_[size_t(id)].text;
    if (!Untranslated(text)) return text;
    if (size_t(id) < japanese_.size() && !japanese_[size_t(id)].text.empty()) return japanese_[size_t(id)].text;
    return text;
}

const std::string& Messages::Japanese(int id) const {
    static const std::string none;
    return id >= 0 && size_t(id) < japanese_.size() ? japanese_[size_t(id)].text : none;
}

const MessageFormat& Messages::Format(int id) const {
    static const MessageFormat none;
    return id >= 0 && size_t(id) < messages_.size() ? messages_[size_t(id)].format : none;
}

}
