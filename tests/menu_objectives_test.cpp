#include "../CoD3Host/menu_input.h"
#include "../CoD3Host/keyboard_actions.h"
#include <cstdio>
#include <stdexcept>
int main(){int n=0;auto check=[&](bool ok){++n;if(!ok)throw std::runtime_error("regression failed");};
check(MenuInput::Apply(0x1000,true,false,true,false)==0x10);
check(MenuInput::Apply(0x1010,false,false,true,false)==0x1000);
check(MenuInput::Apply(0,false,false,false,true)==0x1000);
check(MenuInput::Apply(0,true,false,false,true)==0x10);
check(MenuInput::Apply(0x1000,false,true,true,false)==0x1000);
KeyboardActions::State s;KeyboardActions::Update(s,false,true,false,false,true);
for(int i=0;i<600;++i) {KeyboardActions::Update(s,false,false,false,false,true);check(s.objectives);}
KeyboardActions::Update(s,false,true,false,false,true);check(!s.objectives);
printf("PASS: %d Space/Enter and objectives persistence checks\n",n);}

