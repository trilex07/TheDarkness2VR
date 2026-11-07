#pragma once
#include <cstring>
namespace d2vr_native_build {
// Known identities are diagnostic labels only, never admission gates.
// Unknown hashes continue to exact PE/instruction/import/owner validation.
inline bool KnownRuntimeSha(const char* sha) {
    return sha && (!std::strcmp(sha,"d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1") ||
        !std::strcmp(sha,"f263b81b950930f9244e8f5feae78de19689ed0c3de6e3a2923a85d6d4f9fdfa"));
}
}
