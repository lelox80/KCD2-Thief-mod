#include "Packet.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
void check(bool ok,const char* why) { if(!ok) { fprintf(stderr,"FAIL: %s\n",why);exit(1); } }
int main(int argc,char** argv) {
    kt::BridgePacket p;
    const std::string full="b"+std::string(25,'1'),zero="b"+std::string(25,'0');
    const std::string valid="v2|1|2|3|0x123|4|25.5|0.016|0.05|9|"+full+"|"+full+"|10|5|1|1|100|240|safe";
    check(kt::parsePacket(valid,p)&&p.entity==0x123&&p.requestSerial==10&&p.dryRun&&kt::safePacket(p),"complete safe packet");
    for(const std::string bad:{valid+"|extra",std::string("v2|1"),std::string(513,'a'),"v1"+valid.substr(2),
         valid.substr(0,valid.size()-4)+"bad reason",std::string("v2|1|2|3|0|4|nan|0.016|0.05|9|0|0|10|5|1|1|100|240|bad")})
        check(!kt::parsePacket(bad,p),"malformed input refuses");
    check(p.entity==0x123&&p.requestSerial==10,"failed parse preserves existing snapshot");
    const std::string partial="v2|1|2|3|0|4|25.5|0.016|0.05|9|"+zero+"|"+zero+"|10|5|1|1|100|240|unknown";
    check(kt::parsePacket(partial,p)&&!kt::safePacket(p),"unknown packet remains unsafe");
    std::uint32_t id=1;
    check(!kt::entityToken("userdata: 00000123",id)&&!kt::entityToken("4294967296",id),"no pointer-string/overflow entity conversion");
    std::uint32_t mask=0;
    check(kt::gateMask(full,mask)&&mask==33554431u,"all 25 bits survive without a floating-point mask");
    for(unsigned bit=0;bit<25;++bit) {
        auto one=zero;one[bit+1]='1';
        check(kt::gateMask(one,mask)&&mask==(1u<<bit),"every gate identity preserved");
        auto missing=full;missing[bit+1]='0';
        check(kt::gateMask(missing,mask)&&mask==(33554431u^(1u<<bit)),"no low-bit collisions above float32 precision");
    }
    for(const auto& bad:{std::string("33554431"),std::string("33554432"),full+"1",full.substr(1),std::string("b")+std::string(25,'2')})
        check(!kt::gateMask(bad,mask),"numeric rounded or malformed masks refused");
    std::uint64_t counter=0;
    check(kt::decimal("8388607",counter)&&!kt::decimal("8388608",counter),"counter range stays exactly representable in retail Lua");
    if(argc==2) {
        std::ifstream source(argv[1]);check(source.good(),"open actual Lua wire fixture");
        const std::uint32_t known[]={33554431u,33554431u,33554431u^(1u<<10),33554431u};
        const std::uint32_t safe[]={33554431u,33554431u^(1u<<10),33554431u^(1u<<10),33554430u};
        std::string line;unsigned index=0;
        while(std::getline(source,line)) {
            if(!line.empty()&&line.back()=='\r') line.pop_back();
            check(index<4&&line.size()<=512&&kt::parsePacket(line,p),"actual Lua packet accepted by native parser");
            check(p.knownMask==known[index]&&p.safeMask==safe[index],"actual Lua masks preserve all gates and low-bit differences");
            ++index;
        }
        check(index==4,"exactly four actual Lua wire fixtures");
    } else check(argc==1,"optional wire-fixture path only");
    puts("BRIDGE PACKET FIXTURES PASS: protocol2, exact 25-bit text masks, per-gate identity, counter precision, atomic parsing and unknown refusal.");
}
