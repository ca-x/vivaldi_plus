#include "chrome_about.h"
#include "gzip_decode.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace {
constexpr std::string_view gzip_comment = "vivaldi-plus-chrome-entry";
constexpr std::string_view begin_marker = "<!-- vivaldi-plus-chrome:start -->";
constexpr std::string_view end_marker = "<!-- vivaldi-plus-chrome:end -->";
constexpr std::string_view about_section = "<settings-section page-title=\"$i18n{aboutPageTitle}\">";
constexpr std::string_view help_row = "<cr-link-row class=\"hr\" id=\"help\"";

uint32_t Read32(std::string_view bytes, size_t at) {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= static_cast<uint32_t>(static_cast<unsigned char>(bytes[at + i])) << (8 * i);
    return value;
}
uint16_t Read16(std::string_view bytes, size_t at) {
    return static_cast<uint16_t>(static_cast<unsigned char>(bytes[at]) |
        (static_cast<unsigned char>(bytes[at + 1]) << 8));
}
void Write32(std::string& bytes, size_t at, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[at + i] = static_cast<char>(value >> (i * 8));
}
bool IsGzip(std::string_view bytes) {
    return bytes.size() >= 18 && static_cast<unsigned char>(bytes[0]) == 0x1f && static_cast<unsigned char>(bytes[1]) == 0x8b;
}
std::span<const uint8_t> Bytes(std::string_view value) {
    return {reinterpret_cast<const uint8_t*>(value.data()), value.size()};
}
bool Endpoint(std::string_view url) {
    for (int port = 16877; port <= 16887; ++port)
        if (url == "http://127.0.0.1:" + std::to_string(port) + "/settings.html") return true;
    return false;
}
bool StripEntry(std::string& text, std::string& error) {
    const auto start = text.find(begin_marker), end = text.find(end_marker);
    if (start == std::string::npos && end == std::string::npos) return true;
    if (start == std::string::npos || end == std::string::npos || end < start ||
        text.find(begin_marker, start + begin_marker.size()) != std::string::npos ||
        text.find(end_marker, end + end_marker.size()) != std::string::npos) {
        error = "Malformed Chrome settings entry markers"; return false;
    }
    text.erase(start, end + end_marker.size() - start);
    return true;
}
}

bool PatchChromeAbout(std::string_view pack, std::string_view settings_url,
    std::string_view version, bool enabled, std::string& output, std::string& error) {
    const auto fail = [&](const char* reason) { error = reason; return false; };
    if (enabled && (!Endpoint(settings_url) || version.empty() || version.size() > 64 ||
        !std::all_of(version.begin(), version.end(), [](char c) { return (c >= '0' && c <= '9') || c == '.'; })))
        return fail("Invalid Chrome settings URL or version");
    // Our gzip comment permits the default-off path to avoid inflating unrelated resources.
    if (!enabled && pack.find(gzip_comment) == std::string::npos) { output.assign(pack); error.clear(); return true; }
    if (pack.size() < 18 || pack.size() > 64 * 1024 * 1024 || Read32(pack, 0) != 5)
        return fail("Unsupported Chrome resource pack format");
    const size_t count = Read16(pack, 8), aliases = Read16(pack, 10);
    const size_t data_start = 12 + (count + 1) * 6 + aliases * 4;
    if (data_start > pack.size()) return fail("Truncated Chrome resource index");
    std::vector<uint32_t> offsets(count + 1);
    int previous_id = -1;
    for (size_t i = 0; i <= count; ++i) {
        offsets[i] = Read32(pack, 12 + i * 6 + 2);
        if (offsets[i] < data_start || offsets[i] > pack.size() || (i && offsets[i] < offsets[i - 1]))
            return fail("Invalid Chrome resource offsets");
        if (i < count) {
            const int id = Read16(pack, 12 + i * 6);
            if (id <= previous_id) return fail("Unsorted Chrome resource ids");
            previous_id = id;
        }
    }
    if (offsets.back() != pack.size()) return fail("Trailing Chrome resource bytes");
    for (size_t i = 0; i < aliases; ++i)
        if (Read16(pack, 12 + (count + 1) * 6 + i * 4 + 2) >= count) return fail("Invalid Chrome resource alias");

    size_t selected = count;
    std::string original;
    for (size_t i = 0; i < count; ++i) {
        const auto resource = pack.substr(offsets[i], offsets[i + 1] - offsets[i]);
        if (!enabled && resource.find(gzip_comment) == std::string_view::npos) continue;
        std::string text;
        if (IsGzip(resource)) {
            const auto decoded_size = Read32(resource, resource.size() - 4);
            if (decoded_size > 16 * 1024 * 1024) continue;
            std::vector<uint8_t> decoded;
            std::string decode_error;
            if (!DecodeGzip(Bytes(resource), 16 * 1024 * 1024, decoded, decode_error)) {
                if (resource.find(gzip_comment) != std::string_view::npos) return fail("Modified Chrome resource is corrupt");
                continue;
            }
            if (decoded.empty()) continue;
            text.assign(reinterpret_cast<const char*>(decoded.data()), decoded.size());
        } else text.assign(resource);
        if (text.find("settings-about-page") == std::string::npos || text.find(about_section) == std::string::npos) continue;
        if (selected != count) return fail("Multiple Chrome about templates");
        selected = i; original = std::move(text);
    }
    if (selected == count) return fail("Chrome settings/help template unavailable");
    std::string text = original;
    if (!StripEntry(text, error)) return false;
    if (enabled) {
        const auto section = text.find(about_section);
        const auto section_end = text.find("</settings-section>", section);
        const auto at = text.find(help_row, section);
        if (section_end == std::string::npos || at == std::string::npos || at > section_end ||
            text.find(about_section, section + about_section.size()) != std::string::npos ||
            text.find("$i18n{settings}") == std::string::npos) return fail("Unsupported Chrome about layout or localization");
        const auto entry = std::string(begin_marker) + "<div class=\"cr-row hr\" id=\"vivaldi-plus-chrome\"><div class=\"flex\">Vivaldi++<div class=\"secondary\">" +
            std::string(version) + "</div></div><a id=\"vivaldi-plus-settings\" href=\"" + std::string(settings_url) + "?version=" + std::string(version) +
            "\" target=\"_blank\" rel=\"noopener\">$i18n{settings}</a></div>" + std::string(end_marker);
        text.insert(at, entry);
    }
    if (text == original) { output.assign(pack); error.clear(); return true; }
    std::vector<uint8_t> encoded;
    if (!EncodeGzipStored(Bytes(text), enabled ? gzip_comment : std::string_view{}, encoded, error)) return false;
    const int64_t delta = static_cast<int64_t>(encoded.size()) - (offsets[selected + 1] - offsets[selected]);
    const int64_t new_size = static_cast<int64_t>(pack.size()) + delta;
    if (new_size < 0 || new_size > 64 * 1024 * 1024) return fail("Modified Chrome pack exceeds size limit");
    std::string result;
    result.reserve(static_cast<size_t>(new_size));
    result.append(pack.substr(0, offsets[selected]));
    result.append(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    result.append(pack.substr(offsets[selected + 1]));
    for (size_t i = selected + 1; i <= count; ++i)
        Write32(result, 12 + i * 6 + 2, static_cast<uint32_t>(static_cast<int64_t>(offsets[i]) + delta));
    output = std::move(result); error.clear(); return true;
}
