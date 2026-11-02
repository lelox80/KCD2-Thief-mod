#include "Trajectory.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace kt;
void check(bool ok,const char* text) { if(!ok) { fprintf(stderr,"FAIL: %s\n",text); exit(1); } }
bool near(Vec3 a,Vec3 b,double tolerance=1e-8) { auto d=a-b;return std::abs(d.x)<tolerance&&std::abs(d.y)<tolerance&&std::abs(d.z)<tolerance; }
int main() {
    Trajectory t({0,0,0},{525,0,0},{0,0,0},{100,0,0},1.15);
    check(near(t.at(0).position,{0,0,0})&&near(t.at(0).velocity,{0,0,0}),"initial conditions");
    check(near(t.at(1.15).position,{525,0,0})&&near(t.at(1.15).velocity,{100,0,0}),"terminal conditions");
    for(int fps:{30,60,120}) {
        Vec3 sum{};
        for(double elapsed=0;elapsed<1.15;elapsed+=1.0/fps) sum=sum+t.requestedDelta(elapsed,1.0/fps,0.1);
        check(near(sum,{525,0,0}),"frame-rate independent integrated travel");
    }
    auto current=t.at(.4); auto steered=t.redirected(.4,{250,300,0},{0,100,0});
    check(near(steered.at(0).position,current.position)&&near(steered.at(0).velocity,current.velocity),"steering splice continuity");
    check(near(steered.at(.75).position,{250,300,0}),"steered terminal position");
    double epsilon=1e-6;
    auto derivative=(t.at(.4+epsilon).position-t.at(.4-epsilon).position)*(1/(2*epsilon));
    check(near(derivative,t.at(.4).velocity,1e-6),"analytic derivative matches displacement");
    unsigned refused=0;
    try { t.requestedDelta(0,.5,.1); } catch(const std::invalid_argument&) { ++refused; }
    try { t.at(std::numeric_limits<double>::quiet_NaN()); } catch(const std::invalid_argument&) { ++refused; }
    try { Trajectory bad({},{},{},{},0); } catch(const std::invalid_argument&) { ++refused; }
    try { t.redirected(2,{},{}); } catch(const std::invalid_argument&) { ++refused; }
    check(refused==4,"invalid inputs and time gaps refuse");
    puts("TRAJECTORY FIXTURES PASS: endpoints, derivative, 30/60/120 Hz, steering continuity, invalid inputs. Engine integration untested.");
}
