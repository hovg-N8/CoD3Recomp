#pragma once
#include <string>
namespace KeyboardActions {
struct State { bool reload=false,objectives=false,objectivesWas=false,outWas=false,binoculars=false; };
inline std::string Update(State& s,bool reload,bool objectives,bool out,bool away,bool enabled) {
    std::string commands;
    auto edge=[&](bool held,bool& previous,const char* name) {
        held=held&&enabled;
        if(held!=previous) { commands+=held?"+":"-";commands+=name;commands+='\n';previous=held; }
    };
    edge(reload,s.reload,"reload");
    bool show=s.objectives;
    if(enabled && objectives && !s.objectivesWas) show=!show;
    if(!enabled) show=false;
    s.objectives=show; // Hold the native Back button; do not compete with its command path.
    s.objectivesWas=enabled&&objectives;
    bool binoculars=s.binoculars;
    if(enabled && out && !s.outWas) binoculars=!binoculars;
    if(away || !enabled) binoculars=false;
    edge(binoculars,s.binoculars,"binoculars");
    s.outWas=enabled&&out;
    return commands;
}
}
