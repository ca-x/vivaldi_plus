#include "gzip_decode.h"
#include <array>
#include <algorithm>

namespace {
constexpr size_t kOutputLimit = 64 * 1024 * 1024;

constexpr auto CrcTable() {
    std::array<uint32_t, 256> table = {};
    for (uint32_t i = 0; i < table.size(); ++i) {
        uint32_t value = i;
        for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0u);
        table[i] = value;
    }
    return table;
}

uint32_t Crc32(std::span<const uint8_t> bytes) {
    static constexpr auto table = CrcTable();
    uint32_t value = 0xffffffffu;
    for (const auto byte : bytes) value = table[(value ^ byte) & 255] ^ (value >> 8);
    return value ^ 0xffffffffu;
}

uint32_t Little32(std::span<const uint8_t> bytes, size_t at) {
    return uint32_t(bytes[at]) | (uint32_t(bytes[at + 1]) << 8) |
        (uint32_t(bytes[at + 2]) << 16) | (uint32_t(bytes[at + 3]) << 24);
}

class Bits {
public:
    explicit Bits(std::span<const uint8_t> bytes) : bytes_(bytes) {}
    bool Read(unsigned count, uint32_t& value) {
        if (count > 24 || count > bytes_.size() * 8 - position_) return false;
        value = 0;
        for (unsigned bit = 0; bit < count; ++bit, ++position_)
            value |= uint32_t((bytes_[position_ / 8] >> (position_ % 8)) & 1u) << bit;
        return true;
    }
    void Align() { position_ = (position_ + 7) & ~size_t(7); }
    size_t Consumed() const { return (position_ + 7) / 8; }
    bool Take(size_t size, std::span<const uint8_t>& result) {
        if (position_ % 8 || size > bytes_.size() - position_ / 8) return false;
        result = bytes_.subspan(position_ / 8, size);
        position_ += size * 8;
        return true;
    }
private:
    std::span<const uint8_t> bytes_;
    size_t position_ = 0;
};

class Huffman {
public:
    bool Build(std::span<const uint8_t> lengths, bool code_lengths, bool allow_empty, std::string& error) {
        counts_.fill(0);
        max_ = 0;
        if (lengths.size() > symbols_.size()) { error = "Too many Huffman symbols"; return false; }
        for (const auto length : lengths) {
            if (length > 15) { error = "Invalid Huffman code length"; return false; }
            ++counts_[length];
            max_ = std::max(max_, unsigned(length));
        }
        if (max_ == 0) {
            if (allow_empty) return true;
            error = "Empty Huffman tree"; return false;
        }
        int left = 1;
        for (unsigned length = 1; length <= 15; ++length) {
            left = (left << 1) - static_cast<int>(counts_[length]);
            if (left < 0) { error = "Oversubscribed Huffman tree"; return false; }
        }
        // RFC 1951 permits one 1-bit literal/distance code. An unused distance
        // alphabet can also be empty. Code-length alphabets must be complete.
        if (left && (code_lengths || max_ != 1 || counts_[1] != 1)) {
            error = "Incomplete Huffman tree"; return false;
        }
        std::array<unsigned, 16> offset = {};
        for (unsigned length = 1; length < 15; ++length) offset[length + 1] = offset[length] + counts_[length];
        for (size_t symbol = 0; symbol < lengths.size(); ++symbol)
            if (lengths[symbol]) symbols_[offset[lengths[symbol]]++] = static_cast<uint16_t>(symbol);
        return true;
    }

    bool Decode(Bits& bits, unsigned& symbol, std::string& error) const {
        unsigned code = 0, first = 0, index = 0;
        for (unsigned length = 1; length <= max_; ++length) {
            uint32_t bit = 0;
            if (!bits.Read(1, bit)) { error = "Truncated Huffman bitstream"; return false; }
            // Huffman code bits arrive in canonical MSB order, although the
            // containing DEFLATE bytes and extra-bit integers are LSB first.
            code |= bit;
            const auto count = counts_[length];
            if (code >= first && code - first < count) { symbol = symbols_[index + code - first]; return true; }
            index += count;
            first = (first + count) << 1;
            code <<= 1;
        }
        error = "Invalid Huffman code"; return false;
    }
private:
    std::array<unsigned, 16> counts_ = {};
    std::array<uint16_t, 288> symbols_ = {};
    unsigned max_ = 0;
};

