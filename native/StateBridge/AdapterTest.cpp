#include "Adapter.h"
#include <cstdio>
#include <cstdlib>
void check(bool ok,const char* why) { if(!ok) { fprintf(stderr,"FAIL: %s\n",why);exit(1); } }
kt::AdapterInput fixture() {
    kt::AdapterInput f;auto& s=f.snapshot;auto& p=s.packet;
    p.epoch=p.generation=p.identity=p.sequence=p.observerSequence=1;p.entity=42;
    p.asyncSeconds=1;p.frameDt=1./60;p.observerAge=0;p.health=100;p.stamina=240;
    p.knownMask=p.safeMask=kt::RequiredBridgeMask;p.enabled=true;p.dryRun=true;
    s.valid=true;s.wallstamp=s.observerWallstamp=f.now=1;f.ownerEntity=42;
    f.actorToken=123;f.controllerToken=456;f.flags=0x1060;f.request={0,3.1,0};return f;
}
void advance(kt::AdapterInput& f) {f.now+=1./60;f.snapshot.wallstamp=f.snapshot.observerWallstamp=f.now;++f.snapshot.packet.sequence;++f.snapshot.packet.observerSequence;}
int main() {
    kt::MovementAdapter a;auto f=fixture();
    check(a.step(f).reason==kt::AdapterReason::ContextChanged,"first packet only establishes context");
    advance(f);check(!a.step(f).motion.modified,"default never activates");
    advance(f);++f.snapshot.packet.requestSerial;check(a.step(f).motion.modified,"one explicit valid trigger");
    advance(f);f.snapshot.packet.safeMask=0;auto r=a.step(f);check(!r.motion.modified&&!r.eligible,"unsafe snapshot cancels");
    advance(f);f.snapshot.packet.safeMask=kt::RequiredBridgeMask;check(!a.step(f).motion.modified,"safe again never retries old request");
    advance(f);++f.snapshot.packet.requestSerial;f.ownerEntity=43;check(a.step(f).reason==kt::AdapterReason::IdentityMismatch,"Lua and native entity must match");
    advance(f);f.ownerEntity=42;check(!a.step(f).motion.modified,"identity refusal consumed request");
    advance(f);++f.snapshot.packet.generation;++f.snapshot.packet.requestSerial;check(a.step(f).reason==kt::AdapterReason::ContextChanged,"load transition baselines serial");
    advance(f);check(!a.step(f).motion.modified,"new generation never resumes action");
    advance(f);++f.snapshot.packet.requestSerial;f.flags=0x81060;check(a.step(f).reason==kt::AdapterReason::RequestMode,"jump/untraced flags refuse");
    advance(f);f.flags=0x1060;f.snapshot.wallstamp=f.now-.2;check(a.step(f).reason==kt::AdapterReason::Stale,"native TTL expires");
    advance(f);a.step(f);check(a.step(f).reason==kt::AdapterReason::RepeatedSnapshot,"never integrate same simulation delta twice");
    {
        kt::MovementAdapter capped;auto high=fixture();high.request={0,4.2,0};capped.step(high);
        advance(high);++high.snapshot.packet.requestSerial;
        bool refused=false;
        for(unsigned i=0;i<60;++i) {
            const auto decision=capped.step(high);
            if(decision.motion.modified)
                check(std::hypot(decision.motion.request.x,decision.motion.request.y)<=kt::MaxPrototypeHorizontalRequest,"absolute cap holds during stronger profile");
            if(decision.reason==kt::AdapterReason::RequestMode) {
                check(!decision.motion.modified&&!decision.eligible&&decision.motion.request.y==4.2,"over-cap candidate returns original request and cancels");
                refused=true;break;
            }
            advance(high);
        }
        check(refused,"higher baseline cannot escape the ordinary-running absolute cap");
        advance(high);check(!capped.step(high).motion.modified,"cap cancellation never retries the consumed intent");
    }
    puts("ADAPTER FIXTURES PASS: opt-in, complete identity/gates, one-shot consumption, epoch transitions, modes, TTL and duplicate-frame refusal.");
}
