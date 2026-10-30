# Native DXT3 font serialization correction

The headset-rejected atlas payload contained cache-filtered DWORD planes in an
uncompressed resource. The native reader at 0xA552A0 bypasses decompression when
stored and unpacked sizes match. The decompression path 0x7E56B0 -> 0x66C330
scatters planes into strided records. Texture format 2 resolves to DXT3 at
0xE11FB0, and 0xE120F0 reads the compressed-format surface bytes directly.
Evidence belongs to EXE SHA256
D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
Private decompilation and game assets are excluded from Git.

The corrected generator interleaves the four 4096-byte DWORD planes of each
16KB chunk into 1024 native 16-byte BC2 blocks before writing a raw resource.
It preserves the original font pixels, metrics and other language data.
The former rejected payload remains blocked by the installer.

Validation uses Pillow's independent DDS/BC2 decoder on the actual raw payload,
checks original RGBA pixels at every native glyph UV, validates 108 Polish glyphs
across six atlases, and reproduces/rejects the old corrupt layout. Existing
archive validation checks append-only replacements and untouched original bytes.
Native QuickBMS extraction of all four font archives and all live resources
passed on the corrected private stage. These checks do not prove headset output.

Build a corrected font-only package with explicit private inputs:

```powershell
python -B tools/polish_release.py --repaired-fonts --stage <font-stage> --accepted-cache <immutable-backup> --accepted-receipt <rollback.json> --out <new-package>
```

The stage contains only B.Font, H.Font, B.Texture_en, H.Texture_en archive pairs
under baseline and their original extracted resources under extract. Accepted
speech and text archive bytes are copied unchanged from the verified rollback
receipt. No DLL, INI, EXE, SteamVR setting or game process is changed by the builder.
Use the existing verified installer only while the game is closed, retaining its
new rollback package. Runtime font rendering remains pending headset acceptance.


## Complete English font coverage

The readable six-atlas build omitted 19 live English fonts, including Corbel and
Arial families. The generator now covers all 25 nonempty English atlases (450
Polish glyphs), and rejects an incomplete native texture inventory. Four empty
Japanese-only placeholders remain unchanged. Atlas growth is used only when
unused blocks cannot hold the added glyphs; original glyph pixels and metrics
remain validated with the native BC2 decoder. Headset acceptance remains pending.
