#pragma once

#include "VaultTrial.h"

namespace ktruntime {
constexpr float AirMinHeight=1.72f,AirMaxHeight=2.20f;
struct Marker {
    ktcapsule::Vector position,facing;
    std::uint32_t corner=1;
};
static_assert(sizeof(Marker)==28&&offsetof(Marker,corner)==24);
constexpr Marker Placeholder[]={{{0,0,0},{0,-1,0}},{{0.5f,0,0},{0,-1,0}}};

inline void markers(const ktvault::Wall& wall,Marker (&out)[2]) {
    const auto p=wall.lip,f=wall.facing;
    const float dx=wall.halfWidth*f.y,dy=-wall.halfWidth*f.x;
    out[0]={{p.x-dx,p.y-dy,p.z},{-f.x,-f.y,0}};
    out[1]={{p.x+dx,p.y+dy,p.z},{-f.x,-f.y,0}};
}

enum class Survey {NotRun, Face, Top, Height, Section, Measured, Crest};
constexpr const char* SurveyNames[]={"not_run","face","top","height","wall_section","measured","crest"};
struct SurveyReading {
    Survey stage=Survey::NotRun;
    float height=0;
    bool heightKnown=false;
};
struct SurveyTrace {
    SurveyReading broad,crest;
    Survey stage() const {return crest.stage==Survey::NotRun?broad.stage:crest.stage;}
};

inline bool detect(ktcapsule::Query host,void* world,const char* tag,void* physical,
                   ktcapsule::Vector feet,const float (&direction)[3],ktvault::Wall& wall,bool taller=false,
                   SurveyReading* reading=nullptr,bool crestOnly=false) {
    using namespace ktcapsule;
    if(reading) *reading={Survey::Face,0,false};
    const float length=direction[0]*direction[0]+direction[1]*direction[1];
    if(!(std::abs(length-1)<0.02f&&std::abs(direction[2])<=0.01f)) return false;
    const float scale=1/std::sqrt(length);
    const Vector f{direction[0]*scale,direction[1]*scale,0};
    Shape sample{{feet.x,feet.y,feet.z+0.60f},{0,0,1},0.04f,0.02f};
    const float hit=query(host,world,tag,physical,sample,{f.x*1.35f,f.y*1.35f,0});
    if(!(hit>0.35f&&hit<1.25f)) return false;
    const float depth=hit+(crestOnly?0.070f:0.10f);
    const float x=feet.x+f.x*depth,y=feet.y+f.y*depth;
    if(reading) reading->stage=Survey::Top;
    sample.center={x,y,feet.z+(taller?AirMaxHeight+0.22f:1.85f)};
    if(crestOnly) {
        sample.radius=0.018f;
        sample.halfSpan=0.01f;
    }
    if(query(host,world,tag,physical,sample,{})!=0) return false;
    const float down=query(host,world,tag,physical,sample,{0,0,-1.12f});
    if(!(down>0&&down<1.12f)) return false;
    const float z=sample.center.z-down-sample.radius-sample.halfSpan;
    const float height=z-feet.z;
    if(!std::isfinite(height)) return false;
    if(reading) {reading->height=height;reading->heightKnown=true;}
    if(!(down>0.12f&&down<(crestOnly?1.10f:1.02f))) return false;
    if(reading) reading->stage=Survey::Height;
    if(!(height>=(taller?AirMinHeight:0.78f)&&height<=(taller?AirMaxHeight:1.65f))) return false;
    const Vector lip{x,y,z};
    const float inset=crestOnly?0.0f:0.42f;
    const Corridor route{feet,{x+f.x*inset,y+f.y*inset,z},z+0.16f};
    if(reading) reading->stage=Survey::Section;
    if(!ledgeSectionClear(host,world,tag,physical,{lip,f,crestOnly})) return false;
    wall={ktledge::NoGuide,route,f,lip,0.27f,crestOnly};
    if(reading) reading->stage=crestOnly?Survey::Crest:Survey::Measured;
    return true;
}

inline bool survey(ktcapsule::Query host,void* world,const char* tag,void* physical,
                   ktcapsule::Vector feet,const float (&direction)[3],ktvault::Wall& wall,
                   bool taller,SurveyTrace& trace) {
    trace={};
    if(detect(host,world,tag,physical,feet,direction,wall,taller,&trace.broad)) return true;
    const auto stage=trace.broad.stage;
    if(stage!=Survey::Top&&stage!=Survey::Height&&stage!=Survey::Section) return false;
    return detect(host,world,tag,physical,feet,direction,wall,taller,&trace.crest,true);
}

inline bool measure(std::uintptr_t base,void* actor,ktcapsule::Vector feet,const float (&direction)[3],
                    ktvault::Wall& wall,bool taller,SurveyTrace& trace,bool crestOnly=false) {
    __try {
        auto* physical=ktvault::physicalOwner(base,actor);
        const auto world=*reinterpret_cast<void**>(base+ktcapsule::WorldGlobal);
        if(!physical||!world||*reinterpret_cast<std::uintptr_t*>(world)!=base+ktcapsule::WorldVtable||
           *reinterpret_cast<void**>(static_cast<unsigned char*>(physical)+0x68)!=world) return false;
        const auto host=reinterpret_cast<ktcapsule::Query>(base+ktcapsule::Intersection);
        const auto tag=reinterpret_cast<const char*>(base+ktcapsule::Tag);
        if(crestOnly) return detect(host,world,tag,physical,feet,direction,wall,taller,&trace.crest,true);
        return survey(host,world,tag,physical,feet,direction,wall,taller,trace);
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}

inline ktcapsule::LedgeSection section(const ktvault::Wall& wall,const ktcapsule::Corridor& route) {
    return {wall.crestOnly?route.landing:wall.lip,wall.facing,wall.crestOnly};
}

inline bool sameLip(const ktvault::Wall& first,const ktvault::Wall& second) {
    const auto p=first.lip,q=second.lip;
    return std::abs(p.x-q.x)<0.09f&&std::abs(p.y-q.y)<0.09f&&std::abs(p.z-q.z)<0.07f;
}
}
