#include "../src/native_build_identity.h"
#include "../src/diagnostic_present_patch.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main() {
 const char* original="d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1";
 const char* player="f263b81b950930f9244e8f5feae78de19689ed0c3de6e3a2923a85d6d4f9fdfa";
 const char* other="ef306de6146b5fe2722645be9de6edfc7635495680a9f4ab6d6ea7e6fb7cdfd4";
 const char* diagnostic="55c8d634fcbe2996d8397d15d68ea728c999697f1dd40e8779b5afe8bbb4c339";
 assert(d2vr_native_build::KnownRuntimeSha(original)); assert(d2vr_native_build::KnownRuntimeSha(player));
 assert(!d2vr_native_build::KnownRuntimeSha(other)); assert(!d2vr_native_build::KnownRuntimeSha(diagnostic));
 assert(!d2vr_native_build::KnownRuntimeSha(nullptr)); assert(!d2vr_native_build::KnownRuntimeSha(""));
 for(auto sha : {original,player,diagnostic,other,"",static_cast<const char*>(nullptr)}) {
   assert(d2vr_present_patch::Identity(sha,0x14c,0x4f68a875,0xdec000,0x400000,0x10b));
   assert(!d2vr_present_patch::Identity(sha,0x8664,0x4f68a875,0xdec000,0x400000,0x10b));
   assert(!d2vr_present_patch::Identity(sha,0x14c,0,0xdec000,0x400000,0x10b));
   assert(!d2vr_present_patch::Identity(sha,0x14c,0x4f68a875,0,0x400000,0x10b));
   assert(!d2vr_present_patch::Identity(sha,0x14c,0x4f68a875,0xdec000,0x500000,0x10b));
   assert(!d2vr_present_patch::Identity(sha,0x14c,0x4f68a875,0xdec000,0x400000,0x20b));
 }
 assert(d2vr_present_patch::Identity(other,0x14c,0x4f68a875,0xdec000,0x400000,0x10b));
 puts("PASS unknown/null hashes never veto; PE rejection contracts retained");
}
