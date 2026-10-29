"""Build a private Polish pack from verified vanilla caches; never edits Steam.

Inputs contain owned game assets and must stay outside Git.  Extraction uses
QuickBMS separately.  Deployment is a separate hash-checked transaction.
"""
from __future__ import annotations

import argparse
import ast
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import wave


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(path):
    with open(path, "rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


class Cache:
    """96-byte TOC entries: stored size at +16, unpacked +20, padding +24."""
    def __init__(self, root, stem):
        self.root, self.stem = Path(root), stem
        self.toc = bytearray((self.root / (stem + ".toc")).read_bytes())
        require(self.toc[:4] == bytes.fromhex("4ec66718"), "TOC magic")
        require((len(self.toc) - 8) % 96 == 0, "TOC length")
        self.entries = {}
        dirs = [""]
        limit = (self.root / (stem + ".cache")).stat().st_size
        for pos in range(8, len(self.toc), 96):
            off, stamp, packed, size, padding, parent = struct.unpack_from("<QQIIII", self.toc, pos)
            name = self.toc[pos+32:pos+96].split(b"\0", 1)[0].decode("utf-8")
            require(name and not any(c in name for c in ("/", "\\", ":")) and name not in (".", ".."), "Unsafe TOC component")
            require(parent < len(dirs), "TOC directory reference")
            path = (dirs[parent] + "/" + name).lstrip("/")
            if off == 0xffffffffffffffff:
                dirs.append(path)
            elif stamp:
                require(off + packed <= limit, "TOC payload bounds")
                require(path not in self.entries, "Duplicate live TOC path: " + path)
                self.entries[path] = (pos, off, packed, size)

    def write(self, out, replacements):
        require(set(replacements) <= self.entries.keys(), "Unknown replacement")
        out = Path(out)
        out.mkdir(parents=True, exist_ok=True)
        dst = out / (self.stem + ".cache")
        shutil.copyfile(self.root / dst.name, dst)
        toc = bytearray(self.toc)
        with dst.open("ab") as f:
            for path, data in sorted(replacements.items()):
                pos = self.entries[path][0]
                # Preserve timestamp, directory identity, name and unrelated entries.
                struct.pack_into("<Q", toc, pos, f.tell())
                struct.pack_into("<III", toc, pos+16, len(data), len(data), 0)
                f.write(data)
        (out / (self.stem + ".toc")).write_bytes(toc)
        rebuilt = Cache(out, self.stem)
        require(rebuilt.entries.keys() == self.entries.keys(), "Changed TOC identities")
        with dst.open("rb") as f:
            for path, data in replacements.items():
                _, off, packed, size = rebuilt.entries[path]
                f.seek(off)
                require(packed == size == len(data) and f.read(size) == data, "Payload readback")
        allowed = set()
        for path in replacements:
            pos = self.entries[path][0]
            allowed.update(range(pos, pos+8))
            allowed.update(range(pos+16, pos+28))
        require(all(a == b or i in allowed for i, (a, b) in enumerate(zip(self.toc, toc))), "Unrelated TOC mutation")


def chunks(data):
    require(data[:4] == b"RIFF" and data[8:12] == b"XWMA", "Not native RIFF xWMA (ASF is unsupported)")
    require(struct.unpack_from("<I", data, 4)[0] + 8 == len(data), "RIFF length")
    result, pos = {}, 12
    while pos < len(data):
        require(pos+8 <= len(data), "Truncated RIFF chunk")
        tag, size = struct.unpack_from("<4sI", data, pos)
        require(tag not in result and pos+8+size <= len(data), "RIFF chunk bounds")
        result[tag] = data[pos+8:pos+8+size]
        pos += 8 + size + (size & 1)
    require(pos == len(data) and {b"fmt ", b"dpds", b"data"} <= result.keys(), "RIFF chunks")
    return result


def audio_format(header, bulk, stream_size, resident=False):
    candidates = []
    for p in range(8, len(header)-79):
        rate = struct.unpack_from("<I", header, p)[0]
        if rate not in (22050, 32000, 44100, 48000) or header[p+4] != 16 or header[p+5] not in (1, 2):
            continue
        channels = header[p+5]
        samples, avg, align = struct.unpack_from("<III", header, p+6)
        count, size = struct.unpack_from("<II", header, p+26)
        bulk_size, _, annotation_size = struct.unpack_from("<III", header, p+55)
        if not samples or (stream_size is not None and size != stream_size) or bulk_size != len(bulk):
            continue
        if resident and header[p+37] != 0:
            continue
        if count == 0:
            valid = avg == align == 0 and size == samples*channels*2
        else:
            valid = avg in (2500, 4000, 6000, 8000, 12000, 20000, 24000) and count*align == size
            valid &= (len(bulk) >= size + count*4) if resident else (annotation_size + count*4 == len(bulk))
            valid &= struct.unpack_from("<II", header, p+67) == (size, size)
            valid &= struct.unpack_from("<I", header, p+59)[0] == len(bulk)
        if valid and resident:
            if count:
                table_start = len(bulk)-size-count*4
                dpds = struct.unpack_from("<"+str(count)+"I", bulk, table_start)
                valid &= dpds[-1] == samples*channels*2 and all(a <= b for a, b in zip((0,)+dpds, dpds))
            else:
                valid &= len(bulk) >= size
        if valid:
            candidates.append((p, rate, channels, samples, avg, align, count, annotation_size))
    require(len(candidates) == 1, "Ambiguous/unsupported audio serialization")
    return candidates[0]


def patch_xwma(header, bulk, stream_size, encoded):
    p, rate, channels, _, avg, align, count, annotation = audio_format(header, bulk, stream_size)
    require(count > 0, "Compressed patch on PCM")
    parts = chunks(encoded)
    fmt = struct.unpack("<HHIIHHH", parts[b"fmt "])
    require(fmt == (0x161, channels, rate, avg, align, 16, 0), "Encoder changed native codec format")
    data, dpds = parts[b"data"], parts[b"dpds"]
    require(len(dpds) % 4 == 0 and len(data) % align == 0, "Packet alignment")
    new_count = len(data)//align
    require(len(dpds) == new_count*4 and new_count > 0, "Packet table count")
    cumulative = struct.unpack("<" + str(new_count) + "I", dpds)
    require(all(a <= b for a, b in zip((0,) + cumulative, cumulative)), "Decoded packet table order")
    require(cumulative[-1] > 0 and cumulative[-1] % (channels*2) == 0, "Decoded sample alignment")
    new_bulk = bulk[:annotation] + dpds
    result = bytearray(header)
    fields = {6: cumulative[-1]//(channels*2), 26: new_count, 30: len(data),
              55: len(new_bulk), 59: len(new_bulk), 67: len(data), 71: len(data)}
    for relative, value in fields.items():
        struct.pack_into("<I", result, p+relative, value)
    allowed = {i for relative in fields for i in range(p+relative, p+relative+4)}
    require(all(a == b or i in allowed for i, (a, b) in enumerate(zip(header, result))), "Unrelated audio header mutation")
    require(new_bulk[:annotation] == bulk[:annotation], "Changed native phonemes")
    audio_format(result, new_bulk, len(data))
    return bytes(result), new_bulk, data


def command(args):
    res = subprocess.run([str(a) for a in args], capture_output=True)
    require(res.returncode == 0, "Tool failed: " + res.stderr.decode(errors="replace")[-1200:])
    return res


def build_audio(stage, voices, ffmpeg, encoder, workers):
    caches = {s: Cache(stage/"baseline", s) for s in ("H.Misc_en", "B.Misc_en", "F.Misc_en")}
    output = {s: {} for s in caches}
    work = stage/"encoded"
    work.mkdir(exist_ok=True)
    jobs, skipped = [], []
    for path in caches["H.Misc_en"].entries:
        voice = voices/(Path(path).name.removesuffix("_en.wav") + ".wav")
        hp, bp = stage/"extract/H.Misc_en"/path, stage/"extract/B.Misc_en"/path
        if not voice.is_file() or not hp.is_file() or not bp.is_file():
            continue
        header, bulk = hp.read_bytes(), bp.read_bytes()
        resident = path not in caches["F.Misc_en"].entries
        size = None if resident else caches["F.Misc_en"].entries[path][3]
        try:
            fmt = audio_format(header, bulk, size, resident)
            if resident:
                size = struct.unpack_from("<I", header, fmt[0]+30)[0]
        except ValueError as e:
            skipped.append({"path": path, "reason": str(e)})
            continue
        jobs.append((path, voice, header, bulk, size, fmt, resident))

    def encode(job):
        path, voice, header, bulk, size, fmt, resident = job
        p, rate, channels, samples, avg, align, count, _ = fmt
        key = hashlib.sha256(path.encode()).hexdigest()[:16]
        wav, encoded, decoded = (work/(key+ext) for ext in (".wav", ".xwma", ".decoded.wav"))
        with wave.open(str(voice)) as v:
            duration = v.getnframes()/v.getframerate()
        def prepare_pcm(frames):
            # Preserve scene timing; speed up long utterances, pad short ones.
            tempo = max(1.0, duration/(frames/rate))
            filters = []
            while tempo > 2:
                filters.append("atempo=2")
                tempo /= 2
            if tempo > 1.000001:
                filters.append("atempo=" + str(tempo))
            filters += ["apad", "atrim=end_sample="+str(frames)]
            command([ffmpeg, "-v", "error", "-y", "-i", voice, "-ar", rate, "-ac", channels,
                     "-af", "aresample="+str(rate)+","+",".join(filters), "-c:a", "pcm_s16le", wav])
            with wave.open(str(wav)) as v:
                require(v.getnframes() == frames and v.getnchannels() == channels and v.getsampwidth() == 2, "PCM duration")
                return v.readframes(frames)
        raw = prepare_pcm(samples)
        if not count:
            require(len(raw) == size, "PCM stream size")
            return path, header, (bulk[:-size]+raw if resident else bulk), (None if resident else raw), ("resident PCM" if resident else "PCM")
        command([encoder, "-b", avg*8, wav, encoded])
        if resident:
            # Embedded streams have serialized offsets whose ownership is not
            # changed here. Keep packet count, stream bytes and all blob offsets
            # exactly identical by fitting the PCM duration to that packet count.
            lower, upper, frames = 1, samples*3, samples
            for attempt in range(25):
                parts = chunks(encoded.read_bytes())
                actual = len(parts[b"data"])//align
                if actual == count:
                    break
                if actual < count:
                    lower = max(lower, frames+1)
                else:
                    upper = min(upper, frames-1)
                require(lower <= upper, "Cannot fit resident packet count: " + path)
                frames = (lower+upper)//2
                prepare_pcm(frames)
                command([encoder, "-b", avg*8, wav, encoded])
            require(len(parts[b"data"]) == size and len(parts[b"dpds"]) == count*4, "Resident stream dimensions")
            require(struct.unpack("<HHIIHHH", parts[b"fmt "]) == (0x161, channels, rate, avg, align, 16, 0), "Resident codec changed")
            dpds = struct.unpack("<"+str(count)+"I", parts[b"dpds"])
            require(all(a <= b for a, b in zip((0,)+dpds, dpds)) and dpds[-1] > 0, "Resident packet table")
            require(abs(dpds[-1]/(channels*2)-samples) <= max(4096, samples*.10),
                    "Resident dialogue timing drift: " + path)
            b = bulk[:len(bulk)-size-count*4]+parts[b"dpds"]+parts[b"data"]
            h = bytearray(header)
            struct.pack_into("<I", h, p+6, dpds[-1]//(channels*2))
            require(len(b) == len(bulk) and h[:p+6] == header[:p+6] and h[p+10:] == header[p+10:], "Resident layout changed")
            audio_format(h, b, size, True)
            h, f = bytes(h), None
        else:
            h, b, f = patch_xwma(header, bulk, size, encoded.read_bytes())
        # Decode every generated stream, checking actual PCM output, not just exit code.
        command([encoder, encoded, decoded])
        parts = chunks(encoded.read_bytes())
        with wave.open(str(decoded)) as v:
            require(v.getnchannels() == channels and v.getframerate() == rate and v.getnframes() > 0, "xWMA round trip")
            decoded_bytes = v.getnframes()*channels*2
        last = struct.unpack_from("<I", parts[b"dpds"], len(parts[b"dpds"])-4)[0]
        require(decoded_bytes == last, "xWMA decoded byte count")
        return path, h, b, f, ("resident xWMA" if resident else "xWMA")

    matched_live = len(jobs)+len(skipped)

    def checked_encode(job):
        try:
            return encode(job)
        except ValueError as e:
            if not job[-1]:
                raise
            # Reject unsafe embedded replacements without touching the original
            # line; do not claim unsupported dialogue was translated.
            return job[0], None, None, None, str(e)

    counts = Counter()
    with ThreadPoolExecutor(max_workers=workers) as pool:
        for i, (path, h, b, f, kind) in enumerate(pool.map(checked_encode, jobs), 1):
            if h is None:
                skipped.append({"path": path, "reason": kind})
                continue
            for stem, data in (("H.Misc_en", h), ("B.Misc_en", b), ("F.Misc_en", f)):
                if data is not None:
                    output[stem][path] = data
            counts[kind] += 1
            if i % 100 == 0:
                print("Audio", i, "/", len(jobs), flush=True)
    require(jobs, "No audio replacements")
    for stem, cache in caches.items():
        cache.write(stage/"candidate", output[stem])
    return {"replaced": sum(counts.values()), "formats": dict(counts), "unsupported": skipped,
            "voice_files": len(list(voices.glob("*.wav"))), "matched_live_assets": matched_live}


def category(data, marker):
    require(data.count(marker) == 1, "Ambiguous language category")
    p = data.index(marker) + len(marker)
    version, count, size = struct.unpack_from("<III", data, p)
    strings = data[p+12:p+12+size].split(b"\0")
    require(strings[-1] == b"" and len(strings) == count+1, "Language string count")
    entries, cursor = [], p+12+size
    for i in range(count):
        start = cursor
        n = struct.unpack_from("<I", data, cursor)[0]
        cursor += 4
        key = data[cursor:cursor+n].decode("latin1")
        cursor += n
        length = struct.unpack_from("<H", data, cursor+9)[0]
        require(length == len(strings[i]), "Language byte length")
        cursor += 11
        entries.append((key, strings[i].decode("utf-8"), data[start:cursor]))
    return p, cursor, version, entries


def translate(data, marker, translations):
    p, end, version, entries = category(data, marker)
    strings, index, replaced, retained = bytearray(), bytearray(), 0, []
    token = re.compile(r"<[^>]+>|%(?:\d+\$)?[-+0 #]*\d*(?:\.\d+)?[sdifu]|%%")
    for key, text, original in entries:
        replacement = translations.get(key, text)
        if Counter(token.findall(text)) != Counter(token.findall(replacement)):
            retained.append(key)
            replacement = text
        raw = replacement.encode("utf-8")
        require(len(raw) <= 65535 and b"\0" not in raw, "Translation size")
        strings += raw + b"\0"
        index += original[:-2] + struct.pack("<H", len(raw))
        replaced += replacement != text
    result = data[:p] + struct.pack("<III", version, len(entries), len(strings)) + strings + index + data[end:]
    _, _, _, checked = category(result, marker)
    require([e[0] for e in entries] == [e[0] for e in checked], "Language keys changed")
    return result, {"replaced": replaced, "retained_for_tokens": retained}


def build_text(stage, manifest, translations_source):
    tree = ast.parse(translations_source.read_text(encoding="utf-8-sig"))
    maps = [ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
            and any(isinstance(t, ast.Name) and t.id == "MENU_TRANSLATIONS" for t in n.targets)]
    require(len(maps) == 1, "Menu translation input")
    story = {}
    for line in manifest.read_text(encoding="utf-8").splitlines():
        item = json.loads(line)
        story[item["key"]] = item.get("text_pl", item.get("text_en", ""))
    path = next((stage/"extract/H.Misc").rglob("Languages.cl"))
    data = path.read_bytes()
    data, menu_report = translate(data, b"/D2/Language/Menu/", maps[0])
    data, story_report = translate(data, b"/D2/Language/SPStoryDialog/", story)
    cache = Cache(stage/"baseline", "H.Misc")
    keys = [p for p in cache.entries if Path(p).name == "Languages.cl"]
    require(len(keys) == 1, "Languages.cl live identity")
    cache.write(stage/"candidate", {keys[0]: data})
    return {"menu": menu_report, "story": story_report}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument("--voices", type=Path, required=True)
    parser.add_argument("--ffmpeg", type=Path, required=True)
    parser.add_argument("--encoder", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--translations-source", type=Path, required=True)
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--part", choices=("audio", "text", "fonts", "all"), default="all")
    args = parser.parse_args()
    stage = args.stage
    report_path = stage/"build-report.json"
    report = json.loads(report_path.read_text()) if report_path.exists() else {}
    if args.part in ("audio", "all"):
        report["audio"] = build_audio(stage, args.voices, args.ffmpeg, args.encoder, args.workers)
    if args.part in ("text", "all"):
        report["text"] = build_text(stage, args.manifest, args.translations_source)
    if args.part in ("fonts", "all"):
        from polish_fonts import build_fonts
        report["fonts"] = build_fonts(stage)
    report["files"] = {p.name: sha(p) for p in sorted((stage/"candidate").glob("*"))}
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps({k: v for k, v in report.items() if k != "files"}, ensure_ascii=True), flush=True)


if __name__ == "__main__":
    main()
