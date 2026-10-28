"""Audit two pinned functions and emit relocation metadata, never EXE bytes.

Reads the installed image as a file. Does not load/execute it, attach to a
process or touch Steam. Capstone independently recovers the complete reachable
CFG. Every direct branch stays inside its own block; every relative call is
an E8 rel32 site in the emitted plan. No local SEH, code-pointer immediate,
indirect jump, call/pop PC discovery or relative transfer is silently copied.
"""
import argparse
import hashlib
import json
from pathlib import Path

import capstone as cs
import capstone.x86_const as x86
import pefile

EXE_SHA = "d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1"
FUNCTIONS = [(0x888720, 0x554), (0x985D80, 0xF5E)]


def dominates(edges, entry, boundary, target):
    """Intraprocedural only: calls return normally; no callee-side-effect claim."""
    def reaches(blocked):
        pending, seen = [entry], set()
        while pending:
            node = pending.pop()
            if node == blocked or node in seen:
                continue
            if node == target:
                return True
            seen.add(node)
            pending.extend(edges.get(node, ()))
        return False
    return reaches(None) and not reaches(boundary)


def audit(pe, start, size):
    base = pe.OPTIONAL_HEADER.ImageBase
    decoder = cs.Cs(cs.CS_ARCH_X86, cs.CS_MODE_32)
    decoder.detail = True
    pending, instructions = [start], {}
    calls, returns, branches = [], [], []
    while pending:
        address = pending.pop()
        if address in instructions:
            continue
        assert start <= address < start + size, f"escaping CFG: {address:x}"
        instruction = next(decoder.disasm(pe.get_data(address-base, 16), address), None)
        assert instruction and address + instruction.size <= start + size
        instructions[address] = instruction
        assert instruction.mnemonic not in {"fstenv", "fnstenv", "fnsave", "fsave", "int", "sysenter", "iret"}
        for operand in instruction.operands:
            assert operand.type != x86.X86_OP_MEM or operand.mem.segment not in {x86.X86_REG_FS, x86.X86_REG_GS}
        if instruction.group(cs.CS_GRP_RET):
            returns.append(address)
            continue
        if instruction.group(cs.CS_GRP_JUMP):
            assert instruction.operands[0].type == x86.X86_OP_IMM, f"indirect jump: {address:x}"
            target = instruction.operands[0].imm
            assert start <= target < start + size, f"external jump: {address:x}"
            branches.append((address, target))
            pending.append(target)
            if instruction.mnemonic == "jmp":
                continue
        elif instruction.group(cs.CS_GRP_CALL):
            operand = instruction.operands[0]
            if operand.type == x86.X86_OP_IMM:
                assert instruction.size == 5 and instruction.bytes[0] == 0xE8
                assert not start <= operand.imm < start + size, "internal call/PC discovery"
                calls.append((address-start, operand.imm))
        else:
            for operand in instruction.operands:
                if operand.type == x86.X86_OP_IMM:
                    assert not any(a <= operand.imm < a+n for a, n in FUNCTIONS), f"code pointer: {address:x}"
                if operand.type == x86.X86_OP_MEM and not operand.mem.base and not operand.mem.index:
                    assert not any(a <= operand.mem.disp < a+n for a, n in FUNCTIONS), f"code memory: {address:x}"
        pending.append(address + instruction.size)
    # Reject overlapping decoding and transfers into the middle of instructions.
    prior_end = start
    for address, instruction in sorted(instructions.items()):
        assert address >= prior_end
        prior_end = address + instruction.size
    assert prior_end == start + size
    assert all(target in instructions for _, target in branches)
    edges = {}
    for address, instruction in instructions.items():
        successors = []
        if not instruction.group(cs.CS_GRP_RET):
            if instruction.group(cs.CS_GRP_JUMP):
                successors.append(instruction.operands[0].imm)
            if instruction.mnemonic != "jmp":
                successors.append(address + instruction.size)
        edges[address] = successors
    order = []
    if start == 0x985D80:
        # The private owner adapter displaces exactly CALL EAX, MOV ECX,[EAX],
        # CMP [ECX],0. No branch may enter either displaced post-call instruction.
        span_start, span_end = 0x985F75, 0x985F7C
        assert pe.get_data(span_start-base, span_end-span_start) == bytes.fromhex("ffd08b08833900")
        assert [(instructions[a].mnemonic, instructions[a].size)
                for a in (span_start, span_start+2, span_start+4)] == [("call", 2), ("mov", 2), ("cmp", 3)]
        assert all(not span_start < target < span_end for _, target in branches)
        assert span_end in instructions
        # Getter interception must be unavoidable on every normal path to the
        # native deep copy. Also prove ordering of two later direct consumers;
        # their semantic role and indirect earlier callees remain unproven.
        for boundary, target in ((0x98618E, 0x98619A),
                                 (0x98619A, 0x9863B1), (0x98619A, 0x9863C3)):
            assert target in instructions and dominates(edges, start, boundary, target), \
                f"frustum ordering bypass: {boundary:x} -> {target:x}"
            order.append([boundary, target])
    body = pe.get_data(start-base, size)
    return {"address": start, "size": size, "sha256": hashlib.sha256(body).hexdigest(),
            "instructions": len(instructions), "returns": returns,
            "branches": branches, "calls": sorted(calls), "dominance": order}


