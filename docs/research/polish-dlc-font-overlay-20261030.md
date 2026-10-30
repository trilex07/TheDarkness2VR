# Polish glyphs: official patch font overrides

Headset feedback after ca3e75ae: readable ASCII, but Polish menu characters still rendered as asterisks. The base Cache.Windows font/atlas hashes matched the package. Steam language was English. Atlas coverage alone did not prove effective mounted font coverage.

The official Cache.DLC/PCPatchManifests/Cache.Windows directory also contains B.Font and H.Font: 63 live resources each, including 21 non-Japanese FontCache fonts. All 21 extracted bulk resources are byte-identical to their original base counterparts, without the added Polish codepoints. These official patch tables were omitted from previous releases. Twenty headers are identical; Corbel_26 has additional trailing official metadata, which is retained.

The correction appends the matching corrected bulk tables to the two DLC archives and changes only the English UV/metric counts in their headers. It preserves all original compressed archive prefixes, TOC identities and unrelated shader/Japanese resources. It does not change the DLL, INIs, EXE, base font atlases, text or speech.

Native menu glyph selection B03840 calls 524B90, which uses unsigned UTF-16 lower_bound at 6F6370 over the selected language's codepoint array. Missing codes use the recorded asterisk index. The offline gate verifies sorted codes and successful lookup for all original and 18 added Polish characters in each repaired override; it is not headset proof.

Reproduce privately:

```
python -B tools/polish_font_overlay.py --stage <DLC-stage> --base-stage <verified-base-font-stage> --out <new-package> --quickbms <owned-extractor> --bms-script <darkness2-script>
python -B -m unittest discover -s test -p "test_polish*.py"
```

The DLC stage needs baseline/{B.Font,H.Font}.{toc,cache} and their native extracted resources in extract/{B.Font,H.Font}. The base stage is the original/extracted/repaired ca3e75ae font stage. Game assets remain private.

Validation: 37 regressions PASS. Native QuickBMS round-trip of both DLC archives PASS: all 63 live resources per archive, 21 replacements per archive. Original prefixes and all unmodified resources preserved. No C++ or DLL changes; no x86 rebuild required for this data-only repair.

The four-file package requires all eight exact corrected base font archive hashes before installation. It verifies the complete original DLC preimage, backs up all four files, stages before replacing, verifies final SHA-256 and restores on failure. Explicit rollback is included. To roll back the combined stability/font candidate as well, restore this DLC overlay first, then use the earlier combined package rollback.

Runtime result: pending consolidated PSVR2 feedback. Do not claim Polish glyphs visually fixed until verified in menu and subtitles.