bool ReadDynamic(Bits& bits, Huffman& literals, Huffman& distances, std::string& error) {
    uint32_t hlit = 0, hdist = 0, hclen = 0;
    if (!bits.Read(5, hlit) || !bits.Read(5, hdist) || !bits.Read(4, hclen)) { error = "Truncated dynamic header"; return false; }
    hlit += 257; hdist += 1; hclen += 4;
    if (hlit > 286) { error = "Reserved literal alphabet size"; return false; }
    constexpr unsigned order[] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    std::array<uint8_t, 19> code_lengths = {};
    for (size_t i = 0; i < hclen; ++i) {
        uint32_t length = 0;
        if (!bits.Read(3, length)) { error = "Truncated code-length alphabet"; return false; }
        code_lengths[order[i]] = static_cast<uint8_t>(length);
    }
    Huffman codes;
    if (!codes.Build(code_lengths, true, false, error)) return false;
    std::array<uint8_t, 318> lengths = {};
    size_t used = 0;
    const size_t total = hlit + hdist;
    while (used < total) {
        unsigned symbol = 0;
        if (!codes.Decode(bits, symbol, error)) return false;
        if (symbol < 16) { lengths[used++] = static_cast<uint8_t>(symbol); continue; }
        uint32_t extra = 0;
        const unsigned width = symbol == 16 ? 2 : symbol == 17 ? 3 : 7;
        if (symbol > 18 || (symbol == 16 && !used)) { error = "Invalid code-length repeat"; return false; }
        if (!bits.Read(width, extra)) { error = "Truncated code-length repeat"; return false; }
        const size_t repeat = extra + (symbol == 18 ? 11 : 3);
        if (repeat > total - used) { error = "Code-length repeat exceeds alphabet"; return false; }
        const uint8_t length = symbol == 16 ? lengths[used - 1] : 0;
        for (size_t i = 0; i < repeat; ++i) lengths[used++] = length;
    }
    if (!lengths[256]) { error = "Literal alphabet has no end-of-block code"; return false; }
    return literals.Build(std::span(lengths).first(hlit), false, false, error) &&
        distances.Build(std::span(lengths).subspan(hlit, hdist), false, true, error);
}

bool ReadFixed(Huffman& literals, Huffman& distances, std::string& error) {
    std::array<uint8_t, 288> lengths = {};
    for (size_t i = 0; i < lengths.size(); ++i) lengths[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
    std::array<uint8_t, 32> distance_lengths;
    distance_lengths.fill(5);
    return literals.Build(lengths, false, false, error) && distances.Build(distance_lengths, false, false, error);
}

bool ReadCompressed(Bits& bits, const Huffman& literals, const Huffman& distances,
    size_t limit, std::vector<uint8_t>& output, std::string& error) {
    constexpr unsigned length_base[] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    constexpr unsigned length_extra[] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    constexpr unsigned distance_base[] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    constexpr unsigned distance_extra[] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    while (true) {
        unsigned symbol = 0;
        if (!literals.Decode(bits, symbol, error)) return false;
        if (symbol < 256) {
            if (output.size() == limit) { error = "Decoded data exceeds output limit"; return false; }
            output.push_back(static_cast<uint8_t>(symbol)); continue;
        }
        if (symbol == 256) return true;
        if (symbol > 285) { error = "Reserved literal/length code"; return false; }
        uint32_t extra = 0;
        if (!bits.Read(length_extra[symbol - 257], extra)) { error = "Truncated length"; return false; }
        const size_t length = length_base[symbol - 257] + extra;
        if (!distances.Decode(bits, symbol, error)) return false;
        if (symbol >= 30) { error = "Reserved distance code"; return false; }
        if (!bits.Read(distance_extra[symbol], extra)) { error = "Truncated distance"; return false; }
        const size_t distance = distance_base[symbol] + extra;
        if (distance > output.size()) { error = "Distance precedes the output window"; return false; }
        if (length > limit - output.size()) { error = "Decoded data exceeds output limit"; return false; }
        for (size_t i = 0; i < length; ++i) output.push_back(output[output.size() - distance]);
    }
}

bool Inflate(std::span<const uint8_t> data, size_t limit, std::vector<uint8_t>& output, std::string& error) {
    Bits bits(data);
    uint32_t final = 0;
    do {
        uint32_t type = 0;
        if (!bits.Read(1, final) || !bits.Read(2, type)) { error = "Truncated block header"; return false; }
        if (type == 0) {
            bits.Align();
            uint32_t length = 0, complement = 0;
            if (!bits.Read(16, length) || !bits.Read(16, complement) || (length ^ complement) != 0xffff) { error = "Invalid stored block length"; return false; }
            std::span<const uint8_t> bytes;
            if (length > limit - output.size() || !bits.Take(length, bytes)) { error = "Truncated or oversized stored block"; return false; }
            output.insert(output.end(), bytes.begin(), bytes.end());
        } else if (type == 1 || type == 2) {
            Huffman literals, distances;
            if (!(type == 1 ? ReadFixed(literals, distances, error) : ReadDynamic(bits, literals, distances, error)) ||
                !ReadCompressed(bits, literals, distances, limit, output, error)) return false;
        } else { error = "Reserved block type"; return false; }
    } while (!final);
    if (bits.Consumed() != data.size()) { error = "Trailing data after final deflate block"; return false; }
    return true;
}
}

