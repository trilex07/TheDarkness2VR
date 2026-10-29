#include "../../src/psvr2/IpcProtocol.h"

#include <iostream>
#include <vector>

namespace {
int failures = 0;
void require(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "FAILED: " << name << '\n';
        ++failures;
    }
}
}

int main() {
    using namespace d2vr::psvr2;
    const auto pistol = weaponCommand(TriggerHand::Left, 4, 6, 3);
    const auto shotgun = weaponCommand(TriggerHand::Right, 2, 4, 7);
    require(packTriggerCommand(pistol) != packTriggerCommand(shotgun), "hands do not alias");
    const auto packed = packTriggerCommand(shotgun);
    const auto unpacked = unpackTriggerCommand(packed);
    require(unpacked.effect == TriggerEffect::Weapon && unpacked.hand == TriggerHand::Right
        && unpacked.startPosition == 2 && unpacked.endPosition == 4
        && unpacked.strength == 7, "weapon roundtrip");
    const auto vibration = vibrationCommand(TriggerHand::Left, 3, 7, 55);
    const auto vibrationBack = unpackTriggerCommand(packTriggerCommand(vibration));
    require(vibrationBack.effect == TriggerEffect::Vibration && vibrationBack.frequency == 55
        && vibrationBack.amplitude == 7 && vibrationBack.hand == TriggerHand::Left,
        "vibration roundtrip");

    const auto encoded = encodeTriggerState(2, 10, pistol);
    DecodedIpcMessage decoded{};
    require(validateIpcMessage(encoded.bytes.data(), encoded.size, &decoded)
        == IpcValidationResult::Valid, "encoded trigger valid");
    require(decodeTriggerCommand(decoded).startPosition == 4, "decoded pistol");

    auto bad = encoded;
    bad.bytes[0] ^= 0xff;
    require(validateIpcMessage(bad.bytes.data(), bad.size) == IpcValidationResult::BadMagic,
        "bad magic");

    const auto hello = encodeHello(1, 1, "ab");
    require(validateIpcMessage(hello.bytes.data(), hello.size) == IpcValidationResult::Valid,
        "hello");
    DecodedIpcMessage helloDecoded{};
    require(validateIpcMessage(hello.bytes.data(), hello.size, &helloDecoded)
        == IpcValidationResult::Valid, "hello decoded");
    require(constantTimeTokenEquals(helloDecoded, "ab"), "token matches");
    require(!constantTimeTokenEquals(helloDecoded, "ac"), "token rejects");
    require(!constantTimeTokenEquals(helloDecoded, "a"), "token length");

    ServerSessionState session{};
    require(handleServerMessage(session, helloDecoded, "ab") == ServerSessionAction::Authorized,
        "hello authorizes");
    require(handleServerMessage(session, decoded, "ab") == ServerSessionAction::TriggerState,
        "trigger after hello");
    require(handleServerMessage(session, decoded, "ab") == ServerSessionAction::Reject,
        "replay rejected");

    const auto shutdown = encodeShutdown(4, 11);
    DecodedIpcMessage shutdownDecoded{};
    require(validateIpcMessage(shutdown.bytes.data(), shutdown.size, &shutdownDecoded)
        == IpcValidationResult::Valid, "shutdown");
    ServerSessionState ending{};
    handleServerMessage(ending, helloDecoded, "ab");
    require(handleServerMessage(ending, shutdownDecoded, "ab") == ServerSessionAction::Shutdown,
        "shutdown action");
    return failures == 0 ? 0 : 1;
}
