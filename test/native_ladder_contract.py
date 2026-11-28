"""Exercise the pinned Steam ladder state machine in an offline x86 fixture."""

import argparse
import hashlib
import json
from pathlib import Path
import struct

import pefile
from unicorn import UC_ARCH_X86, UC_HOOK_CODE, UC_MODE_32, Uc
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESP


IMAGE_SHA256 = "d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1"
IMAGE_BASE = 0x400000
LADDER_TABLE = 0xFFCF80
LADDER_MOUNT_CALL = 0x824028
LADDER_INPUT = 0x7DC4B0
LADDER_UPDATE = 0xAC7AB0
LADDER_EVENT = 0x821230

FIXTURE = 0x20000000
STOP = FIXTURE
ACTOR = FIXTURE + 0x1000
ACTOR_TABLE = FIXTURE + 0x2000
ACTOR_REF = FIXTURE + 0x3000
ACTION = FIXTURE + 0x4000
ANIMATION = FIXTURE + 0x5000
ANIMATION_TABLE = FIXTURE + 0x6000
ANIMATION_REF = FIXTURE + 0x7000
EVENT = FIXTURE + 0x8000
EVENT_REF = FIXTURE + 0x8100
MOVEMENT = FIXTURE + 0x9000
STACK = FIXTURE + 0xEFF0

RETURN_FALSE = FIXTURE + 0x100
GET_ANIMATION = FIXTURE + 0x110
GET_ANIMATION_REF = FIXTURE + 0x120
PLAY_ANIMATION = FIXTURE + 0x140
RUNG_END_ID, TOP_END_ID = 103, 104
TRAVERSAL_MODE = ACTOR + 0x540
CURRENT_RUNG = ACTION + 0x6E8
NEXT_RUNG = ACTION + 0x6EC
ACTION_STATE = ACTION + 0x798