bool DecodeGzip(std::span<const uint8_t> input, size_t max_output,
    std::vector<uint8_t>& output, std::string& error) {
    const auto fail = [&](const char* message) { error = message; return false; };
    if (max_output > kOutputLimit || input.size() > kOutputLimit + kOutputLimit / 32 + 65536) return fail("Gzip input/output limit exceeds 64 MiB budget");
    if (input.size() < 18 || input[0] != 0x1f || input[1] != 0x8b || input[2] != 8 || (input[3] & 0xe0)) return fail("Invalid gzip header");
    const size_t footer = input.size() - 8;
    size_t at = 10;
    if (input[3] & 4) {
        if (footer - at < 2) return fail("Truncated gzip extra field");
        const size_t size = unsigned(input[at]) | (unsigned(input[at + 1]) << 8);
        at += 2;
        if (size > footer - at) return fail("Truncated gzip extra data");
        at += size;
    }
    for (const unsigned flag : {8u, 16u}) if (input[3] & flag) {
        while (at < footer && input[at]) ++at;
        if (at == footer) return fail("Unterminated gzip name/comment");
        ++at;
    }
    if (input[3] & 2) {
        if (footer - at < 2) return fail("Truncated gzip header CRC");
        const auto expected = unsigned(input[at]) | (unsigned(input[at + 1]) << 8);
        if ((Crc32(input.first(at)) & 0xffffu) != expected) return fail("Gzip header CRC mismatch");
        at += 2;
    }
    const uint32_t expected_size = Little32(input, footer + 4);
    if (expected_size > max_output) return fail("Gzip size exceeds output limit");
    std::vector<uint8_t> candidate;
    candidate.reserve(expected_size);
    if (!Inflate(input.subspan(at, footer - at), max_output, candidate, error)) return false;
    if (candidate.size() != expected_size) return fail("Gzip decoded size mismatch");
    if (Crc32(candidate) != Little32(input, footer)) return fail("Gzip data CRC mismatch");
    output = std::move(candidate);
    error.clear();
    return true;
}

bool EncodeGzipStored(std::span<const uint8_t> input, std::string_view comment,
    std::vector<uint8_t>& output, std::string& error) {
    if (input.size() > kOutputLimit || comment.size() >= 128 ||
        std::any_of(comment.begin(), comment.end(), [](unsigned char c) { return c == 0 || c > 127; })) {
        error = "Stored gzip input/comment exceeds its supported bounds"; return false;
    }
    std::vector<uint8_t> candidate = {31,139,8,static_cast<uint8_t>(comment.empty() ? 0 : 16),0,0,0,0,0,255};
    candidate.reserve(input.size() + (input.size() / 65535 + 1) * 5 + comment.size() + 19);
    if (!comment.empty()) { candidate.insert(candidate.end(), comment.begin(), comment.end()); candidate.push_back(0); }
    size_t at = 0;
    do {
        const size_t size = std::min(size_t(65535), input.size() - at);
        const auto complement = static_cast<uint16_t>(~static_cast<uint16_t>(size));
        candidate.push_back(at + size == input.size() ? 1 : 0);
        candidate.push_back(static_cast<uint8_t>(size)); candidate.push_back(static_cast<uint8_t>(size >> 8));
        candidate.push_back(static_cast<uint8_t>(complement)); candidate.push_back(static_cast<uint8_t>(complement >> 8));
        if (size) candidate.insert(candidate.end(), input.begin() + at, input.begin() + at + size);
        at += size;
    } while (at < input.size());
    for (const auto value : {Crc32(input), static_cast<uint32_t>(input.size())})
        for (unsigned i = 0; i < 4; ++i) candidate.push_back(static_cast<uint8_t>(value >> (i * 8)));
    output = std::move(candidate);
    error.clear();
    return true;
}
