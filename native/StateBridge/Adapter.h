#pragma once
#include "Packet.h"
#include "../SwoopCore/BoundedSwoop.h"

namespace kt {
constexpr double MaxPrototypeHorizontalRequest=6.25;
struct NativeSnapshot {
    BridgePacket packet;
    double wallstamp=0,observerWallstamp=0;
    bool valid=false;
};
struct AdapterInput {
    NativeSnapshot snapshot;
    double now=0;
    std::uint32_t ownerEntity=0;
    std::uint64_t actorToken=0,controllerToken=0,flags=0;
    Vec3 request;
};
enum class AdapterReason {
    NoSnapshot,ContextChanged,SerialRollback,Unsafe,Stale,IdentityMismatch,
    RequestMode,RepeatedSnapshot,Idle,Core,Contention
};
struct AdapterResult {
    SwoopResult motion;
    AdapterReason reason=AdapterReason::NoSnapshot;
    bool eligible=false,identityMatch=false,triggerConsumed=false;
};
class MovementAdapter {
    BoundedSwoop core_;
    std::uint64_t epoch_=0,generation_=0,identity_=0,reset_=0,consumed_=0,lastSequence_=0;
    std::uint32_t entity_=0;
public:
    void stop() { core_.disable(); }
    void invalidate() { stop();epoch_=generation_=identity_=0;entity_=0; }
    AdapterResult step(const AdapterInput& f) {
        AdapterResult out;out.motion.request=f.request;
        const auto& s=f.snapshot; const auto& p=s.packet;
        if(!s.valid) { stop();return out; }
        if(p.epoch!=epoch_||p.generation!=generation_||p.identity!=identity_||p.entity!=entity_||p.resetSerial!=reset_) {
            stop();epoch_=p.epoch;generation_=p.generation;identity_=p.identity;entity_=p.entity;reset_=p.resetSerial;
            consumed_=p.requestSerial;lastSequence_=p.sequence;
            out.reason=AdapterReason::ContextChanged;return out;
        }
        if(p.requestSerial<consumed_||p.sequence<lastSequence_) { stop();out.reason=AdapterReason::SerialRollback;return out; }
        const bool trigger=p.requestSerial>consumed_;
        consumed_=p.requestSerial;out.triggerConsumed=trigger; // Consume even on every refusal below.
        const bool repeated=p.sequence==lastSequence_; lastSequence_=p.sequence;
        out.identityMatch=p.entity&&p.entity==f.ownerEntity&&f.actorToken&&f.controllerToken;
        if(!out.identityMatch) { stop();out.reason=AdapterReason::IdentityMismatch;return out; }
        if(!p.enabled||!safePacket(p)) { stop();out.reason=AdapterReason::Unsafe;return out; }
        if(!std::isfinite(f.now)||f.now<s.wallstamp||f.now-s.wallstamp>0.15||
           f.now<s.observerWallstamp||f.now-s.observerWallstamp>0.25) {
            stop();out.reason=AdapterReason::Stale;return out;
        }
        // Whitelist only the complete request modes seen in ordinary retail
        // locomotion. Unknown animation/jump flags never acquire this action.
        const double horizontal=std::hypot(f.request.x,f.request.y);
        if((f.flags!=0x1060&&f.flags!=0x201060)||!f.request.finite()||std::abs(f.request.z)>1e-5||
           !std::isfinite(horizontal)||horizontal>4.2) {
            stop();out.reason=AdapterReason::RequestMode;return out;
        }
        if(repeated) { stop();out.reason=AdapterReason::RepeatedSnapshot;return out; }
        out.eligible=true;
        // Token 1 identifies the supported normal-locomotion mode class. It is
        // a compatibility gate, NOT an observed animation-owner pointer. The
        // adapter never becomes a physics/root-motion owner of its own.
        SwoopBinding binding{p.generation,p.identity,f.actorToken,f.controllerToken,1};
        if(!core_.enabled()) {
            if(!trigger) { out.reason=AdapterReason::Idle;return out; }
            if(!core_.enable(binding)) { out.reason=AdapterReason::Unsafe;return out; }
        }
        SwoopFrame frame;
        frame.binding=binding;frame.wallNow=f.now;frame.nativeWallstamp=s.wallstamp;
        frame.observerWallstamp=s.observerWallstamp;frame.nativeSequence=p.sequence;
        frame.observerSequence=p.observerSequence;frame.safeMask=RequiredSwoopGates;
        frame.simulationDt=p.frameDt;frame.acceptedPlayerRequest=true;frame.acceptedMode=true;
        frame.movementPresent=(f.flags&0x20)!=0;frame.startHeld=trigger;frame.request=f.request;
        out.motion=core_.step(frame);out.reason=AdapterReason::Core;
        // Absolute prototype cap near the ordinary retail running speed already
        // observed, regardless of relative gain. Higher-speed bases refuse.
        if(std::hypot(out.motion.request.x,out.motion.request.y)>MaxPrototypeHorizontalRequest) {
            stop();out.motion={};out.motion.request=f.request;out.reason=AdapterReason::RequestMode;out.eligible=false;
        }
        return out;
    }
};
}