class Fixture:
    def __init__(self, executable):
        raw = executable.read_bytes()
        assert hashlib.sha256(raw).hexdigest() == IMAGE_SHA256, "Unexpected Steam executable"
        pe = pefile.PE(data=raw)
        assert pe.FILE_HEADER.Machine == 0x14C and pe.OPTIONAL_HEADER.ImageBase == IMAGE_BASE
        self.cpu = Uc(UC_ARCH_X86, UC_MODE_32)
        self.cpu.mem_map(IMAGE_BASE, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
        self.cpu.mem_write(IMAGE_BASE, pe.get_memory_mapped_image())
        self.cpu.mem_map(FIXTURE, 0x10000)
        assert self.word(LADDER_TABLE + 0x254) == LADDER_UPDATE
        assert self.word(LADDER_TABLE + 0x268) == LADDER_INPUT
        assert self.word(LADDER_TABLE + 0x2D8) == LADDER_EVENT
        assert self.cpu.mem_read(0xF9444C, 21) == b"LadderDismountTopEnd\0"

        # The synthetic actor and animation objects expose only these calls.
        self.cpu.mem_write(RETURN_FALSE, bytes.fromhex("31c0 c3"))
        self.cpu.mem_write(GET_ANIMATION, b"\xb8" + struct.pack("<I", ANIMATION) + b"\xc3")
        # Return a strong reference through the caller's output pointer.
        self.cpu.mem_write(GET_ANIMATION_REF, bytes.fromhex("8b442404 c700") +
                           struct.pack("<I", ANIMATION_REF) + bytes.fromhex("ff05") +
                           struct.pack("<I", ANIMATION_REF + 4) + bytes.fromhex("c20400"))
        self.cpu.mem_write(PLAY_ANIMATION, bytes.fromhex("c20400"))
        self.put(ACTOR, ACTOR_TABLE)
        self.put(ACTOR_TABLE + 0x214, RETURN_FALSE)
        self.put(ACTOR_TABLE + 0x2CC, GET_ANIMATION)
        self.put(ACTOR_TABLE + 0x480, GET_ANIMATION_REF)
        self.put(ANIMATION, ANIMATION_TABLE)
        self.put(ANIMATION_TABLE + 0xD4, PLAY_ANIMATION)
        self.put(ANIMATION_REF, ANIMATION, 3, 0)
        self.put(ACTOR_REF, ACTOR, 3, 0)
        self.put(ACTION, LADDER_TABLE)
        self.put(ACTION + 0x6E0, ACTOR_REF)
        self.put(0x1110304, 106, 105, TOP_END_ID, RUNG_END_ID, 102, 101, 0x3F)

        # Replace only external event forwarding and actor transform commits.
        # The traversal state, thresholds, branch selection and event handlers
        # below use the original pinned machine code.
        self.cpu.mem_write(0xB9BA10, bytes.fromhex("8b44240c ff4804 c20c00"))
        self.cpu.mem_write(0xE09F40, bytes.fromhex("c3"))
        self.cpu.mem_write(0xE09FA0, bytes.fromhex("c3"))
        self.animations = []
        self.transforms = []
        self.cpu.hook_add(UC_HOOK_CODE, self.record_animation, begin=PLAY_ANIMATION, end=PLAY_ANIMATION)
        self.cpu.hook_add(UC_HOOK_CODE, self.record_transform, begin=0xE09FA0, end=0xE09FA0)

    def put(self, address, *values):
        self.cpu.mem_write(address, struct.pack(f"<{len(values)}I", *values))

    def word(self, address):
        return struct.unpack("<I", self.cpu.mem_read(address, 4))[0]

    def record_animation(self, cpu, _address, _size, _data):
        target = cpu.reg_read(UC_X86_REG_ECX)
        assert target == ANIMATION
        self.animations.append(self.word(cpu.reg_read(UC_X86_REG_ESP) + 4))

    def record_transform(self, cpu, _address, _size, _data):
        self.transforms.append((cpu.reg_read(UC_X86_REG_EAX), self.word(cpu.reg_read(UC_X86_REG_ESP) + 4)))

    def run(self, entry, *arguments, end=STOP):
        self.put(STACK, STOP, *arguments)
        self.cpu.reg_write(UC_X86_REG_ESP, STACK)
        self.cpu.reg_write(UC_X86_REG_ECX, ACTION)
        self.cpu.emu_start(entry, end, count=4000)
        assert self.cpu.reg_read(UC_X86_REG_EIP) == end
        return self.cpu.reg_read(UC_X86_REG_EAX) & 255

    def mount(self):
        self.put(STACK + 8, ACTOR_REF)
        self.cpu.reg_write(UC_X86_REG_EBP, STACK)
        self.run(LADDER_MOUNT_CALL, end=0x82403B)
        assert self.word(TRAVERSAL_MODE) == 6

    def movement(self, lateral, vertical):
        self.cpu.mem_write(MOVEMENT, struct.pack("<6f", 0, lateral, 0, 0, 0, vertical))
        self.run(LADDER_INPUT, ACTOR_REF, MOVEMENT, 0, MOVEMENT, end=0x7DC59F)
        return struct.unpack("<f", self.cpu.mem_read(ACTION + 0x6FC, 4))[0]

    def update(self):
        self.put(ACTOR_REF + 4, 3)
        self.animations.clear()
        self.transforms.clear()
        result = self.run(LADDER_UPDATE, ACTOR_REF, 0)
        assert self.word(ACTOR_REF + 4) == 2
        assert self.word(ANIMATION_REF + 4) == 3
        return result

    def event(self, event_id):
        self.put(EVENT + 4, event_id)
        self.put(EVENT_REF, 0, 3, 0)
        self.transforms.clear()
        self.run(LADDER_EVENT, EVENT, 0, EVENT_REF)
        assert self.word(EVENT_REF + 4) == 2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    args = parser.parse_args()
    ladder = Fixture(args.exe)
    ladder.mount()
    ladder.put(CURRENT_RUNG, 1, 1, 4)
    ladder.put(ACTION_STATE, 2)

    assert ladder.movement(.25, 0) == 0
    assert ladder.update() == 1 and not ladder.animations
    assert ladder.word(CURRENT_RUNG) == 1 and ladder.word(ACTION_STATE) == 2
    assert ladder.movement(.25, -1) == -1
    assert ladder.update() == 1 and ladder.animations == [13, 13]
    assert ladder.word(NEXT_RUNG) == 0 and ladder.word(ACTION_STATE) == 3
    ladder.event(RUNG_END_ID)
    assert ladder.word(CURRENT_RUNG) == 0 and ladder.word(ACTION_STATE) == 2
    assert ladder.update() == 1 and ladder.animations == [14, 14]
    assert ladder.word(ACTION_STATE) == 1

    ladder.put(ACTION_STATE, 2)
    for rung in range(1, 5):
        assert ladder.movement(.25, 1) == 1
        expected_animation = 6 | ((rung - 1) & 1)
        assert ladder.update() == 1 and ladder.animations == [expected_animation]*2
        assert ladder.word(NEXT_RUNG) == rung and ladder.word(ACTION_STATE) == 3
        ladder.event(RUNG_END_ID)
        assert ladder.word(CURRENT_RUNG) == rung and ladder.word(ACTION_STATE) == 2
        assert ladder.word(TRAVERSAL_MODE) == 6

    # A reversed forward packet at the upper rung selects descent.
    assert ladder.movement(.25, -1) == -1
    assert ladder.update() == 1 and ladder.animations == [12, 12]
    assert ladder.word(NEXT_RUNG) == 3 and ladder.word(ACTION_STATE) == 3
    ladder.put(ACTION_STATE, 2)  # same upper rung, before the queued animation
    assert ladder.movement(.25, 1) == 1
    assert ladder.update() == 1 and ladder.animations == [8, 8]
    assert ladder.word(ACTION_STATE) == 1 and ladder.word(TRAVERSAL_MODE) == 6
    ladder.event(TOP_END_ID)
    assert ladder.transforms == [(ACTOR, ACTION + 0x50)]
    assert ladder.word(ACTION_STATE) == 4
    assert ladder.update() == 0 and not ladder.animations
    print(json.dumps(dict(exe_sha256=IMAGE_SHA256, mounted_mode=6, climbed_rungs=4,
                          downward_rungs=1, bottom_animation=14, reversed_top_animation=12,
                          top_animation=8, top_end_state=4,
                          terminal_update=0, method="x86 fixture with synthetic actor and animation events")))


if __name__ == "__main__":
    main()
