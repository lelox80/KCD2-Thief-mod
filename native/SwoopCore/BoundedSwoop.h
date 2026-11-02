#pragma once
#include "Trajectory.h"
#include <algorithm>
#include <cstdint>

namespace kt {

// Tuned retail-feel prototype bounds, not donor units or a faithful Thief
// trajectory. Distances use the host request's units. The adapter must establish
// those units before opting in; this core scales an existing horizontal request.
struct DiagnosticSwoopSettings {
    double duration = 0.90;
    double peakGain = 2.0;
    double travelBudget = 4.0;
    double cooldown = 1.0;
};

struct SwoopBinding {
    std::uint64_t generation = 0, identityEpoch = 0;
    std::uint64_t actorToken = 0, controllerToken = 0, animationOwnerToken = 0;
    bool valid() const {
        return generation && identityEpoch && actorToken && controllerToken && animationOwnerToken;
    }
    bool operator==(const SwoopBinding& b) const {
        return generation == b.generation && identityEpoch == b.identityEpoch &&
            actorToken == b.actorToken && controllerToken == b.controllerToken &&
            animationOwnerToken == b.animationOwnerToken;
    }
};

enum SwoopSafeGate : std::uint32_t {
    Crouched = 1u, Grounded = 2u, LifecycleSafe = 4u,
    ControllerOwned = 8u, AnimationCompatible = 16u,
    RequiredSwoopGates = Crouched | Grounded | LifecycleSafe | ControllerOwned | AnimationCompatible
};

struct SwoopFrame {
    SwoopBinding binding;
    double wallNow = 0, nativeWallstamp = 0, observerWallstamp = 0;
    std::uint64_t nativeSequence = 0, observerSequence = 0;
    std::uint32_t safeMask = 0;
    double simulationDt = 0;
    bool acceptedPlayerRequest = false, acceptedMode = false, movementPresent = false;
    bool startHeld = false, manualStop = false;
    Vec3 request;
};

enum class SwoopReason {
    Disabled, InvalidSettings, NoTrigger, Active, Cooldown, Completed,
    TravelBudget, ManualStop, BindingChanged, UnknownOrUnsafe,
    StaleNative, StaleObserver, InvalidClock, SequenceRollback,
    FrameGap, RejectedRequest, InvalidRequest, ZeroInput
};

struct SwoopResult {
    Vec3 request;
    SwoopReason reason = SwoopReason::Disabled;
    bool modified = false, active = false, enabled = false;
    double gain = 1, elapsed = 0, travel = 0;
};

// Single-owner, allocation-free state machine. Call enable/disable/step from the
// same serialized owner; this class deliberately makes no engine/thread claims.
// All refusals forward the original vector. No catch-up, additive velocity,
// position changes, vertical changes, or implicit rearming occur here.
class BoundedSwoop {
    DiagnosticSwoopSettings settings_;
    SwoopBinding binding_;
    bool enabled_ = false, active_ = false, startHeld_ = false, haveClock_ = false;
    std::uint64_t nativeSequence_ = 0, observerSequence_ = 0;
    double nativeStamp_ = 0, observerStamp_ = 0;
    double lastWall_ = 0, cooldownUntil_ = 0, actionStartedWall_ = 0, elapsed_ = 0, travel_ = 0;

    bool settingsValid() const {
        return std::isfinite(settings_.duration) && settings_.duration > 0 && settings_.duration <= 1.0 &&
            std::isfinite(settings_.peakGain) && settings_.peakGain >= 1 && settings_.peakGain <= 2.0 &&
            std::isfinite(settings_.travelBudget) && settings_.travelBudget > 0 && settings_.travelBudget <= 4.0 &&
            std::isfinite(settings_.cooldown) && settings_.cooldown >= 0.75;
    }
    // Integral on [0,u] of a symmetric smoothstep pulse with zero endpoint
    // derivatives. Analytic integration avoids a frame-rate-dependent envelope.
    static double pulseIntegral(double u) {
        u = std::clamp(u, 0.0, 1.0);
        const auto ramp = [](double x) { return 4*x*x*x - 4*x*x*x*x; };
        return u <= 0.5 ? ramp(u) : 0.5 - ramp(1-u);
    }
    void finish(double now) {
        active_ = false;
        cooldownUntil_ = std::max(cooldownUntil_, now + settings_.cooldown);
    }
    SwoopResult result(const SwoopFrame& frame, SwoopReason reason) const {
        return {frame.request, reason, false, active_, enabled_, 1, elapsed_, travel_};
    }
    SwoopResult refuse(const SwoopFrame& frame, SwoopReason reason, double now) {
        if (active_) finish(now);
        return result(frame, reason);
    }
public:
    explicit BoundedSwoop(DiagnosticSwoopSettings settings = {}) : settings_(settings) {}

    bool enable(SwoopBinding binding) {
        if (enabled_ || !binding.valid() || !settingsValid()) return false;
        binding_ = binding;
        enabled_ = true;
        active_ = startHeld_ = haveClock_ = false;
        nativeSequence_ = observerSequence_ = 0;
        nativeStamp_ = observerStamp_ = 0;
        elapsed_ = travel_ = 0;
        // Preserve cooldown across explicit disable/re-enable in this instance.
        return true;
    }
    void disable() {
        if (active_) finish(lastWall_);
        enabled_ = active_ = false;
    }
    bool enabled() const { return enabled_; }
    bool active() const { return active_; }

