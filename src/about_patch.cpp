#include "about_patch.h"
#include <regex>

namespace {
constexpr std::string_view begin_marker = "/* vivaldi-plus-ui:start */";
constexpr std::string_view end_marker = "/* vivaldi-plus-ui:end */";

std::string RegexLiteral(std::string_view text) {
    std::string result;
    for (const char c : text) {
        if (std::string_view("\\.^$|()[]{}*+?").find(c) != std::string_view::npos) result += '\\';
        result += c;
    }
    return result;
}

std::string JavascriptString(std::string_view text) {
    constexpr char hex[] = "0123456789abcdef";
    std::string result = "\"";
    for (size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c == 0xe2 && i + 2 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0x80 &&
            (static_cast<unsigned char>(text[i + 2]) == 0xa8 || static_cast<unsigned char>(text[i + 2]) == 0xa9)) {
            result += static_cast<unsigned char>(text[i + 2]) == 0xa8 ? "\\u2028" : "\\u2029";
            i += 2;
        } else if (c == '"' || c == '\\') {
            result += '\\'; result += static_cast<char>(c);
        } else if (c < 32 || c == '<' || c == '>' || c == '&') {
            result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15];
        } else result += static_cast<char>(c);
    }
    return result + '"';
}

std::string PercentEncode(std::string_view text) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (const unsigned char c : text) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') result += static_cast<char>(c);
        else { result += '%'; result += hex[c >> 4]; result += hex[c & 15]; }
    }
    return result;
}

bool ValidEndpoint(std::string_view url) {
    for (int port = 16877; port <= 16887; ++port)
        if (url == "http://127.0.0.1:" + std::to_string(port) + "/settings.html") return true;
    return false;
}

bool UniqueMatch(const std::string& source, const std::regex& pattern, std::smatch& match) {
    auto item = std::sregex_iterator(source.begin(), source.end(), pattern);
    if (item == std::sregex_iterator()) return false;
    match = *item;
    return ++item == std::sregex_iterator();
}
}

bool RemoveAboutUi(std::string_view input, std::string& output, std::string& error) {
    const auto fail = [&](const char* message) { error = message; return false; };
    if (input.size() > 64 * 1024 * 1024 || input.find('\0') != std::string_view::npos) return fail("Invalid UI bundle size/content");
    std::string source(input);
    const auto marker = source.find(begin_marker);
    if (marker != std::string::npos) {
        const auto end = source.find(end_marker, marker + begin_marker.size());
        if (end == std::string::npos || source.find(begin_marker, marker + begin_marker.size()) != std::string::npos ||
            source.find(end_marker) != end || source.find(end_marker, end + end_marker.size()) != std::string::npos)
            return fail("Malformed or repeated Vivaldi++ integration markers");
        source.erase(marker, end + end_marker.size() - marker);
    } else if (source.find(end_marker) != std::string::npos) return fail("Unmatched integration marker");
    output = std::move(source); error.clear(); return true;
}

bool PatchAboutUi(std::string_view input, std::string_view settings_url,
    std::string_view version, std::string& output, std::string& error) {
    const auto fail = [&](const char* message) { error = message; return false; };
    if (!ValidEndpoint(settings_url)) return fail("Settings URL must use the Vivaldi++ loopback endpoint");
    if (version.empty() || version.size() > 128) return fail("Invalid version");
    std::string source;
    if (!RemoveAboutUi(input, source, error)) return false;

    static const std::regex card_pattern(R"rx(\(0,([A-Za-z_$][A-Za-z0-9_$]*)\.jsxs\)\("div",\{className:"card system-build-card",children:\[([A-Za-z_$][A-Za-z0-9_$]*),([A-Za-z_$][A-Za-z0-9_$]*)\]\}\))rx");
    std::smatch card;
    if (!UniqueMatch(source, card_pattern, card)) return fail("Expected one native system-and-build card; unsupported Vivaldi UI bundle");
    const size_t position = static_cast<size_t>(card.position());
    const auto start = position > 6000 ? position - 6000 : 0;
    const std::string component = source.substr(start, position - start);
    const std::string runtime = card[1].str();
    const auto content_pattern = std::regex("(^|[^A-Za-z0-9_$])" + RegexLiteral(card[3].str()) + "=\\(0," + RegexLiteral(runtime) + R"rx(\.jsxs\)\("div",\{className:"card-content",children:\[)rx");
    static const std::regex translation_pattern(R"rx(\(0,([A-Za-z_$][A-Za-z0-9_$]*)\.Z\)\("title","Version"\))rx");
    static const std::regex navigation_pattern(R"rx(onClick:\(\)=>([A-Za-z_$][A-Za-z0-9_$]*)\("https://vivaldi\.com/bugreport/"\))rx");
    std::smatch content, translation, navigation;
    if (!UniqueMatch(component, content_pattern, content) || !UniqueMatch(component, translation_pattern, translation))
        return fail("Native build-card content or translation callback is missing/ambiguous");
    if (!UniqueMatch(source, navigation_pattern, navigation)) return fail("Native about-page navigation callback is missing/ambiguous");
    const size_t navigation_at = static_cast<size_t>(navigation.position());
    if ((position > navigation_at ? position - navigation_at : navigation_at - position) > 25000)
        return fail("Native navigation callback is outside the about component");
    const std::string navigate = navigation[1].str();
    const std::regex declaration_pattern("(?:const|let|var)\\s+" + RegexLiteral(navigate) + "\\s*=");
    if (!std::regex_search(source.begin(), source.begin() + position, declaration_pattern))
        return fail("Native navigation callback is not declared before the build card");
    const std::string jsx = "(0," + runtime + ".jsx)";
    const std::string jsxs = "(0," + runtime + ".jsxs)";
    const auto url = JavascriptString(std::string(settings_url) + "?version=" + PercentEncode(version));
    const std::string translate = "(0," + translation[1].str() + ".Z)";
    const auto row = std::string(begin_marker) + jsxs + "(\"div\",{id:\"vivaldi-plus-about\",className:\"info-row\",children:[" +
        jsx + "(\"span\",{className:\"label\",children:\"Vivaldi++\"})," +
        jsxs + "(\"span\",{className:\"value\",style:{display:\"inline-flex\",alignItems:\"baseline\",gap:\"8px\"},children:[" +
        jsx + "(\"span\",{children:" + JavascriptString(version) + "})," +
        jsx + "(\"a\",{id:\"vivaldi-plus-settings\",className:\"link-button\",style:{padding:0,margin:0,font:\"inherit\",lineHeight:\"inherit\"},href:" + url +
        ",target:\"_blank\",rel:\"noopener\",onClick:e=>{e.preventDefault();" + navigate + "(" + url +
        ")},children:" + translate + "(\"Settings\")})]})]})," + std::string(end_marker);
    source.insert(start + static_cast<size_t>(content.position() + content.length()), row);
    output = std::move(source);
    error.clear();
    return true;
}
