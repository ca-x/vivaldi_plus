#include "../src/gzip_decode.h"
#include "fixtures/gzip_cases.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>

namespace {
using Bytes = std::vector<uint8_t>;
void Check(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
Bytes Text(const std::string& value) { return {value.begin(), value.end()}; }
uint32_t Crc(std::span<const uint8_t> bytes) {
    uint32_t value = 0xffffffffu;
    for (const auto byte : bytes) {
        value ^= byte;
        for (int i = 0; i < 8; ++i) value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0u);
    }
    return value ^ 0xffffffffu;
}
void Append32(Bytes& output, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) output.push_back(static_cast<uint8_t>(value >> (i * 8)));
}
Bytes Wrap(std::span<const uint8_t> deflate, std::span<const uint8_t> expected) {
    Bytes gzip = {31,139,8,0,0,0,0,0,0,255};
    gzip.insert(gzip.end(), deflate.begin(), deflate.end());
    Append32(gzip, Crc(expected)); Append32(gzip, static_cast<uint32_t>(expected.size()));
    return gzip;
}
struct Writer {
    Bytes bytes;
    size_t bits = 0;
    void Put(uint32_t value, unsigned count) {
        for (unsigned i = 0; i < count; ++i, ++bits) {
            if (bits / 8 == bytes.size()) bytes.push_back(0);
            bytes[bits / 8] |= static_cast<uint8_t>(((value >> i) & 1) << (bits % 8));
        }
    }
    void Code(unsigned value, unsigned count) {
        for (unsigned i = count; i > 0; --i) Put((value >> (i - 1)) & 1, 1);
    }
    void Align() { while (bits % 8) Put(0, 1); }
    void Fixed(unsigned symbol) {
        if (symbol < 144) Code(0x30 + symbol, 8);
        else if (symbol < 256) Code(0x190 + symbol - 144, 9);
        else if (symbol < 280) Code(symbol - 256, 7);
        else Code(0xc0 + symbol - 280, 8);
    }
};
void Good(std::span<const uint8_t> gzip, const Bytes& expected, size_t limit = 64 * 1024 * 1024) {
    Bytes output = {9,8,7}; std::string error;
    if (!DecodeGzip(gzip, limit, output, error)) { std::cerr << error << '\n'; Check(false, "Valid gzip was rejected"); }
    Check(output == expected && error.empty(), "Decoded data differs from Node zlib/known payload");
}
void Bad(std::span<const uint8_t> gzip, const char* category = "", size_t limit = 64 * 1024 * 1024) {
    const Bytes sentinel = {9,8,7}; Bytes output = sentinel; std::string error;
    Check(!DecodeGzip(gzip, limit, output, error), "Corrupt/oversized gzip was accepted");
    Check(output == sentinel && !error.empty(), "Failure changed output or did not explain the error");
    if (error.find(category) == std::string::npos) { std::cerr << error << '\n'; Check(false, "Wrong corruption failure category"); }
}
Writer Dynamic(const Bytes& lengths, unsigned literals = 257, unsigned distances = 1) {
    Writer writer;
    writer.Put(1, 1); writer.Put(2, 2);
    writer.Put(literals - 257, 5); writer.Put(distances - 1, 5); writer.Put(14, 4);
    constexpr unsigned order[] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1};
    for (const auto symbol : order) writer.Put(symbol == 0 ? 1 : symbol == 1 || symbol == 2 ? 2 : 0, 3);
    for (const auto length : lengths) {
        if (length == 0) writer.Code(0, 1);
        else writer.Code(length == 1 ? 2 : 3, 2);
    }
    return writer;
}

