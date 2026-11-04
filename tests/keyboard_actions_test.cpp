#include "keyboard_actions.h"
#include "../CoD3Host/sprint_input.h"
#include <cstdio>
#include <stdexcept>
int main() {
 KeyboardActions::State s;int n=0;
 auto check=[&](std::string got,const char* want){++n;if(got!=want) throw std::runtime_error(got+" != "+want);};
 check(KeyboardActions::Update(s,false,true,false,false,true),"");
 check(KeyboardActions::Update(s,false,true,false,false,true),"");
 check(KeyboardActions::Update(s,false,false,false,false,true),"");
 if(!s.objectives) throw std::runtime_error("objectives did not stay on after release");++n;
 check(KeyboardActions::Update(s,false,true,false,false,true),"");
 check(KeyboardActions::Update(s,false,false,false,false,true),"");
 check(KeyboardActions::Update(s,false,true,false,false,true),"");
 check(KeyboardActions::Update(s,false,false,false,false,false),"");
 check(KeyboardActions::Update(s,true,false,true,false,true),"+reload\n+binoculars\n");
 check(KeyboardActions::Update(s,false,false,false,true,true),"-reload\n-binoculars\n");
 SprintInput::State sprint;
 auto expect=[&](bool ok){++n;if(!ok) throw std::runtime_error("sprint transition failed");};
 SprintInput::Filter(sprint,0x40,32767,0,0,true,false,true);expect(sprint.running);
 SprintInput::Filter(sprint,0,32767,0,0,true,false,true);expect(sprint.running);
 SprintInput::Filter(sprint,0x40,32767,0,0,true,false,true);expect(!sprint.running);
 sprint={};SprintInput::Filter(sprint,0x40,32767,0,0,true,false,true);
 SprintInput::Filter(sprint,0,32767,255,0,true,false,true);expect(!sprint.running);
 sprint={};SprintInput::Filter(sprint,0x40,32767,0,0,true,false,true);
 SprintInput::Filter(sprint,0,32767,0,255,true,false,true);expect(!sprint.running);
 sprint={};SprintInput::Filter(sprint,0x40,32767,0,0,true,false,true);
 SprintInput::Filter(sprint,0,0,0,0,true,false,true);expect(!sprint.running);
 sprint={};SprintInput::Filter(sprint,0x40,0,0,0,true,false,true);expect(!sprint.running);
 printf("PASS: %d objectives-toggle, input and sprint-transition checks\n",n);
}


