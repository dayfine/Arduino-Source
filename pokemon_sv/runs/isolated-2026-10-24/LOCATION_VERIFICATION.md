# Charcadet location mismatch confirmed — 2026-10-24

The intended location did come from Austin John Plays:
https://www.youtube.com/watch?v=wP2rClXrn3A&t=519s

The locally cached original transcript at 521–525 seconds says Charcadet and
West Province Area One Central. At 525 seconds the video shows the center on
the southwest/coastal side of West Province Area One; at 540 seconds Charcadet
are visible along the grassy path, with the center nearby on the minimap.

The ACTUAL pilot flight confirmation at 13:59:23 reads `西第１區：北`, meaning
West Province Area One North. This is direct evidence of the wrong fly anchor,
not merely a mixed spawn pool near the intended Central anchor. The prototype's
unverified relative seek selected a nearby center and accepted generic flight
success without checking its name. Numel sightings and wall obstruction belong
to that North-center run; they do not invalidate Austin John's Central example.

Evidence:
- [Actual North confirmation](evidence/wrong-anchor-north.png)
- [Austin John's Central map at 525 seconds](evidence/austin-john-central-map-525s.jpg)
- [Charcadet path at 540 seconds](evidence/austin-john-charcadet-path-540s.jpg)
- [Actual Numel at wrong anchor](evidence/numel-at-wrong-anchor.png)
- [Confirmed previous Fire Encounter Lv1](evidence/fire-encounter-lv1-confirmed.png)

## Next field iteration

1. Fix flight selection and explicitly verify West Province Area One Central
   BEFORE confirming the flight. Reject North rather than accepting the nearest
   generic PokeCenter. Do not guess a new relative offset and call it verified.
2. Match the video path/terrain from Central, then record a reproducible approach
   and safe movement bounds that avoid the center/wall and other spawn pools.
3. Make ordinary Fire Lv2 recipe, verify actual Fire Encounter Lv2 on the Meal
   Powers screen and retain that screenshot. Current code selects 6 Rice +
   2 Curry Powder + 2 Wasabi, but that recipe has NOT been tested in this game.
   Shared placement supports six rice servings; confirm setup stage time budget
   accommodates selection/assembly before sustained testing.
4. Capture landing and 3–5 minutes of clearing, distinguishing sound counts from
   species-confirmed kills. Preserve any unresolved refusal before resets.
5. Save and restore AZP with actual traversal/first kill verification.

No new experiment began after reaching the recovery-reserve threshold. User's
quota-handback instruction prompted opening release AZP; user asked why, and
this reason was explained. Latest capture showed Switch Home with Violet still
running, followed by colour bars. No farmer Start was issued, no flight to a
new destination was issued, and no restoration is claimed complete.

## Permission switching check

The live app requested microphone access on reopening. Checked exact identities:
release PokemonAutomation.SerialPrograms, Apple Developer ID signature;
lab com.dayfine.pokemonautomation.lab, persistent local certificate signature.
Old ad-hoc lab builds shared the release bundle ID; stale grants are a plausible
cause of this first live reapproval, not a confirmed TCC database diagnosis.

After approval, release -> signed lab -> release produced no further observed
UserNotificationCenter permission dialog. This is a bounded switching check,
not proof across future changes. Read-only TCC database access remains denied;
no database write/reset or added privacy access was attempted.

Current release PID 1139 open/stopped, selected AZP/zerogate, sole device owner.
Prototype closed. Capture lacks a game picture, so resume the game/restore HDMI,
verify saved Zero Gate, then start AZP. Do not issue blind game commands or claim
farming resumed while the capture is unavailable.