void BlockTests() {
    Writer overlap;
    overlap.Put(1,1); overlap.Put(1,2); overlap.Fixed('A'); overlap.Fixed(285); overlap.Code(0,5); overlap.Fixed(256);
    Good(Wrap(overlap.bytes, Bytes(259,'A')), Bytes(259,'A'));
    Writer far;
    far.Put(0,1); far.Put(0,2); far.Align(); far.Put(32768,16); far.Put(32767,16);
    Bytes expected;
    for (unsigned i=0;i<32768;++i) { expected.push_back(static_cast<uint8_t>(i%251)); far.Put(expected.back(),8); }
    far.Put(1,1); far.Put(1,2); far.Fixed(285); far.Code(29,5); far.Put(8191,13); far.Fixed(256);
    for (size_t i=0;i<258;++i) expected.push_back(expected[i]);
    Good(Wrap(far.bytes, expected), expected);
    Writer invalid_distance;
    invalid_distance.Put(1,1); invalid_distance.Put(1,2); invalid_distance.Fixed(257); invalid_distance.Code(0,5);
    Bad(Wrap(invalid_distance.bytes, {}), "Distance");
    Writer reserved_length;
    reserved_length.Put(1,1); reserved_length.Put(1,2); reserved_length.Fixed(286);
    Bad(Wrap(reserved_length.bytes, {}), "Reserved literal");
    Writer reserved_distance;
    reserved_distance.Put(1,1); reserved_distance.Put(1,2); reserved_distance.Fixed('A'); reserved_distance.Fixed(257); reserved_distance.Code(30,5);
    Bad(Wrap(reserved_distance.bytes, {}), "Reserved distance");
    Bad(Wrap(Bytes{7}, {}), "Reserved block");
    Bad(Wrap(Bytes{1,1,0,0,0,'x'}, {}), "stored");
    Bad(Wrap(Bytes{1,2,0,253,255,'x'}, {}), "stored");

    Bytes single(258,0); single[256]=1;
    auto empty=Dynamic(single); empty.Code(0,1);
    Good(Wrap(empty.bytes,{}),{},0);
    Good(Wrap(Bytes{1,0,0,255,255},{}),{},0);
    auto invalid_code=Dynamic(single); invalid_code.Code(1,1);
    Bad(Wrap(invalid_code.bytes,{}), "Huffman");
    Bytes two(258,0);two[65]=1;two[256]=1;
    auto literal_only=Dynamic(two);literal_only.Code(0,1);literal_only.Code(1,1);
    Good(Wrap(literal_only.bytes,Bytes{'A'}),Bytes{'A'});
    auto no_eob=Dynamic(Bytes(258,0));
    Bad(Wrap(no_eob.bytes,{}), "end-of-block");
    Bytes incomplete(258,0);incomplete[0]=2;incomplete[256]=2;
    Bad(Wrap(Dynamic(incomplete).bytes,{}), "Incomplete");
    Bytes oversubscribed(258,0);oversubscribed[0]=1;oversubscribed[1]=1;oversubscribed[256]=1;
    Bad(Wrap(Dynamic(oversubscribed).bytes,{}), "Oversubscribed");
    two[257]=2;
    Bad(Wrap(Dynamic(two).bytes,{}), "Incomplete");
    for (const auto lengths : {Bytes{1,1,1,0},Bytes{2,0,0,0},Bytes{0,0,0,0}}) {
        Writer codes;codes.Put(1,1);codes.Put(2,2);codes.Put(0,5);codes.Put(0,5);codes.Put(0,4);
        for (auto value:lengths) codes.Put(value,3);
        Bad(Wrap(codes.bytes,{}), "Huffman");
    }
    Writer first_repeat;first_repeat.Put(1,1);first_repeat.Put(2,2);first_repeat.Put(0,5);first_repeat.Put(0,5);first_repeat.Put(0,4);
    for (auto value: {1,0,0,1}) first_repeat.Put(value,3);
    first_repeat.Code(1,1); // Symbol 16 follows symbol 0 in canonical order.
    Bad(Wrap(first_repeat.bytes,{}), "repeat");
    Writer excess_repeat;excess_repeat.Put(1,1);excess_repeat.Put(2,2);excess_repeat.Put(0,5);excess_repeat.Put(0,5);excess_repeat.Put(0,4);
    for(auto value:{0,0,1,1})excess_repeat.Put(value,3);
    for(int i=0;i<2;++i){excess_repeat.Code(1,1);excess_repeat.Put(127,7);}
    Bad(Wrap(excess_repeat.bytes,{}), "repeat");
}

void HeaderTests(const Bytes& stored_expected, const Bytes& fixed_expected) {
    Bytes extra={31,139,8,30,0,0,0,0,0,255,3,0,'a','b','c','n','a','m','e',0,'n','o','t','e',0};
    const auto header_crc=Crc(extra);extra.push_back(static_cast<uint8_t>(header_crc));extra.push_back(static_cast<uint8_t>(header_crc>>8));
    extra.insert(extra.end(),std::begin(kGzipStored)+10,std::end(kGzipStored));
    Good(extra,stored_expected);
    extra[6]^=1;Bad(extra,"header CRC");
    Bytes bad_crc(std::begin(kGzipStored),std::end(kGzipStored));bad_crc[bad_crc.size()-8]^=1;Bad(bad_crc,"data CRC");
    Bytes bad_size(std::begin(kGzipStored),std::end(kGzipStored));bad_size.back()=1;Bad(bad_size);
    Bytes bad_flags(std::begin(kGzipStored),std::end(kGzipStored));bad_flags[3]=0xe0;Bad(bad_flags,"header");
    Bytes bad_method(std::begin(kGzipStored),std::end(kGzipStored));bad_method[2]=9;Bad(bad_method,"header");
    Bytes concatenated(std::begin(kGzipStored),std::end(kGzipStored));concatenated.insert(concatenated.end(),std::begin(kGzipStored),std::end(kGzipStored));Bad(concatenated);
    Bytes trailing(std::begin(kGzipStored),std::end(kGzipStored));trailing.insert(trailing.end()-8,1,0);Bad(trailing,"Trailing");
    for(size_t i=0;i<std::size(kGzipStored);++i)Bad(std::span(kGzipStored).first(i));
    Bad(kGzipFixed,"limit",1000);
    Bad(Wrap(std::span(kGzipFixed).subspan(10,std::size(kGzipFixed)-18),{}),"limit",1000);
    Bad(kGzipFixed,"limit",64*1024*1024+1);
    for(size_t i=0;i<std::size(kGzipFixed);++i)for(unsigned bit=0;bit<8;++bit){
        Bytes changed(std::begin(kGzipFixed),std::end(kGzipFixed));changed[i]^=static_cast<uint8_t>(1u<<bit);
        Bytes output={9,8,7};std::string error;
        if(!DecodeGzip(changed,64*1024*1024,output,error))Check(output==Bytes({9,8,7})&&!error.empty(),"Mutation failure changed output");
        else Check(output==fixed_expected,"Mutated stream bypassed data integrity validation");
    }
}

