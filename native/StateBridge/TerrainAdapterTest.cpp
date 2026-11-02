// Owned inputs only. These fixtures verify refusal/consumption, not terrain physics.
#include "Adapter.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
void check(bool ok,const char* why) {if(!ok) {fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
kt::AdapterInput frame(double dt) {
    kt::AdapterInput f;auto& p=f.snapshot.packet;
    p.epoch=p.generation=p.identity=p.sequence=p.observerSequence=1;p.entity=42;
    p.asyncSeconds=10;p.health=100;p.stamina=240;
    p.frameDt=dt;p.knownMask=p.safeMask=kt::RequiredBridgeMask;p.enabled=true;p.dryRun=false;
    f.snapshot.valid=true;f.now=f.snapshot.wallstamp=f.snapshot.observerWallstamp=10;
    f.ownerEntity=42;f.actorToken=123;f.controllerToken=456;f.flags=0x1060;f.request={0,3.1,0};return f;
}
void next(kt::AdapterInput& f) {
    f.now+=f.snapshot.packet.frameDt;f.snapshot.wallstamp=f.snapshot.observerWallstamp=f.now;
    ++f.snapshot.packet.sequence;++f.snapshot.packet.observerSequence;
}
void unchanged(const kt::AdapterResult& r,const kt::AdapterInput& f,const char* why) {
    check(!r.motion.modified&&r.motion.request.x==f.request.x&&r.motion.request.y==f.request.y&&r.motion.request.z==f.request.z,why);
}
int main() {
    constexpr std::uint32_t Contact=1u<<10;
    for(double rate:{30.,60.,120.}) for(bool unknown:{false,true}) {
        kt::MovementAdapter adapter;auto f=frame(1/rate);adapter.step(f);
        next(f);++f.snapshot.packet.requestSerial;check(adapter.step(f).motion.modified,"supported moving press starts");
        for(unsigned i=0;i<3;++i) {next(f);check(adapter.step(f).motion.modified,"grounded pulse continues");}
        // Step off support while still holding ordinary movement. No impulse,
        // vertical injection or position correction substitutes for the host.
        next(f);f.snapshot.packet.safeMask&=~Contact;
        if(unknown) f.snapshot.packet.knownMask&=~Contact;
        unchanged(adapter.step(f),f,"unsafe/unknown support immediately forwards original request");
        // A new press while airborne is consumed even if contact later returns.
        next(f);++f.snapshot.packet.requestSerial;unchanged(adapter.step(f),f,"airborne press refuses");
        for(unsigned i=0;i<static_cast<unsigned>(rate/2);++i) {
            next(f);unchanged(adapter.step(f),f,"no gain during airborne continuation");
        }
        f.snapshot.packet.knownMask=f.snapshot.packet.safeMask=kt::RequiredBridgeMask;
        for(unsigned i=0;i<static_cast<unsigned>(2*rate);++i) {
            next(f);unchanged(adapter.step(f),f,"landing never resumes or retries a consumed press");
        }
        next(f);++f.snapshot.packet.requestSerial;check(adapter.step(f).motion.modified,"fresh grounded press after cooldown works");
        // A terrain/animation mode carrying a nonzero Z request is unsupported.
        // Preserve it exactly rather than flattening/boosting a step or jump.
        next(f);f.request={1,2,0.2};unchanged(adapter.step(f),f,"nonzero host Z refuses without rewriting it");
        next(f);f.request={0,3.1,0};unchanged(adapter.step(f),f,"horizontal mode recovery does not rearm");
        printf("TERRAIN REFUSAL %.0f Hz: %s contact, airborne press, landing no-rearm and original-Z preservation pass.\n",rate,unknown?"unknown":"unsafe");
    }
    puts("TERRAIN ADAPTER FIXTURES PASS. Slopes, stairs, physical edge latency and collision still require retail observation.");
}
