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

}

bool Messages::Load(const Vfs& vfs, int language, std::string* error) {
    texts_.clear();
    std::vector<uint8_t> file;
    if (!vfs.Read("message/message.pac", file, error)) return false;
    std::vector<std::span<const uint8_t>> languages, messages;
    if (!Blocks(file, languages, error)) return false;
    if (language < 0 || size_t(language) >= languages.size()) return Fail(error, "no such message language");
    if (!Blocks(languages[size_t(language)], messages, error)) return false;
    // a message: u32 kind, u16 text offset, u16 x 3 character count, u32 0, then per-character style rows and records,
    // then the text (NUL-terminated UTF-8) at the text offset
    for (const std::span<const uint8_t> m : messages) {
        std::string text;
        if (m.size() >= 16) {
            size_t base = 0;
            std::memcpy(&base, &m[4], 2);
            if (base < m.size()) {
                const char* s = reinterpret_cast<const char*>(&m[base]);
                text = Unescape(std::string(s, strnlen(s, m.size() - base)));
            }
        }
        texts_.push_back(std::move(text));
    }
    return true;
}

const std::string& Messages::Get(int id) const {
    static const std::string none;
    return id >= 0 && size_t(id) < texts_.size() ? texts_[size_t(id)] : none;
}

}
