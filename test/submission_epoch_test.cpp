#include "../src/submission_epoch.h"
#include <cstdio>
#include <cstdlib>
#include <thread>
#define CHECK(x) do{if(!(x)){std::printf("FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using d2vr_submission::Epoch;
int main(){
 Epoch gate;CHECK(!gate.Begin(11));gate.Configure(true);
 auto first=gate.Begin(11);CHECK(gate.CanReuse(first));
 gate.Invalidate();CHECK(!gate.Current(first));
 auto fresh=gate.Begin(11);CHECK(gate.CanReuse(fresh));
 {Epoch::Bypass outer(gate);CHECK(!gate.Current(fresh));
  auto during=gate.Begin(11);CHECK(!gate.CanReuse(during));
  {Epoch::Bypass nested(gate);CHECK(!gate.CanReuse(gate.Begin(11)));}
  CHECK(!gate.CanReuse(gate.Begin(11)));
 }
 CHECK(gate.CanReuse(gate.Begin(11)));
 auto before=gate.Begin(11);
 std::thread writer([&]{gate.ObserveMutation(12);});writer.join();
 CHECK(!gate.Current(before) && !gate.Enabled());CHECK(gate.Reasons()&Epoch::ForeignThread);
 gate.Invalidate();gate.Configure(true);CHECK(!gate.Enabled()); // Reset cannot reopen the gate
 for(auto reason:{Epoch::OpaqueStateBlock,Epoch::InterfaceEscape,Epoch::DeviceReplaced}){
  Epoch state;state.Configure(true);auto outstanding=state.Begin(11);CHECK(state.Current(outstanding));
  state.Deny(reason);CHECK(!state.CanReuse(outstanding));
  state.Invalidate();state.Configure(true);CHECK(!state.Begin(11));CHECK(state.Reasons()==reason);
 }
 Epoch recursive;recursive.Configure(true);auto outer=recursive.Begin(11);
 // Same-thread callbacks invalidate before forwarding; returning from the
 // outer successful native call must not republish its obsolete state.
 recursive.Invalidate();auto callback=recursive.Begin(11);CHECK(recursive.Current(callback));
 CHECK(!recursive.Current(outer));
 Epoch missing;missing.Configure(true);CHECK(!missing.Begin(0));CHECK(missing.Reasons()&Epoch::UncontrolledMutation);
 Epoch disabled;disabled.ObserveMutation(12);CHECK(!disabled.Reasons());
 disabled.Configure(true);CHECK(disabled.CanReuse(disabled.Begin(11)));
 puts("PASS submission epoch: bypass nesting, late publication, foreign mutation, persistent StateBlock/interface denial and reset");
}
