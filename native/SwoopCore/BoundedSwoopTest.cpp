#include "BoundedSwoop.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace kt;
static void check(bool ok, const char* text) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", text); std::exit(1); }
}
static bool same(Vec3 a, Vec3 b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
static bool near(double a, double b, double tolerance=1e-9) { return std::abs(a-b)<tolerance; }
static SwoopBinding binding() { return {1,2,3,4,5}; }
static SwoopFrame validFrame(double now=10, double dt=1.0/60) {
    SwoopFrame f;
    f.binding = binding();
    f.wallNow = f.nativeWallstamp = f.observerWallstamp = now;
    f.nativeSequence = f.observerSequence = 1;
    f.safeMask = RequiredSwoopGates;
    f.simulationDt = dt;
    f.acceptedPlayerRequest = f.acceptedMode = f.movementPresent = true;
    f.startHeld = true;
    f.request = {0.6,0.8,-2.125};
    return f;
}
static void advance(SwoopFrame& f, double dt=1.0/60) {
    f.wallNow += dt;
    f.nativeWallstamp = f.observerWallstamp = f.wallNow;
    ++f.nativeSequence;
    ++f.observerSequence;
    f.simulationDt = dt;
}
static BoundedSwoop armed(DiagnosticSwoopSettings settings={}) {
    BoundedSwoop action(settings);
    check(action.enable(binding()), "explicit enable accepts known nonzero binding");
    return action;
}
static void cancellation(SwoopFrame f, SwoopReason reason, const char* description) {
    auto action = armed();
    const auto start = validFrame();
    check(action.step(start).modified, "fixture starts active");
    const auto output = action.step(f);
    check(output.reason==reason && !output.active && !output.modified && same(output.request,f.request), description);
    if (output.enabled) {
        advance(f);
        f.binding = binding();
        f.nativeWallstamp = f.observerWallstamp = f.wallNow;
        f.safeMask = RequiredSwoopGates;
        f.request = start.request;
        f.acceptedPlayerRequest = f.acceptedMode = f.movementPresent = true;
        const auto held = action.step(f);
        check(!held.modified && !held.active, "cancelled held input does not retry");
    }
}

int main() {
    {
        BoundedSwoop disabled;
        const auto f = validFrame();
        auto output = disabled.step(f);
        check(output.reason==SwoopReason::Disabled && same(output.request,f.request), "default is observation-only");
        check(!disabled.enable({}), "unknown binding refuses opt-in");
        check(disabled.enable(binding()) && !disabled.enable(binding()), "only one live owner can enable");
        disabled.disable();
        check(!disabled.enabled() && !disabled.active(), "explicit disable clears authorization");
    }
    {
        std::array<DiagnosticSwoopSettings,12> invalid{};
        invalid[0].duration = 1.001; invalid[1].duration = 0;
        invalid[2].peakGain = 2.001; invalid[3].peakGain = .99;
        invalid[4].travelBudget = 0; invalid[5].cooldown = .749;
        invalid[6].duration = std::numeric_limits<double>::quiet_NaN();
        invalid[7].peakGain = std::numeric_limits<double>::infinity();
        invalid[8].travelBudget = std::numeric_limits<double>::infinity();
        invalid[9].travelBudget = 4.001;
        invalid[10].cooldown = std::numeric_limits<double>::quiet_NaN();
        invalid[11].cooldown = std::numeric_limits<double>::infinity();
        for (auto config : invalid) {
            BoundedSwoop action(config);
            check(!action.enable(binding()), "invalid or expanded diagnostic bounds refuse opt-in");
        }
        check(armed({1.0,2.0,4.0,.75}).enabled(), "exact hard-limit settings are accepted");
    }
    std::array<double,3> travelByRate{};
    unsigned rateIndex = 0;
    for (int fps : {30,60,120}) {
        auto action = armed();
        auto f = validFrame(10,1.0/fps);
        double travel = 0, lastGain = 1, peak = 1;
        unsigned modifiedFrames = 0;
        SwoopResult output;
        for (unsigned i=0; i<200; ++i) {
            const SwoopFrame before = f;
            output = action.step(f);
            check(output.gain>=1 && output.gain<=2.0, "gain stays inside prototype ceiling");
            check(output.elapsed<=.90 && output.travel<=4.0, "duration and travel stay bounded");
            check(output.request.z==f.request.z, "vertical request is copied exactly");
            check(f.binding==before.binding && f.safeMask==before.safeMask && same(f.request,before.request), "input and gate fields remain unchanged");
            if (output.modified) {
                check(near(output.request.x/f.request.x,output.request.y/f.request.y), "scalar gain preserves horizontal direction");
                check(std::abs(output.gain-lastGain)<.15, "pulse has gradual onset and decay");
                travel += (std::hypot(output.request.x,output.request.y)-1)*f.simulationDt;
                ++modifiedFrames;
                peak = std::max(peak,output.gain);
            }
            lastGain = output.gain;
            if (!output.active && modifiedFrames) break;
            advance(f,1.0/fps);
        }
        check(output.reason==SwoopReason::Completed && modifiedFrames && peak>1.99, "finite prototype pulse completes");
        travelByRate[rateIndex++] = travel;
        const auto held = action.step(f);
        check(!held.modified && !held.active, "held start does not retrigger after completion");
        // Cooldown cannot be bypassed by a quick key release/repress.
        advance(f,1.0/fps); f.startHeld=false; action.step(f);
        advance(f,1.0/fps); f.startHeld=true;
        check(action.step(f).reason==SwoopReason::Cooldown, "fresh trigger during cooldown refuses");
        for (int i=0; i<fps; ++i) { advance(f,1.0/fps); action.step(f); }
        check(!action.active(), "cooldown expiry does not retry an earlier rejected key");
        advance(f,1.0/fps); f.startHeld=false; action.step(f);
        advance(f,1.0/fps); f.startHeld=true;
        check(action.step(f).modified, "release and new press after cooldown starts a new action");
    }
    // Compare analytically integrated extra travel independently of baseline
    // host travel. A lower baseline lets the complete curve fit within budget.
    check(near(travelByRate[0],travelByRate[1]) && near(travelByRate[1],travelByRate[2]) &&
        near(travelByRate[0],.90*.5), "30/60/120 Hz integrated pulse adds the same travel");

    for (int fps : {30,60,120}) {
        constexpr double crouchSpeed = 3.101;
        auto action = armed(); auto request = validFrame(10,1.0/fps);
        request.request = {.6*crouchSpeed,.8*crouchSpeed,-2.125};
        double integrated=0, peak=1, lastTravel=0;
        SwoopResult output;
        unsigned count=0;
        for (;count<200;++count) {
            output=action.step(request);
            check(output.gain>=1 && output.gain<=2 && output.elapsed<=.90 && output.travel<=4,
                "measured crouch speed obeys gain, duration and total request travel limits");
            check(output.request.z==request.request.z,"measured crouch-speed pulse preserves vertical input");
            check(near(output.request.x/request.request.x,output.request.y/request.request.y),
                "measured crouch-speed pulse retains horizontal direction");
            if (output.modified) {
                integrated+=std::hypot(output.request.x,output.request.y)*request.simulationDt;
                peak=std::max(peak,output.gain);
            }
            check(output.travel>=lastTravel,"travel accounting never moves backward");
            lastTravel=output.travel;
            if (!output.active) break;
            advance(request,1.0/fps);
        }
        check(count<200 && output.reason==SwoopReason::TravelBudget && peak>1.99,
            "measured crouch-speed pulse reaches peak then stops at travel budget");
        check(integrated<=4 && integrated>4-crouchSpeed/fps && near(integrated,output.travel),
            "measured crouch-speed integration stays within one host frame of fixed budget");
        advance(request,1.0/fps);
        const auto held=action.step(request);
        check(!held.modified && !held.active,"budget exhaustion never retries a held request");
        std::printf("CROUCH PROTOTYPE %d Hz: requested travel=%.9f, peak gain=%.9f, elapsed=%.9f s\n",
            fps,output.travel,peak,output.elapsed);
    }

    auto f = validFrame(); advance(f);
    for (auto gate : {Crouched,Grounded,LifecycleSafe,ControllerOwned,AnimationCompatible}) {
        auto unsafe = f; unsafe.safeMask &= ~gate;
        cancellation(unsafe,SwoopReason::UnknownOrUnsafe,"unknown or false gate cancels and forwards");
    }
    auto changed = f; ++changed.binding.generation;
    cancellation(changed,SwoopReason::BindingChanged,"generation change cancels and disarms");
    changed=f; ++changed.binding.identityEpoch;
    cancellation(changed,SwoopReason::BindingChanged,"identity epoch change cancels and disarms");
    changed=f; ++changed.binding.actorToken;
    cancellation(changed,SwoopReason::BindingChanged,"actor change cancels and disarms");
    changed=f; ++changed.binding.controllerToken;
    cancellation(changed,SwoopReason::BindingChanged,"controller change cancels and disarms");
    changed=f; ++changed.binding.animationOwnerToken;
    cancellation(changed,SwoopReason::BindingChanged,"animation owner change cancels and disarms");
    changed=f; changed.nativeWallstamp=f.wallNow-.151;
    cancellation(changed,SwoopReason::StaleNative,"native snapshot age over 150 ms cancels");
    changed=f; changed.observerWallstamp=f.wallNow-.251;
    cancellation(changed,SwoopReason::StaleObserver,"observer snapshot age over 250 ms cancels");
    changed=f; changed.nativeSequence=0;
    cancellation(changed,SwoopReason::StaleNative,"unknown snapshot sequence cancels");
    changed=f; changed.nativeSequence=1;
    cancellation(changed,SwoopReason::SequenceRollback,"relabelled snapshot timestamp cancels");
    changed=f; changed.nativeWallstamp=f.wallNow+.001;
    cancellation(changed,SwoopReason::StaleNative,"future native snapshot cancels");
    changed=f; changed.simulationDt=.151;
    cancellation(changed,SwoopReason::FrameGap,"simulation gap cancels without catch-up");
    changed=f; changed.wallNow+=.2; changed.nativeWallstamp=changed.observerWallstamp=changed.wallNow;
    cancellation(changed,SwoopReason::FrameGap,"wall gap cancels even when simulation dt is small");
    changed=f; changed.simulationDt=0;
    cancellation(changed,SwoopReason::FrameGap,"paused simulation cancels");
    changed=f; changed.wallNow=9;
    cancellation(changed,SwoopReason::InvalidClock,"clock rollback cancels");
    changed=f; changed.wallNow=std::numeric_limits<double>::quiet_NaN();
    cancellation(changed,SwoopReason::InvalidClock,"NaN clock cancels");
    changed=f; changed.acceptedPlayerRequest=false;
    cancellation(changed,SwoopReason::RejectedRequest,"nonplayer request cancels");
    changed=f; changed.acceptedMode=false;
    cancellation(changed,SwoopReason::RejectedRequest,"unaccepted movement mode cancels");
    changed=f; changed.movementPresent=false;
    cancellation(changed,SwoopReason::RejectedRequest,"missing request flag cancels");
    changed=f; changed.request={0,0,-2.125};
    cancellation(changed,SwoopReason::ZeroInput,"zero horizontal input cancels");
    changed=f; changed.request.x=std::numeric_limits<double>::quiet_NaN();
    const auto nanOutput = armed().step(changed);
    check(nanOutput.reason==SwoopReason::InvalidRequest && !nanOutput.modified && std::isnan(nanOutput.request.x) && nanOutput.request.z==changed.request.z,"NaN input is forwarded without mutation");
    changed=f; changed.request.z=std::numeric_limits<double>::infinity();
    auto infiniteAction=armed(); check(!infiniteAction.step(changed).modified,"infinite vertical input refuses before any write");
    changed=f; changed.manualStop=true;
    cancellation(changed,SwoopReason::ManualStop,"manual stop cancels and disarms");
    {
        auto action=armed({.55,1.35,.1,.75});
        auto request=validFrame();
        double reported=0;
        for (unsigned i=0;i<60;++i) {
            auto output=action.step(request);
            check(output.travel<=.1,"travel budget bounds every frame");
            reported=output.travel;
            if (!output.active) { check(output.reason==SwoopReason::TravelBudget,"travel budget ends action"); break; }
            advance(request);
        }
        check(reported>0 && reported<=.1,"travel budget is consumed without catch-up");
    }
    {
        auto action=armed(); auto request=validFrame();
        action.step(request);
        request.startHeld=false;
        advance(request); action.step(request);
        action.disable(); check(action.enable(binding()),"explicit re-enable remains possible");
        advance(request); request.startHeld=true;
        check(action.step(request).reason==SwoopReason::Cooldown,"re-enable preserves active cancellation cooldown");
    }
    {
        auto action=armed(); auto request=validFrame();
        action.step(request);
        request.wallNow += 1.0/60;
        check(action.step(request).modified,"unchanged cached snapshots remain usable within TTL");
        advance(request); action.step(request);
        advance(request); request.observerSequence=1;
        const auto rollback=action.step(request);
        check(!rollback.modified && !rollback.active && rollback.reason==SwoopReason::SequenceRollback,
            "observer sequence rollback cancels even while timestamps remain fresh");
    }
    {
        auto action=armed(); auto request=validFrame();
        request.safeMask &= ~Grounded;
        check(!action.step(request).modified,"unsafe first start is refused");
        advance(request); request.safeMask=RequiredSwoopGates;
        check(action.step(request).reason==SwoopReason::NoTrigger,"first refused held key cannot retry after state recovers");
    }
    {
        auto action=armed(); auto request=validFrame(10,.001);
        for (unsigned i=0;i<20;++i) {
            const auto output=action.step(request);
            if (!output.active) { check(output.reason==SwoopReason::Completed,"wall duration also bounds slowly advancing simulation"); break; }
            advance(request,.1); request.simulationDt=.001;
        }
        check(!action.active(),"wall duration cannot stretch into a long action");
    }
    std::puts("BOUNDED SWOOP FIXTURES PASS: opt-in, immutable horizontal scalar shaping, 30/60/120 Hz, duration/travel/gain bounds, safe gates, stale state, ownership/generation, pause/gap, invalid values, cooldown. Engine integration untested.");
}