def main():
    # A real bypass, an unreachable consumer and a mandatory boundary differ.
    assert dominates({0: [1], 1: [2], 2: [3]}, 0, 1, 3)
    assert not dominates({0: [1, 2], 1: [3], 2: [3]}, 0, 1, 3)
    assert not dominates({0: [1], 1: []}, 0, 1, 3)
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True, type=Path)
    parser.add_argument("--header", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    assert hashlib.sha256(args.exe.read_bytes()).hexdigest() == EXE_SHA
    pe = pefile.PE(str(args.exe))
    assert pe.FILE_HEADER.Machine == 0x14C and pe.OPTIONAL_HEADER.ImageBase == 0x400000
    blocks = [audit(pe, start, size) for start, size in FUNCTIONS]
    assert (0x88896D-0x888720, 0x985D80) in blocks[0]["calls"]
    assert (0x98618E-0x985D80, 0x9962D0) in blocks[1]["calls"]
    assert (0x98619A-0x985D80, 0x935A70) in blocks[1]["calls"]
    text = ["#pragma once", "// Generated by test/native_view_clone_plan.py. Metadata only; no native bytes.",
            '#include "native_view_code_clone.h"', "namespace d2vr_native_clone {",
            "inline const Block viewBlocks[]={"]
    for block in blocks:
        text.append('    {reinterpret_cast<void*>(0x%X),0x%X,"%s"},' %
                    (block["address"], block["size"], block["sha256"]))
    text.extend(["};", "inline const Call viewCalls[]={"])
    for index, block in enumerate(blocks):
        for offset, target in block["calls"]:
            text.append("    {%d,0x%X,0x%X}," % (index, offset, target))
    text.extend(["};", "// Complete consecutive code-pointer tables plus the preceding RTTI/header word.",
                 "inline constexpr unsigned viewTableCounts[]={82,77};",
                 "inline const char* viewTableDigests[]={"])
    for table, count in ((0xFC8930, 82), (0xFC6800, 77)):
        for index in range(count):
            target = int.from_bytes(pe.get_data(table-0x400000+index*4, 4), "little")
            assert any(s.VirtualAddress <= target-0x400000 < s.VirtualAddress+s.Misc_VirtualSize and
                       s.Characteristics & 0x20000000 for s in pe.sections)
        text.append('    "%s",' % hashlib.sha256(pe.get_data(table-4-0x400000, (count+1)*4)).hexdigest())
    text.extend(["};", "}", ""])
    if args.header:
        args.header.write_text("\n".join(text), encoding="utf-8")
    report = {"exe_sha256": EXE_SHA, "blocks": blocks, "status": "STATIC_CFG_AUDIT_PASS_NOT_GAME_EXECUTION",
              "limitations": "Return addresses move with copied calls. Native callees may observe them; game compatibility and HMD output unproven. Not installed."}
    if args.report:
        args.report.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print("PASS pinned CFG:", [(hex(b["address"]), b["instructions"], len(b["calls"]), len(b["returns"])) for b in blocks],
          "; no indirect jumps/local SEH/code pointers; no game execution or hook publication")


if __name__ == "__main__":
    main()