void EncoderTests() {
    Bytes payload;
    for(size_t size : {size_t(0),size_t(1),size_t(65535),size_t(65536),size_t(131071)}) {
        payload.resize(size);
        for(size_t i=0;i<size;++i)payload[i]=static_cast<uint8_t>(i%251);
        for(const std::string comment : {std::string(),std::string("vivaldi-plus-chrome-entry")}) {
            Bytes gzip={1,2,3};std::string error;
            Check(EncodeGzipStored(payload,comment,gzip,error)&&error.empty(),"Stored gzip encoder failed");
            Good(gzip,payload);
            Check(gzip[3]==(comment.empty()?0:16),"Stored gzip comment flag is wrong");
        }
    }
    for(const std::string comment : {std::string(128,'x'),std::string("bad\0comment",11),std::string("中文")}) {
        Bytes output={9,8,7};std::string error;
        Check(!EncodeGzipStored({},comment,output,error)&&output==Bytes({9,8,7})&&!error.empty(),"Invalid gzip comment must not change output");
    }
}
}

int main(int argc,char** argv) {
    if(argc==4&&std::string_view(argv[1])=="--encode") {
        std::ifstream input(argv[2],std::ios::binary);Check(input.good(),"Could not open plain input");
        Bytes bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()},output;std::string error;
        if(!EncodeGzipStored(bytes,"vivaldi-plus-chrome-entry",output,error)){std::cerr<<error<<'\n';return 1;}
        std::ofstream file(argv[3],std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char*>(output.data()),static_cast<std::streamsize>(output.size()));
        Check(file.good(),"Could not write encoded output");std::cout<<"Encoded "<<bytes.size()<<" bytes\n";return 0;
    }
    if(argc==3){
        std::ifstream input(argv[1],std::ios::binary);Check(input.good(),"Could not open gzip input");
        Bytes bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()},output;std::string error;
        if(!DecodeGzip(bytes,64*1024*1024,output,error)){std::cerr<<error<<'\n';return 1;}
        std::ofstream file(argv[2],std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char*>(output.data()),static_cast<std::streamsize>(output.size()));
        Check(file.good(),"Could not write decoded output");std::cout<<"Decoded "<<output.size()<<" bytes\n";return 0;
    }
    const auto stored=Text(std::string("stored: hello\0世界\n",sizeof("stored: hello\0世界\n")-1));
    Good(kGzipStored,stored);
    std::string fixed;for(int i=0;i<100;++i)fixed+="Vivaldi++ portable browser settings.\n";
    Good(kGzipFixed,Text(fixed));
    Bytes dynamic;for(unsigned i=0;i<16000;++i)dynamic.push_back(static_cast<uint8_t>(32+(i*i+17*i)%41));
    Good(kGzipDynamic,dynamic);
    std::string utf8;for(int i=0;i<1500;++i)utf8+="界面设置 · Vivaldi++\n";
    Good(kGzipUtf8,Text(utf8));
    uint32_t seed=0x12345678;Bytes long_distance;
    for(unsigned i=0;i<1024;++i){seed=1664525u*seed+1013904223u;long_distance.push_back(static_cast<uint8_t>(seed>>24));}
    for(unsigned i=0;i<1024;++i)long_distance.push_back(long_distance[i]);
    Good(kGzipLongDistance,long_distance);
    HeaderTests(stored,Text(fixed));BlockTests();EncoderTests();
    std::cout<<"PASS gzip stored/fixed/dynamic/UTF8/overlap/32768-distance/header/CRC/corruption/budget tests\n";
}
