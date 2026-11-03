# FRLG console B captures (index)

The PNGs stay local in `~/FRLGLab/captures/` (game screenshots are not committed). They come
from the CFR batches logged in [newgame-firered-console-B.md](newgame-firered-console-B.md),
section "2026-11-02 — Shiny Magikarp to Box 1", and are meant as sample frames for PC, box,
save and summary detectors. File names are `<batch>_<shot>.png`; the descriptions come from the
batch log (what the batch had just done), not from a frame-by-frame review.

## `~/FRLGLab/captures/pc-deposit-20261102/` (Route 4 Pokemon Center PC, deposit, save)

| Files | Batch step | Shows |
|---|---|---|
| `pc5_n.png`, `pc5_nw.png`, `pc5_w.png`, `pc5_t.png`, `pc5_boot.png`, `pc5_someone.png`, `pc5_menu.png`, `pc5_storage.png` | pc5: A at the PC | the PC booting, "SOMEONE'S PC", and the storage menu opening (`n`/`nw`/`w`/`t`: intermediate frames of the walk-up and text) |
| `pc6_n.png`, `pc6_t.png`, `pc6_e7.png`, `pc6_lamp.png`, `pc6_opts.png`, `pc6_dep.png`, `pc6_box.png` | pc6: DOWN, A (DEPOSIT) | the storage options (WITHDRAW / DEPOSIT / ...), DEPOSIT chosen, the party/box view |
| `pc7_s.png`, `pc7_t.png`, `pc7_e1.png`, `pc7_karp.png`, `pc7_sub.png` | pc7: RIGHT, DOWN to slot 3 | the cursor on the shiny Magikarp and its submenu |
| `pc8_a.png` .. `pc8_d.png`, `pc8_which.png` | pc8: A, A (STORE) | STORE chosen, "which box" prompt |
| `pc9_o.png`, `pc9_o2.png`, `pc9_o3.png`, `pc9_stored.png` | pc9: A (BOX1 0/30) | box picker, then Magikarp stored in BOX1 |
| `pc10_b1.png` .. `pc10_b4.png` | pc10: B x4 | backing out of the PC to the overworld |
| `sv3_start.png`, `sv3_before.png`, `sv3_chk.png`, `sv3_after.png` | sv3: START, SAVE | START menu with the cursor on SAVE, the save card, after saving |
| `sv4_q1.png` .. `sv4_q3.png`, `sv4_before.png`, `sv4_chk.png`, `sv4_after.png` | sv4: overwrite YES | "already a saved file ... overwrite?" prompt, the save in progress, back to the overworld |
| `pr1_sel.png`, `pr1_sub.png` | pr1: A too early | a blind A before the menu was drawn went to the text box; WITHDRAW opened instead |
| `pr2_back.png` | pr2: B | "Continue BOX operations?" |
| `pr3_menu.png`, `pr4_sub.png` | pr3-4: DEPOSIT, slot 2 | the storage menu and the Rattata's submenu |
| `pr5_which.png`, `pr5_stored.png` | pr5: STORE, BOX1 | box picker, Rattata stored |
| `pr6_start.png`, `pr6_ow.png` | pr6: B x4, START | the START menu and the overworld |
| `pr7_q2.png`, `pr7_q3.png` | pr7: save | the overwrite prompt and the saved state (1:22, party = Squirtle only) |

## `~/FRLGLab/captures/summary-20261102/` (party summary)

| Files | Batch step | Shows |
|---|---|---|
| `sm3_party.png` | sm3: START, POKéMON | the party screen |
| `sm3_sub.png` | sm3: A | Squirtle's submenu (SUMMARY) |
| `sm3_s1.png`, `sm3_s2.png`, `sm3_s3.png`, `sm3_s3a.png` | sm3: SUMMARY, RIGHT x2, A | summary pages 1-3; page 3 is the move list (WATER GUN 25/25, TAIL WHIP 30/30, BUBBLE 30/30, WITHDRAW 40/40) |
