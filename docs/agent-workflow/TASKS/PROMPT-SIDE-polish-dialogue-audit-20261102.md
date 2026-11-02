# Prompt dla SIDE: ponowny audyt polskiego dubbingu

Jesteś SIDE, pracujesz offline w osobnym worktree i na dedykowanej gałęzi.
MAIN prowadzi aktywny test Steam. Nie uruchamiaj/zamykaj gry, nie podmieniaj
plików Steam/AppData, nie przełączaj checkoutu MAIN i nie dotykaj jego zmian.
Nie publikuj prywatnych nagrań ani archiwów gry w Git/zdalnych serwisach.

Zadanie: audyt CAŁEGO aktualnie zainstalowanego polskiego dubbingu: tempo,
ucięte końcówki/początki, czytelność, zgodność tekstu i znaczenia, właściwy głos.
Najpierw raport i dowody. Użytkownik preferuje skrócenie polskiego tekstu oraz
ponowną syntezę naturalnej wypowiedzi zamiast przyspieszania lub cięcia słów.

Rozpocznij od AGENTS.md, bootstrap.ps1, BASELINE.md, FAILED-APPROACHES.md i
HANDOFF-villa-cinema-hands-audio-20261102.md w docs/agent-workflow/handoffs.
MAIN checkout: C:/Users/Trilex/Darkness2VR/.main-camera-shake.
Source runtime44d9e7ba; stan udokumentowany b228b58e. Bieżące zmiany MAIN mogą
dotyczyć kina i rąk: nie przejmuj ich. Stwórz własny per-task handoff.

Konkretne przykłady użytkownika:
1. Wyjście z windy w willi: „co tylko będziesz potrzebował, Dżeki”.
   Silne dopasowanie: Sd02170Vinnie, English „Yeah. Sure. Whatever you need,
   Jackie.” Manifest PL ma błędne „Jak tylko będziesz potrzebowała, Jackie.”
   Potwierdź rzeczywisty zainstalowany tekst/audio i budżet natywnej kwestii.
2. S03MansionI rozmowa o tym, czy szef jest wariatem, podejście Jackiego:
   Sd02690Wiseguy1, Sd02700Wiseguy2, Sd02710Wiseguy1, Sd02720Jackie,
   Sd02730Wiseguy1, Sd02740Wiseguy2. MAIN zainstalował sześć skróconych nagrań
   i zgodne napisy; użyj ich jako aktualnego stanu, nie nadpisuj starymi WAV.

Źródła (czytaj, zapisuj tylko we własnym miejscu roboczym):
- Gra: C:/Program Files (x86)/Steam/steamapps/common/Darkness II/Cache.Windows.
- Oryginały/manifesty/raw TTS: C:/Users/Trilex/Darkness2VR/audio/polish,
  story_en_reference, story_voicepack_xlingual, manifest_story_pl.jsonl.
- Native baseline/extract: C:/Users/Trilex/AppData/Local/Darkness2VR/runtime/
  polish-side-completion (baseline, extract/H.Misc_en i B.Misc_en).
- Obecny targeted receipt: C:/Users/Trilex/AppData/Local/Darkness2VR/packages/
  villa-natural-dialogue-20261102T1707/deployment.json.
- Prywatny MAIN raport: .main-camera-shake/test-output/dialogue-timing/audit.json
  i package-final/{manifest.json,asr.json,encoded/*.decoded.wav}.
- Narzędzia MAIN: tools/polish_pack.py (Cache, audio_format, patch_xwma,
  category/translate), build-dialogue-repair.py, synthesize_timed_dialogue.py,
  qa-timed-dialogue.py. Przenieś potrzebne source commits do własnej gałęzi.

Sprawdź faktycznie zainstalowane H/B/F.Misc_en, nie tylko surowe WAV. Zapisz
preimage SHA archiwów, identyfikatory/pathy kwestii i źródło tekstu Languages.cl.
Porównaj native sample budgets/codec headers/dpds i dekodowany payload z raw
syntezą. Poprzedni audyt:1412 dopasowanych linii,706 raw WAV >1.15x native
budget. To sygnał backlogu, NIE dowód konkretnego tempa wszystkich payloadów.
Stary pakowacz stosował atempo oraz apad/atrim. Nowy MAIN guard odrzuca za długą
mowę. Znajdź również inne aktywne skrypty mogące nadal automatycznie przyspieszać.

Dla każdej podejrzanej kwestii podaj: ID/scena/postać, EN, aktualny PL,
native budżet, raw czas mowy i pauz, zainstalowany czas/realne przekształcenia,
początek/koniec mowy, cicha końcówka, ASR oczekiwane/uzyskane, confidence i
powód flagi. Nie utożsamiaj długości pliku (padding) z naturalnym tempem.
Rozróżnij potwierdzone cięcie od podejrzenia. ASR nie jest ludzkim odsłuchem,
zwłaszcza dla jedno-/dwuwyrazowych nagrań; zachowaj krótkie próbki do odsłuchu.
Sprawdź ubytki znaczenia, błędny rodzaj, imiona, halucynacje i urwane zdania.

Wyjście: per-task handoff + raport JSON/CSV prywatnie + krótki indeks w Git,
lista priorytetów i propozycje krótszych tekstów. Jeżeli robisz próbny fix,
tylko lokalna paczka kilku potwierdzonych linii: naturalna synteza z tym samym
donorem głosu, bez speedup, bez cięcia aktywnej mowy, z cichym końcem, natywny
codec roundtrip i dopasowane napisy. Zachowaj resztę tekstów/fonts/tekstur i
metadane fonemów. Nie przebudowuj całego voicepacka w ciemno.
Commituj tylko kod/teksty/redagowany indeks/handoff; nagrania i cache lokalne.
Raportuj PASS/FAIL/UNKNOWN/NOT RUN osobno. Instalację i odsłuch w goglach
wykonuje MAIN/użytkownik. Dostarcz MAIN ścieżkę paczki, hashe i rollback plan.
