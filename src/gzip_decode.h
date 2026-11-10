#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Decode one gzip member. Input and output are bounded to 64 MiB of decoded
// content; failures leave output unchanged. No external compression library.
bool DecodeGzip(std::span<const uint8_t> input, size_t max_output,
    std::vector<uint8_t>& output, std::string& error);

// Encode uncompressed RFC 1951 stored blocks inside a gzip member. The optional
// ASCII comment can identify owned resources without extra on-disk metadata.
bool EncodeGzipStored(std::span<const uint8_t> input, std::string_view comment,
    std::vector<uint8_t>& output, std::string& error);
