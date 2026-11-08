# Handoff: player ladder and language feedback

Date: 2026-11-08. Task: ladder-language-20261108. Role MAIN (explicit user).
Branch: codex/ladder-language-20261108.
Worktree: C:/Users/Trilex/Darkness2VR/.side-mod-audit-20261107.
Base: 5f0d5ed9. Current installed before this task: 642704E5 DLL, prior head
movement / weapon-laser candidate; historical BASELINE acceptance is distinct.
Read BASELINE, WORKFLOW and FAILED-APPROACHES. No main merge or release.

## User scope

Player uses Pico, without eye tracking, and a build preceding the recent
tentacle aim / throw changes. Prioritize ladder dismount and unwanted Polish
menu text, including language selection inside VR Settings. Ladder is after
defeating Swifty; no save supplied. Do not classify these reports as regressions
of the newest candidate. Golden dot identity is unconfirmed from screenshots.

## Confirmed language causes and implemented repair

- Panel labels were hardcoded Polish, independently of PolishText.
- Runtime and language helper treated absent PolishText/PolishVoice as ON.
- Public installer core INI explicitly shipped both flags as 1; its text
  choice did not override them. English native-menu module was not guaranteed.
- Panel now uses the text-language flag for all pages, controls, errors and
  prompts. Language page explicitly offers Polish game/VR text and Polish
  voice independently. Missing flags default OFF / English. Panel changes
  immediately; existing verified helper applies game assets after exit.
- Installer counterpart: C:/Users/Trilex/vr-mods/TrilexModInstaller,
  codex/darkness-language-20261108, local commit 029e63e. Choice-derived flags
  are applied after package integrity checks and before prepared receipt hashes
  in both install/export. English native-menu module selected for EN, Polish
  text module for PL. No Polish audio module added. 100 tests PASS.
- Public packaging gap remains: its four-file core lacks the helper/profiles
  used by this private Steam installation. Full public in-menu switching must
  not be advertised as finished until profiles are included/reconstructed and
  tested. No release or external push in this task.

## Fresh validation

- Settings and geometry 5787; renderer actual EN/PL TextOut calls, all pages,
  English-only ASCII labels and localized errors 1754; host routing 50;
  submission 7215; preferences/missing defaults/independent flags 29;
  weapon-laser gates 9248. PASS.
- Language helper: four combinations + absent keys -> EN + live-owner wait +
  tampered-profile rejection without writes. PASS (synthetic fixtures).
- Protected-native full x86 build and candidate profile assertion PASS.
- DLL: build/language-fix-final-20261108/d3d9.dll,
  SHA256 9F7222AE8BCEE180D5EA29E1F914F948C5E1E811C86877351A5B8EC7A4773F90.
- Updated helper SHA256
  F39297CF988F0936A53D0FCE3DAFBBF0BCABAE73AF93CE04F1C3880319B4628A.
- No game launched or physical headset acceptance in this task.

## Ladder: evidence, not a fix

Installed DarknessII.exe SHA256 remains D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
Read-only PE section parsing / Capstone disassembly found:

- D2LadderAction string F68C54 registered EC5990 with descriptor 1110328.
- LadderDismountTopEnd string F9444C is interned by 8212BC -> 8DF2B0;
  resulting token at 111030C is compared in event handler 821230.
- 82136D..8213C5 top-end branch obtains the mounted actor through action+6E0,
  calls E09F40/E09FA0 for its transforms and sets action+798 to 4.
- Associated vtable span begins FFCF80 (event handler at +2D8), written by
  AE1993; constructor AE1980 calls 5C4840 and initializes +6E0 actor reference.
- AC7AB0 update checks state+798 ==4 at AC7AFA..AC7B03 and returns the finished
  path. At state2, positive action+6FC uses A6ECF0 then A0AF60; negative uses
  4AED20 then 79E020. Rung transition sets state3 at AC7E6E, end-of-rung event
  sets state2 at 821361. These are observations, not root-cause proof.
- This inspection does NOT establish whether the player falls, remounts,
  reverses input, misses an animation event, or sees a camera-only return.
  No movement/physics/animation offsets or native bytes have been patched.
- REA Ghidra startup exceeded its 330000ms deadline; fallback was local static
  parsing, not guessed offsets. REA close_binary completed. No analysis target
  process or Steam process was started.

## Deployment and next steps

Source commit 292491a1c921796cba083d2a0acfc78829eac460 frozen and installed
as MAIN. Package: build/language-fix-steam-20261108. Install succeeded and
post-install VerifyOnly PASS: both payload hashes match, exact rollback payloads
match and all nine protected files are unchanged. Receipt: installed.json.
Game/headset execution remains NOT RUN. Public installer changes remain local.
tools/prepare-language-fix-candidate.ps1 pins exact current DLL 642704E5 and
helper 5937C0B7, provides both rollback payloads and protects nine other files,
including INIs, caches and language manifest. It changes only DLL + helper.

Ladder remains OPEN. Next is an actual after-Swifty reproduction comparing
native keyboard/physical-pad forward input with Pico forwarding and checking
whether the top-end event and actor/world position complete. Preserve saves,
configs and active user's Steam session. WORKFLOW.md says: "Do not inject input
without explicit authorization"; MAIN role alone does not establish synthetic
gameplay-input approval. Need user authorization before driving that scene.
