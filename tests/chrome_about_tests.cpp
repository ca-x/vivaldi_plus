#include "../src/chrome_about.h"
#include "../src/gzip_decode.h"
#include "../src/version.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {
const std::string module = "settings-about-page; function t(){return html`<settings-section page-title=\"$i18n{aboutPageTitle}\"><div>Chrome</div><cr-link-row class=\"hr\" id=\"help\"></cr-link-row></settings-section><span>$i18n{settings}</span>`;}";
const std::string url = "http://127.0.0.1:16877/settings.html";
void Check(bool ok, const char* text) { if (!ok) { std::cerr << text << '\n'; std::exit(1); } }
void U16(std::string& b, size_t at, unsigned value) { b[at] = static_cast<char>(value); b[at + 1] = static_cast<char>(value >> 8); }
void U32(std::string& b, size_t at, uint32_t value) { for (unsigned i=0;i<4;i++) b[at+i]=static_cast<char>(value>>(8*i)); }
uint32_t Read32(const std::string& b,size_t at) { uint32_t n=0;for(unsigned i=0;i<4;i++)n|=static_cast<uint32_t>(static_cast<unsigned char>(b[at+i]))<<(8*i);return n; }
std::span<const uint8_t> Bytes(std::string_view b) { return {reinterpret_cast<const uint8_t*>(b.data()),b.size()}; }
std::string Fixture(std::string text=module) {
    std::vector<uint8_t> gzip; std::string error;
    Check(EncodeGzipStored(Bytes(text), {}, gzip, error), "Could not create compressed fixture");
    std::string pack(34,'\0'); U32(pack,0,5);pack[4]=1;U16(pack,8,2);U16(pack,10,1);
    U16(pack,12,7);U32(pack,14,34);U16(pack,18,42);U32(pack,20,39);U32(pack,26,39+static_cast<uint32_t>(gzip.size()));
    U16(pack,30,43);U16(pack,32,1); pack += std::string("a\0b\0c",5);
    pack.append(reinterpret_cast<const char*>(gzip.data()),gzip.size());return pack;
}
std::string Decoded(const std::string& pack) {
    const auto start=Read32(pack,20),end=Read32(pack,26);std::vector<uint8_t> out;std::string error;
    Check(DecodeGzip(Bytes(std::string_view(pack).substr(start,end-start)),16*1024*1024,out,error),error.c_str());
    return {reinterpret_cast<const char*>(out.data()),out.size()};
}
}

int main(int argc,char** argv) {
    if(argc==4) {
        std::ifstream in(argv[2],std::ios::binary);Check(in.good(),"Could not read pack");
        const std::string pack{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
        std::string out,error;Check(PatchChromeAbout(pack,url,RELEASE_VER_STR,std::string(argv[1])=="--patch",out,error),error.c_str());
        std::ofstream file(argv[3],std::ios::binary);file.write(out.data(),out.size());Check(file.good(),"Could not write pack");return 0;
    }
    const auto pack=Fixture();std::string patched,error;
    Check(PatchChromeAbout(pack,url,"1.6.1.0",true,patched,error),error.c_str());
    Check(patched.substr(30,9)==pack.substr(30,9),"Aliases and unrelated binary resource must remain identical");
    const auto text=Decoded(patched);
    Check(text.find("id=\"vivaldi-plus-settings\"")!=std::string::npos&&text.find("?version=1.6.1.0")!=std::string::npos,"Settings link must be in the real Chrome about section");
    Check(text.find("$i18n{settings}</a>")!=std::string::npos,"Chrome must localize the settings entry");
    std::string repeated;Check(PatchChromeAbout(patched,url,"1.6.1.0",true,repeated,error)&&repeated==patched,"Reinstall must not rewrite resource bytes");
    std::string upgraded;Check(PatchChromeAbout(patched,"http://127.0.0.1:16878/settings.html","1.6.1.1",true,upgraded,error),error.c_str());
    Check(Decoded(upgraded).find("16877")==std::string::npos,"Port update must replace the existing link");
    std::string removed;Check(PatchChromeAbout(upgraded,{}, {},false,removed,error)&&Decoded(removed)==module,"Disabling WebUI must restore the original template text");
    Check(removed.find("vivaldi-plus-chrome-entry")==std::string::npos,"Disabling must remove the gzip marker too");
    Check(PatchChromeAbout(removed,{}, {},false,repeated,error)&&repeated==removed,"Repeated disable must be a no-op");
    Check(PatchChromeAbout(pack,{}, {},false,repeated,error)&&repeated==pack,"Default-off must leave an untouched pack identical");
    std::string bad=pack;U32(bad,20,1);repeated="unchanged";
    Check(!PatchChromeAbout(bad,url,"1",true,repeated,error)&&repeated=="unchanged","Invalid offsets must not change output");
    Check(!PatchChromeAbout(Fixture("unsupported"),url,"1",true,repeated,error),"Missing about template must be rejected");
    Check(!PatchChromeAbout(pack,"http://evil/settings.html","1",true,repeated,error),"Remote settings URL must be rejected");
    Check(!PatchChromeAbout(pack,url,"1\"<script>",true,repeated,error),"Template injection version must be rejected");
    Check(!PatchChromeAbout(Fixture(module+module),url,"1",true,repeated,error),"Ambiguous about sections must be rejected");
    std::cout<<"PASS Chrome settings/help resource integration, toggling and idempotence\n";
}
