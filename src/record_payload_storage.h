#pragma once
#include <cstdint>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace d2vr_record_payload {
// Only the byte arena uses default initialization. RecPush holds its lock and
// fills every appended byte before publishing a record. Copy failures explicitly
// supply the legacy readable prefix/zero tail and reject the recording. No
// uninitialized payload escapes and zero padding cannot certify valid data.
template<class T> struct Allocator : std::allocator<T> {
    using value_type=T;
    template<class U> struct rebind {using other=Allocator<U>;};
    Allocator() noexcept=default;
    template<class U> Allocator(const Allocator<U>&) noexcept {}
    template<class U> void construct(U* p) {::new(static_cast<void*>(p)) U;}
    template<class U,class A,class... Args> void construct(U* p,A&& a,Args&&... args) {
        ::new(static_cast<void*>(p)) U(std::forward<A>(a),std::forward<Args>(args)...);
    }
};
template<class T,class U> bool operator==(const Allocator<T>&,const Allocator<U>&) noexcept{return true;}
template<class T,class U> bool operator!=(const Allocator<T>&,const Allocator<U>&) noexcept{return false;}
using Storage=std::vector<uint8_t,Allocator<uint8_t>>;
}
