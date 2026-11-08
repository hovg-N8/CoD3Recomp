#pragma once
#include "bot_navigation.h"
#include "bot_failed_routes.h"
#include <algorithm>
#include <array>

namespace BotNavigation {
struct Visit { Point at{}; bool valid=false; };
struct Route { Point next{}; bool valid=false; unsigned queries=0; };

// Bounded local best-first search; every edge is a body sweep plus floor
// support. Keeping the first waypoint until reached avoids wall jitter.
template<class Query>
Route Detour(Point from, Point goal, int hand, bool escaping,
             const std::array<Visit,8>& visits, Query&& query,
             const FailedRoutes* failed=nullptr, uint32_t now=0) {
    Route result;
    if (!Finite(from)||!Finite(goal)||Distance(from,goal)<20) return result;
    // 0.5.700: 96 traces (was 64): Climb may now try a piece twice (a
    // stair's lift, then the whole rise), and on stairs (16-unit pieces,
    // three traces each) a few ways used the 64 up before the way out was
    // tried (Carentan's cellar stairs)
    auto bounded=[&](Point a,Point b,bool body) {
        if (result.queries>=96) return Hit{};
        ++result.queries; return query(a,b,body);
    };
    struct Node { Point at,first; float cost,rank; unsigned depth; bool expanded; };
    std::array<Node,32> nodes{}; unsigned count=1;
    nodes[0]={from,from,0,Distance(from,goal),0,false};
    float best=-std::numeric_limits<float>::infinity();
    const float initial=Distance(from,goal);
    for(unsigned expansion=0;expansion<12 && result.queries<96;++expansion) {
        unsigned pick=32; float rank=std::numeric_limits<float>::infinity();
        for(unsigned i=0;i<count;++i) if(!nodes[i].expanded && nodes[i].depth<4 && nodes[i].rank<rank) {pick=i;rank=nodes[i].rank;}
        if(pick==32)break;
        nodes[pick].expanded=true; const Node parent=nodes[pick];
        const float yaw=std::atan2(goal.y-parent.at.y,goal.x-parent.at.x);
        for(float degrees : {0.0f,45.0f,-45.0f,90.0f,-90.0f,135.0f,-135.0f,180.0f}) {
            if(result.queries>=96 || count==nodes.size())break;
            const float angle=yaw+degrees*hand*0.01745329252f;
            const float length=std::fmin(Distance(parent.at,goal),96.0f);
            Point candidate{parent.at.x+std::cos(angle)*length,parent.at.y+std::sin(angle)*length,parent.at.z},landing;
            bool duplicate=false;
            for(unsigned i=0;i<count;++i)if(Distance(candidate,nodes[i].at)<40){duplicate=true;break;}
            if(duplicate || !Reachable(parent.at,candidate,bounded,landing))continue;
            // 0.5.195: never route back onto a remembered failure.
            if(failed && failed->NearFailedTarget(landing,now))continue;
            const Point first=pick==0?landing:parent.first;
            float revisit=0;
            for(auto v:visits)if(v.valid && Distance(first,v.at)<72)revisit=180;
            float bias=0;
            if(escaping) {
                const float dx=goal.x-from.x,dy=goal.y-from.y;
                const float cross=(dx*(first.y-from.y)-dy*(first.x-from.x))*hand;
                if(cross>20)bias=35;
                if(Distance(first,from)<40)bias-=200;
            }
            const float cost=parent.cost+Distance(parent.at,landing);
            const float remaining=Distance(landing,goal);
            const float score=initial-remaining-cost*0.12f-revisit+bias;
            nodes[count++]={landing,first,cost,remaining+cost*0.12f+revisit-bias,parent.depth+1,false};
            if(score>best){best=score;result.next=first;result.valid=true;}
        }
    }
    return result;
}

// A96-unit route can fail in a cramped spawn alcove even when a short
// body-supported move is possible. At most eight candidates; never force
// movement through a wall or across missing floor after a failed search.
template<class Query>
bool ShortEscape(Point from,Point goal,int hand,Query&& query,Point& out) {
    if(!Finite(from)||!Finite(goal)||Distance(from,goal)<20)return false;
    const float yaw=std::atan2(goal.y-from.y,goal.x-from.x);
    unsigned calls=0;
    auto bounded=[&](Point a,Point b,bool body){
        if(calls>=40)return Hit{};   // 0.5.700: 40 (was 24), as Detour's
        ++calls;return query(a,b,body);
    };
    for(float degrees:{0.f,45.f,-45.f,90.f,-90.f,135.f,-135.f,180.f}) {
        const float angle=yaw+degrees*hand*0.01745329252f;
        Point candidate{from.x+std::cos(angle)*32,from.y+std::sin(angle)*32,from.z};
        if(Reachable(from,candidate,bounded,out))return true;
    }
    return false;
}

// Test a low obstacle using full-height clearance above it and a supported
// landing. This only requests native jumping; it never changes position.
template<class Query>
bool JumpOver(Point from,Point goal,Query&& query,Point& landing) {
    if(!Finite(from)||!Finite(goal)||Distance(from,goal)<20)return false;
    const float yaw=std::atan2(goal.y-from.y,goal.x-from.x);
    Point low{from.x,from.y,from.z+2};
    Point across{from.x+std::cos(yaw)*64,from.y+std::sin(yaw)*64,from.z+2};
    const Hit blocked=query(low,across,true);
    if(!blocked.valid||blocked.solid||blocked.fraction>=0.999f)return false;
    Point raised{from.x,from.y,from.z+40},far{across.x,across.y,from.z+40};
    const Hit up=query(low,raised,true);if(!up.valid||up.solid||up.fraction<0.999f)return false;
    const Hit travel=query(raised,far,true);if(!travel.valid||travel.solid||travel.fraction<0.999f)return false;
    const Hit floor=query(far,{far.x,far.y,from.z-28},true);
    if(!floor.valid||floor.solid||floor.fraction>=1||!Finite(floor.end)||!Finite(floor.normal)||
       floor.normal.z<0.65f||floor.end.z<from.z-26||floor.end.z>from.z+38)return false;
    landing=floor.end;landing.z+=1;return true;
}

// Native PM_CheckLadder (824A5270) tests surfaceFlags &8 and stores its
// normal in ps+100; PM_LadderMove (824A9D28) runs for ps+44 &0x10.
inline bool LadderSurface(const Hit& hit) {
    return hit.valid&&!hit.solid&&hit.fraction<1&&(hit.surfaceFlags&8)!=0&&
        Finite(hit.normal)&&std::fabs(hit.normal.z)<0.3f&&
        std::hypot(hit.normal.x,hit.normal.y)>0.7f;
}
}
