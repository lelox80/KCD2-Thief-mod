#pragma once
#include <cmath>
#include <stdexcept>

namespace kt {
struct Vec3 {
    double x=0,y=0,z=0;
    Vec3 operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec3 operator*(double s) const { return {x*s,y*s,z*s}; }
    bool finite() const { return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z); }
};
struct Motion { Vec3 position,velocity; };

// Reconstructed cubic Hermite primitive from the donor's position/derivative
// callbacks. Units are caller supplied; this is not wired to a game controller.
class Trajectory {
    Vec3 start_, end_, startVelocity_, endVelocity_;
    double duration_;
public:
    Trajectory(Vec3 start,Vec3 end,Vec3 startVelocity,Vec3 endVelocity,double duration)
      :start_(start),end_(end),startVelocity_(startVelocity),endVelocity_(endVelocity),duration_(duration) {
        if (!start.finite()||!end.finite()||!startVelocity.finite()||!endVelocity.finite()||
            !std::isfinite(duration)||duration<=0) throw std::invalid_argument("invalid trajectory");
    }
    Motion at(double elapsed) const {
        if (!std::isfinite(elapsed)||elapsed<0) throw std::invalid_argument("invalid simulation time");
        const double u=elapsed<duration_ ? elapsed/duration_ : 1;
        const double u2=u*u,u3=u2*u;
        return {
            start_*(2*u3-3*u2+1)+startVelocity_*((u3-2*u2+u)*duration_)+
                end_*(-2*u3+3*u2)+endVelocity_*((u3-u2)*duration_),
            start_*((6*u2-6*u)/duration_)+startVelocity_*(3*u2-4*u+1)+
                end_*((-6*u2+6*u)/duration_)+endVelocity_*(3*u2-2*u)
        };
    }
    // Splice a new goal while retaining current position and velocity. The
    // adapter must separately validate stance, contact, lifecycle and ownership.
    Trajectory redirected(double elapsed,Vec3 goal,Vec3 terminalVelocity) const {
        if (elapsed>=duration_) throw std::invalid_argument("trajectory already complete");
        auto current=at(elapsed);
        return {current.position,goal,current.velocity,terminalVelocity,duration_-elapsed};
    }
    Vec3 requestedDelta(double elapsed,double dt,double maxFrameGap) const {
        if (!std::isfinite(dt)||dt<=0||!std::isfinite(maxFrameGap)||maxFrameGap<=0||dt>maxFrameGap)
            throw std::invalid_argument("invalid or discontinuous frame");
        return at(elapsed+dt).position-at(elapsed).position;
    }
};
}
