# Polish package — MAIN installation and recovery

Private owned-game assets: do not publish the ZIP or commit its payload to Git.
The dedicated SIDE branch contains builders, regressions and recovery tools.

## Contents and limits

- Six atlases contain all 18 Polish glyphs. Native alpha uses separate low/high
  DWORD planes. Original glyph pixel positions, metrics and sampled pixels, and
  other languages, remain intact. The menu atlas gains transparent rows with
  corresponding English UV scaling.
- All 14 language categories are processed. Existing approved Polish strings
  are retained. Other text uses offline NLLB and still needs Polish language
  review. Credits retain attributions. Tags/placeholders/newlines are validated.
- All 1,314 previously accepted Polish audio replacements are retained byte for
  byte. Two missing Vinnie lines gain Polish speech with exact native decoded
  sample counts and offline content checks; names/pronunciation need listening.
  Available baseline evidence contains **97 timing failures and 3 ADPCM
  cases**, rather than 39 timing failures. Every rejected replacement is listed
  in the manifest with native hashes and timing evidence where available.
- All 38 missing story clips are inventoried. Two confirmed spoken lines are
  replaced; the remaining 36 retain original audio. Original decoding and sample
  counts are validated; offline ASR drafts are included. Empty ASR is not proof
  of nonverbal content. Ambiguous clips remain a localization limitation.
- Native archive extraction and original resource integrity are checked offline.
  Full H.Misc extraction fails on both vanilla and candidate at the same
  unchanged 3,743-byte resource (QuickBMS script line 105). Its changed language
  resource is extracted and compared separately; the entire original cache
  prefix and unrelated TOC fields remain byte-identical.
  **Game startup and headset validation remain NOT RUN.** This package is a
  partial localization, not a claim that all speech is Polish.

## Verify and install — MAIN only

Use Python 3.10+. Substitute verified absolute paths. Close the game. Keep the
current VR DLL, input profiles and SteamVR settings unchanged. SIDE does not
deploy this package.

```powershell
python -B tools/polish_package_install.py verify --package C:\private\PolishPack
python -B tools/polish_package_install.py install --package C:\private\PolishPack --cache C:\verified\game\Cache.Windows --backup C:\private\PolishRollback
```

Installation accepts only complete hash-matching original or previously accepted
Polish preimages. Unknown/mixed caches fail before modification. All 16 files
are backed up and verified before replacement. Failure during replacement
triggers immediate restoration and records the result in `rollback.json`.
Keep the package and backup; stop if restoration reports a failure.

## Rollback

Close the game:

```powershell
python -B tools/polish_package_install.py rollback --backup C:\private\PolishRollback --cache C:\verified\game\Cache.Windows
```

Backup/current hashes are verified first. Unexpected later file changes are
protected: retain the backup and reconcile them before attempting restoration.

## Reproduce

Use the preserved private original caches/extractions, accepted audio caches,
the exact manifest source commit, Python and NumPy. The translation mapping is
included; reproduction requires no model, network or text-generation libraries.

```powershell
python -B tools/polish_rebuild.py --stage C:\private\fresh-rebuild --baseline C:\private\original\baseline --extract C:\private\original\extract --accepted-audio C:\private\accepted\candidate --translations C:\private\PolishPack\inputs\translations.json --manifest C:\private\PolishPack\manifest.json
```

Success requires all 16 rebuilt SHA-256 values to match. ZIP entries use sorted
names, fixed timestamps/permissions and fixed compression settings.
`SHA256SUMS` covers manifest, payload, inputs and packaged tools.

## Consolidated MAIN game test

Verify startup, Polish menu/subtitle diacritics, existing Polish speech, native
scene timing/lip sync and retained nonverbal clips in one normal session. Keep
the accepted performance/VR build. Restore this localization backup on startup
or audio regression. Runtime status remains NOT RUN until actual game evidence.


## Font-only recovery candidate after the failed full release

The full `251316ec` localization package is quarantined following the reported
17 FPS / 70 ms CPU session and hang at an enemy. That session also used an
incorrect VR DLL build flavor; it does not isolate an audio or font root cause.
Do not reinstall the full pack as part of the rendering recovery.

`polish_release.py --fonts-only` produces a separate private 16-file installer
package. Only the eight Font/Texture archive and TOC files come from the verified
six-atlas release. The other eight files, including Languages.cl and all speech,
come byte for byte from the immutable accepted rollback backup. No new audio
serialization, retiming, generated translations or DLL/INI changes are included.
Both source-package checksums and the complete backup/preimage receipt must pass.
It makes no writes to Steam and does not start the game.

```
python -B tools/polish_release.py --fonts-only --source-package <verified-full-private-package> --accepted-cache <accepted-rollback-directory> --accepted-receipt <rollback.json> --out <new-private-font-package>
```

The resulting manifest records both input receipt hashes and the unchanged text
and audio file hashes. The existing installer provides backup and rollback.
This isolates font validation in the next consolidated game session; it is not
proof that the enemy hang or runtime font rendering has been fixed. MAIN must
leave the user's current VR/FPS test untouched until that session ends.


## Runtime rejection: six-atlas font payload

The user tested font-only 39ba86e7 on the accepted VR DLL and supplied a PSVR2
image with colored fragments replacing native menu text. Font tables and atlas
previews passing offline did not establish the game's texture upload contract.
The offending B.Texture_en.cache SHA-256 is
4754337fc9d60b4475ac4a47b397ce153679feefe4437fc81faf3687a331151c.
The installer now rejects this payload, including attempts to repackage it as a
font-only release. Keep the original package and deployment receipts as evidence.
MAIN restored the complete accepted font preimage with matching hashes. Existing
Polish speech/text remain present; accented glyph rendering remains unresolved.
Do not redeploy this atlas based on another Python preview or checksum PASS.
The suspected planar-versus-driver block layout needs native upload evidence;
it is a hypothesis, not a confirmed root cause or an implemented repair.
