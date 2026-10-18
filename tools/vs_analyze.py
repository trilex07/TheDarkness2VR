#!/usr/bin/env python3
"""SM1-SM3 vertex/pixel shader bytecode analyzer for Darkness2VR.

Decodes D3D9 shader token streams (raw bytecode as dumped by HelixMod
ShaderOverride files or by our CreateVertexShader hook) and reports:
  - full disassembly
  - which registers feed oPos / oC0
  - matrix multiply patterns (m4x4/m3x4/dp4 chains) and their constant bases
  - DEF constants (HelixMod stereo injection constants etc.)

Usage: python vs_analyze.py <file.bin|file.txt> [...]
"""
import sys, struct, os

# --- opcode table (d3d9types.h) ---
OPS = {
    0: "nop", 1: "mov", 2: "add", 3: "sub", 4: "mad", 5: "mul", 6: "rcp", 7: "rsq",
    8: "dp3", 9: "dp4", 10: "min", 11: "max", 12: "slt", 13: "sge", 14: "exp",
    15: "log", 16: "lit", 17: "dst", 18: "lrp", 19: "frc", 20: "m4x4", 21: "m4x3",
    22: "m3x4", 23: "m3x3", 24: "m3x2", 25: "call", 26: "callnz", 27: "loop",
    28: "ret", 29: "endloop", 30: "label", 31: "dcl", 32: "pow", 33: "crs",
    34: "sgn", 35: "abs", 36: "nrm", 37: "sincos", 38: "rep", 39: "endrep",
    40: "if", 41: "ifc", 42: "else", 43: "endif", 44: "break", 45: "breakc",
    46: "mova", 47: "defb", 48: "defi",
    64: "texcoord", 65: "texkill", 66: "tex", 67: "texbem", 68: "texbeml",
    69: "texreg2ar", 70: "texreg2gb", 71: "texm3x2pad", 72: "texm3x2tex",
    73: "texm3x3pad", 74: "texm3x3tex", 75: "texm3x3diff", 76: "texm3x3spec",
    77: "texm3x3vspec", 78: "expp", 79: "logp", 80: "cnd", 81: "def",
    82: "texreg2rgb", 83: "texdp3tex", 84: "texm3x2depth", 85: "texdp3",
    86: "texm3x3", 87: "texdepth", 88: "cmp", 89: "bem", 90: "dp2add",
    91: "dsx", 92: "dsy", 93: "texldd", 94: "setp", 95: "texldl", 96: "breakp",
    0xFFFE: "comment", 0xFFFF: "end",
}
SRCMODS = {0: "", 1: "neg", 2: "bias", 3: "biasneg", 4: "sign", 5: "signneg",
           6: "comp", 7: "x2", 8: "x2neg", 9: "dz", 10: "dw", 11: "abs",
           12: "absneg", 13: "not"}
DESTMODS = {0: "", 1: "_sat", 2: "_pp", 16: "_bias", 17: "_bias_sat", 18: "_bx2",
            19: "_bx2_sat", 32: "_x2", 33: "_x2_sat", 48: "_dz", 49: "_dw", 64: "_abs"}
REGTYPES = {0: "r", 1: "v", 2: "c", 3: "a", 4: "oPos", 5: "oD", 6: "oT/o",
            7: "i", 8: "oC", 9: "oDepth", 10: "s", 11: "c2", 12: "c3", 13: "c4",
            14: "c5", 15: "loopcount", 16: "tempfloat16", 17: "misc", 18: "label",
            19: "p"}
USAGE = {0: "position", 1: "blendweight", 2: "blendindices", 3: "normal",
         4: "psize", 5: "texcoord", 6: "tangent", 7: "binormal", 8: "tessfactor",
         9: "positiont", 10: "color", 11: "fog", 12: "depth", 13: "sample"}
VERSIONS = {0xFFFE: "vs/ps", 0xFFFF: "dx9"}

def regtype_num(tok):
    return ((tok & 0x70000000) >> 28) | ((tok & 0x1800) >> 8)

def reg_name(tok, is_src):
    rn = tok & 0x7FF
    rt = regtype_num(tok)
    base = REGTYPES.get(rt, "t%d" % rt)
    if rt == 4:   # RASTOUT: oPos/oFog/oPts
        base = {0: "oPos", 1: "oFog", 2: "oPts"}.get(rn, "oR%d" % rn)
        return base
    if rt == 5:
        return "oD%d" % rn
    if rt == 6:
        return "o%d" % rn
    if rt == 8:
        return "oC%d" % rn
    if rt == 10:
        return "s%d" % rn
    name = "%s%d" % (base, rn)
    if is_src:
        sw = (tok >> 16) & 0xFF
        comp = "xyzw"
        sel = "".join(comp[(sw >> (2 * i)) & 3] for i in range(4))
        sm = SRCMODS.get((tok >> 24) & 0xF, "?")
        if sel != "xyzw" or sm:
            name += ".%s%s" % (sm, sel)
    else:
        wm = (tok >> 16) & 0xF
        if wm != 0xF:
            name += "." + "".join("xyzw"[i] for i in range(4) if wm & (1 << i))
        dm = DESTMODS.get((tok >> 20) & 0xF, "?")
        name += dm
    return name

