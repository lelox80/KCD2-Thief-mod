#pragma once
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace kt {
constexpr std::uint32_t RequiredBridgeMask = 33554431u;
constexpr std::uint64_t ExactLuaCounterLimit = 8388607u;
struct BridgePacket {
    std::uint64_t epoch=0,generation=0,identity=0,sequence=0,observerSequence=0;
    std::uint32_t entity=0,knownMask=0,safeMask=0;
    double asyncSeconds=0,frameDt=0,observerAge=0,health=0,stamina=0;
    std::uint64_t requestSerial=0,resetSerial=0;
    bool enabled=false,dryRun=true;
    char reason[64]{};
};
inline bool decimal(std::string_view text,std::uint64_t& out,std::uint64_t limit=ExactLuaCounterLimit) {
    if(text.empty()||text.size()>16||text.front()=='-'||text.front()=='+') return false;
    auto parsed=std::from_chars(text.data(),text.data()+text.size(),out);
    return parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&out<=limit;
}
inline bool real(std::string_view text,double& out) {
    if(text.empty()||text.size()>32||text.front()=='+') return false;
    auto parsed=std::from_chars(text.data(),text.data()+text.size(),out,std::chars_format::general);
    return parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&std::isfinite(out)&&std::abs(out)<=1e12;
}
inline bool entityToken(std::string_view text,std::uint32_t& out) {
    std::uint64_t value=0;
    if(text.size()>2&&text.substr(0,2)=="0x") {
        if(text.size()>10) return false;
        auto parsed=std::from_chars(text.data()+2,text.data()+text.size(),value,16);
        if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||value>0xffffffffull) return false;
    } else if(!decimal(text,value,0xffffffffull)) return false;
    out=static_cast<std::uint32_t>(value); return true;
}
inline bool gateMask(std::string_view text,std::uint32_t& out) {
    // Retail Lua uses float32. Never transport a 25-bit mask as a number:
    // 33554431 rounds to 33554432 and lower-bit differences can collide.
    if(text.size()!=26||text.front()!='b') return false;
    std::uint32_t value=0;
    for(unsigned i=0;i<25;++i) {
        if(text[i+1]!='0'&&text[i+1]!='1') return false;
        if(text[i+1]=='1') value|=1u<<i;
    }
    out=value;return true;
}
inline bool parsePacket(std::string_view bytes,BridgePacket& output) {
    // Parse into a temporary; failed packets never partly update a snapshot.
    if(bytes.empty()||bytes.size()>512) return false;
    for(unsigned char c:bytes) if(c<32||c>126) return false;
    std::string_view field[19]; std::size_t start=0,index=0;
    for(std::size_t i=0;i<=bytes.size();++i) {
        if(i==bytes.size()||bytes[i]=='|') {
            if(index>=19) return false;
            field[index++]=bytes.substr(start,i-start); start=i+1;
        }
    }
    if(index!=19||field[0]!="v2") return false;
    BridgePacket p;std::uint32_t known=0,safe=0;std::uint64_t enabled=0,dry=0;
    if(!decimal(field[1],p.epoch)||!decimal(field[2],p.generation)||!decimal(field[3],p.identity)||
       !entityToken(field[4],p.entity)||!decimal(field[5],p.sequence)||!real(field[6],p.asyncSeconds)||
       !real(field[7],p.frameDt)||!real(field[8],p.observerAge)||!decimal(field[9],p.observerSequence)||
       !gateMask(field[10],known)||!gateMask(field[11],safe)||
       !decimal(field[12],p.requestSerial)||!decimal(field[13],p.resetSerial)||
       !decimal(field[14],enabled,1)||!decimal(field[15],dry,1)||!real(field[16],p.health)||!real(field[17],p.stamina))
        return false;
    if((safe&known)!=safe||field[18].empty()||field[18].size()>=sizeof p.reason) return false;
    for(char c:field[18]) if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c==':')) return false;
    p.knownMask=static_cast<std::uint32_t>(known);p.safeMask=static_cast<std::uint32_t>(safe);
    p.enabled=enabled!=0;p.dryRun=dry!=0;
    memcpy(p.reason,field[18].data(),field[18].size());
    output=p;return true;
}
inline bool safePacket(const BridgePacket& p) {
    return p.epoch&&p.generation&&p.identity&&p.entity&&p.sequence&&p.observerSequence&&
        p.knownMask==RequiredBridgeMask&&p.safeMask==RequiredBridgeMask&&
        p.asyncSeconds>=0&&p.frameDt>0&&p.frameDt<=0.15&&p.observerAge>=0&&p.observerAge<=0.25&&
        p.health>0&&p.stamina>0;
}
}
