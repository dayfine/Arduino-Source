# M7 preparation, 2026-10-21 PDT

Status: preparation COMPLETE; live trial started at 18:49:05 PDT.
See ../2026-10-21-m7-live/RESULT.md. No live acceptance path is marked verified.

- Feature: 57f330fc9, reuse complete reroll observations before further game actions.
- Integration base: 8aa1821b4; prerequisites listed in lab/STACK.md.
- Full arm64 app: PASS (build-prep/build.log and rebuild.log).
- Registered native C++ tests: 55 passed, 0 failed (build-prep/native-tests.log).
- M7 adds four transaction observation cases and two round-loop scenarios.
- Binary: build-prep/app/SerialPrograms.app/Contents/MacOS/SerialPrograms.
- SHA-256: 9bc7b7b0da2396993615384a815fdb160e79ca53cbfc1ae893a73edbf1626603.
- Profile: bbq-lab, isolated stats/settings and copied resource cache.
- Active trial configuration prepared: 1000 quest capacity, one retry,
  Out of Retries=Reroll, save interval 10, zero eggs/Keep Going and Hatch Skip.
  Existing regular exclusions retained; notifications disabled.
- Flying Trial transition: automatic stopping missed two safe boundaries; user
  stopped manually and returned to Central Plaza. Old app closed; lab app opened.

Native evidence includes reuse through planning and the first dispatch, fresh
reads for subsequent quests, conservative fallback for three/missing/unknown
slots and concurrent board changes, and existing retry/save/reroll/Tera tests.

Integration correction 8aa1821b4 changes the old save test's zero-completion
round to use the Reroll policy: M4 intentionally prevents reattempting the same
failed quests under Skip. No production retry/save behavior was changed for it.
Hand this test adaptation to Claude with future integration findings; do not
push it into an existing PR branch.

Preflight completed: full-health Lv.100 Primarina lead and 13,690 BP captured.
Camera/microphone prompts occurred on new app launch; capture now works.