def parse(data):
    if len(data) < 4:
        return None
    words = list(struct.unpack("<%dI" % (len(data) // 4), data[:len(data) // 4 * 4]))
    if (words[0] & 0xFFFE) != 0xFFFE:  # version token 0xFFFE/0xFFFF
        return None
    major = (words[0] >> 8) & 0xFF
    minor = words[0] & 0xFF
    kind = "vs" if (words[0] >> 16) == 0xFFFE and (words[0] & 0xFFFF0000) else "ps"
    kind = "ps" if (words[0] & 0xFFFF0000) == 0xFFFF0000 else "vs"
    out = []
    i = 1
    defs = []
    instrs = []
    while i < len(words):
        tok = words[i]
        op = tok & 0xFFFF
        if op == 0xFFFE:  # comment
            clen = (tok >> 16) & 0x7FFF
            body = words[i + 1:i + 1 + clen]
            cstr = b""
            for w in body:
                cstr += struct.pack("<I", w)
            try:
                cstr = cstr.split(b"\x00")[0].decode("ascii", "replace")
            except Exception:
                cstr = repr(cstr[:60])
            out.append(("comment", i, cstr))
            i += 1 + clen
            continue
        if op == 0xFFFF:
            out.append(("end", i, ""))
            break
        oplen = (tok >> 24) & 0xF
        name = OPS.get(op, "op%d" % op)
        args = words[i + 1:i + 1 + oplen]
        coissue = bool(tok & 0x40000000)
        pred = (tok >> 28) & 0x1
        instrs.append((i, name, args, coissue, pred))
        out.append(("ins", i, (name, args, coissue, pred)))
        if name == "def":
            vals = struct.unpack("<4f", struct.pack("<4I", *args[1:5]))
            defs.append((reg_name(args[0], False), vals))
        i += 1 + oplen
    return {"version": "%s_%d_%d" % (kind, major, minor), "items": out,
            "instrs": instrs, "defs": defs, "words": words}

def analyze(path):
    with open(path, "rb") as f:
        data = f.read()
    sh = parse(data)
    if not sh:
        print("!! %s: not a shader token stream" % path)
        return
    print("=" * 78)
    print("%s  [%s]  %d bytes" % (os.path.basename(path), sh["version"], len(data)))
    for name, vals in sh["defs"]:
        print("  DEF %-8s (%g, %g, %g, %g)" % (name, *vals))

    # Disassembly (bounded)
    disasm = []
    for (idx, name, args, co, pred) in sh["instrs"]:
        if name == "def":
            regs = reg_name(args[0], False)
            vals = struct.unpack("<4f", struct.pack("<4I", *args[1:5]))
            disasm.append("  %5d: def %s, %g, %g, %g, %g" % (idx, regs, *vals))
            continue
        parts = []
        if name == "dcl":
            usage = (args[0] >> 27) & 0xF
            usw = (args[0] >> 24) & 0x7
            parts.append(USAGE.get(usage, "usage%d" % usage) + "[%d]" % usw)
            parts.append(reg_name(args[1], False))
        elif name in ("call", "callnz", "label"):
            parts = [str(a) for a in args]
        elif name == "sincos" and len(args) == 5:
            parts = [reg_name(a, k > 0) for k, a in enumerate(args)]
        else:
            parts = [reg_name(a, k > 0) for k, a in enumerate(args)]
        disasm.append("  %5d: %s%s %s" % (idx, name, (".p" if pred else ""), ", ".join(parts)))

    # Output-write analysis
    opidx = {}
    for n in range(12):
        opidx.setdefault(n, [])
    writes = []
    for (idx, name, args, co, pred) in sh["instrs"]:
        if not args or name in ("def", "defb", "defi", "dcl", "ret", "call", "callnz",
                                "label", "end", "break", "breakc", "breakp", "if",
                                "ifc", "else", "endif", "rep", "endrep", "loop",
                                "endloop", "comment", "nop", "texkill"):
            continue
        dst = args[0]
        rt = regtype_num(dst)
        rn = dst & 0x7FF
        outreg = None
        if rt == 4 and rn == 0:
            outreg = "oPos"
        elif rt == 6 and rn == 0:
            outreg = "o0(pos)"
        elif rt == 8:
            outreg = "oC%d" % rn
        if outreg:
            writes.append((idx, name, outreg, args))
    print("  -- output writes --")
    for (idx, name, outreg, args) in writes:
        srcs = ", ".join(reg_name(a, k > 0) for k, a in enumerate(args[1:]))
        print("  %5d: %-6s %-8s <- %s" % (idx, name, outreg, srcs))

    # Matrix pattern detection: m4x4/m3x4 dest, src, cBase
    print("  -- matrix transforms --")
    for (idx, name, args, co, pred) in sh["instrs"]:
        if name in ("m4x4", "m4x3", "m3x4", "m3x3", "m3x2") and len(args) >= 3:
            cb = args[2]
            print("  %5d: %s %s, %s, %s  (const base %s)" %
                  (idx, name, reg_name(args[0], False), reg_name(args[1], True),
                   reg_name(cb, True), reg_name(cb, True).split(".")[0]))
    # dp4 chains into oPos
    print("  -- dp4 chain feeding outputs --")
    for (idx, name, outreg, args) in writes:
        if name == "dp4":
            srcs = [reg_name(a, True) for a in args[1:]]
            print("  %5d: dp4 %s <- %s" % (idx, outreg, " * ".join(srcs)))

    print("  -- disassembly --")
    for line in disasm:
        print(line)

def main():
    for p in sys.argv[1:]:
        analyze(p)

if __name__ == "__main__":
    main()