    SwoopResult step(const SwoopFrame& frame) {
        const bool rising = frame.startHeld && !startHeld_;
        startHeld_ = frame.startHeld; // A refused held key never retries itself.
        if (!settingsValid()) return result(frame, SwoopReason::InvalidSettings);
        if (!enabled_) return result(frame, SwoopReason::Disabled);
        if (frame.manualStop) {
            enabled_ = false;
            return refuse(frame, SwoopReason::ManualStop, lastWall_);
        }
        if (!(frame.binding == binding_) || !frame.binding.valid()) {
            enabled_ = false;
            return refuse(frame, SwoopReason::BindingChanged, lastWall_);
        }
        if (!std::isfinite(frame.wallNow) || frame.wallNow < 0 ||
            (haveClock_ && frame.wallNow < lastWall_))
            return refuse(frame, SwoopReason::InvalidClock, lastWall_);
        const double wallGap = haveClock_ ? frame.wallNow - lastWall_ : 0;
        lastWall_ = frame.wallNow;
        haveClock_ = true;
        if (!std::isfinite(frame.simulationDt) || frame.simulationDt <= 0 ||
            frame.simulationDt > 0.15 || wallGap > 0.15)
            return refuse(frame, SwoopReason::FrameGap, frame.wallNow);
        if ((frame.safeMask & RequiredSwoopGates) != RequiredSwoopGates)
            return refuse(frame, SwoopReason::UnknownOrUnsafe, frame.wallNow);
        if (!frame.nativeSequence || !std::isfinite(frame.nativeWallstamp) ||
            frame.nativeWallstamp < 0 || frame.nativeWallstamp > frame.wallNow ||
            frame.wallNow - frame.nativeWallstamp > 0.15)
            return refuse(frame, SwoopReason::StaleNative, frame.wallNow);
        if (!frame.observerSequence || !std::isfinite(frame.observerWallstamp) ||
            frame.observerWallstamp < 0 || frame.observerWallstamp > frame.wallNow ||
            frame.wallNow - frame.observerWallstamp > 0.25)
            return refuse(frame, SwoopReason::StaleObserver, frame.wallNow);
        // Repeated immutable snapshots are allowed within their strict TTL.
        if (frame.nativeSequence < nativeSequence_ || frame.observerSequence < observerSequence_ ||
            (frame.nativeSequence == nativeSequence_ && frame.nativeWallstamp != nativeStamp_) ||
            (frame.observerSequence == observerSequence_ && frame.observerWallstamp != observerStamp_) ||
            frame.nativeWallstamp < nativeStamp_ || frame.observerWallstamp < observerStamp_)
            return refuse(frame, SwoopReason::SequenceRollback, frame.wallNow);
        nativeSequence_ = frame.nativeSequence;
        observerSequence_ = frame.observerSequence;
        nativeStamp_ = frame.nativeWallstamp;
        observerStamp_ = frame.observerWallstamp;
        if (!frame.acceptedPlayerRequest || !frame.acceptedMode || !frame.movementPresent)
            return refuse(frame, SwoopReason::RejectedRequest, frame.wallNow);
        if (!frame.request.finite())
            return refuse(frame, SwoopReason::InvalidRequest, frame.wallNow);
        const double speed = std::hypot(frame.request.x, frame.request.y);
        if (!std::isfinite(speed)) return refuse(frame, SwoopReason::InvalidRequest, frame.wallNow);
        if (speed <= 1e-8) return refuse(frame, SwoopReason::ZeroInput, frame.wallNow);
        if (active_ && frame.wallNow - actionStartedWall_ >= settings_.duration) {
            finish(frame.wallNow);
            return result(frame, SwoopReason::Completed);
        }
        if (!active_) {
            if (frame.wallNow < cooldownUntil_) return result(frame, SwoopReason::Cooldown);
            if (!rising) return result(frame, SwoopReason::NoTrigger);
            active_ = true;
            actionStartedWall_ = frame.wallNow;
            elapsed_ = travel_ = 0;
        }

        const double end = std::min(settings_.duration, elapsed_ + frame.simulationDt);
        const double pulseArea = settings_.duration *
            (pulseIntegral(end / settings_.duration) - pulseIntegral(elapsed_ / settings_.duration));
        double gain = 1 + (settings_.peakGain - 1) * pulseArea / frame.simulationDt;
        const double baseTravel = speed * frame.simulationDt;
        const double remaining = settings_.travelBudget - travel_;
        if (!std::isfinite(baseTravel) || baseTravel <= 0)
            return refuse(frame, SwoopReason::InvalidRequest, frame.wallNow);
        if (remaining < baseTravel) {
            finish(frame.wallNow);
            return result(frame, SwoopReason::TravelBudget);
        }
        gain = std::clamp(std::min(gain, remaining / baseTravel), 1.0, settings_.peakGain);
        Vec3 candidate = frame.request;
        candidate.x *= gain;
        candidate.y *= gain;
        if (!candidate.finite()) return refuse(frame, SwoopReason::InvalidRequest, frame.wallNow);
        travel_ = std::min(settings_.travelBudget, travel_ + baseTravel * gain);
        elapsed_ = end;
        SwoopReason reason = SwoopReason::Active;
        if (travel_ >= settings_.travelBudget - 1e-10) {
            finish(frame.wallNow);
            reason = SwoopReason::TravelBudget;
        } else if (elapsed_ >= settings_.duration) {
            finish(frame.wallNow);
            reason = SwoopReason::Completed;
        }
        auto output = result(frame, reason);
        output.request = candidate;
        output.gain = gain;
        output.modified = gain > 1;
        return output;
    }
};
}
