# pokeguy's LeafGreen 1:59:34: staged visual route notes

> Sampled visual review of pokeguy's LeafGreen any% glitchless world-record run, kept as a route reference
> for [the FRLG AutoStory checkpoint list](AutoStory-FRLG-FutureEvents.md); copied from `notes/frlg-wr-route`.

Source: [full video](https://www.youtube.com/watch?v=CbfjWuRWH10),
[verified run](https://www.speedrun.com/pkmnfrlg/runs/z5kkjogz).
Video title: *Pokemon LeafGreen Any% Glitchless Speedrun in 1:59:34
[Current World Record]*. The leaderboard category is English Any% Glitchless,
console (GBI). Do not label this run manipless.

Video duration from public metadata: **02:03:57** (7,437 seconds).
All timestamps below are **video playback positions**, not the run timer.
The on-screen timer is a separate clock; the recording opens with a previous
attempt's time before the new timer begins.

## Review method and scope

Work in five-minute video chunks. The first pass inspects the downloaded
720p clips (00–10) and the full 480p video (10 onward) at one-second intervals, plus quarter-second inspection around
short menus (naming, Trainer Card, move swap, capture bag/nickname); up to
12 frames/second for the brief Ether move menu. Exact
boundary frames are checked when a battle crosses chunks. Notes summarize actions
and outcomes; they are not an exact controller input log. Mark missed text,
unreadable choices and inferred intent explicitly. Audio commentary is not
reviewed in this pass. Storyboard thumbnails are orientation only, not proof
of every intervening action.

Record route choices, interactions, item pickups, menu settings, naming,
party/move changes, battle opponents and actions, healing, saves/resets, and
any uncertainty that affects reproducing the route. A passed time boundary
does not mean that chunk has been reviewed.

The original task briefly selected Werster's run, then the user corrected
the selection to pokeguy. Only pokeguy's video belongs in these notes.
Research notes for the FRLG AutoStory route; no code depends on them.

## Repeatable review process

1. Verify the runner, category and video against the linked run page. Keep
   playback timestamps separate from the on-screen speedrun timer. Download
   the source once; verify its duration and use the same file for later blocks.
2. Take the next **five minutes of video**, starting at the saved resume point.
   Extract one frame per second and arrange consecutive frames in timestamped
   contact sheets: 30 frames per sheet, ten sheets per five-minute block.
   Inspect every sheet in order. This is a sampled visual review, not continuous
   playback or an audio transcription; short actions can fall between samples.
3. Follow the route and battle sequence across those sheets. Record opponents,
   moves, visible HP/PP, items, prices, rewards and story interactions. Reinspect
   brief menus or unclear transitions with denser frames (normally 4 fps;
   12 fps was needed for one Ether menu). Consult individual frames as needed.
   Do not infer unseen actions from an expected walkthrough route.
4. Write a timestamped action table for that block. Distinguish directly read
   values from counts derived from earlier observations. Label unreadable text,
   uncertain intent and unresolved choices explicitly; do not treat sampling
   gaps as evidence that an action never happened.
5. Reconcile a boundary state: location, pending interaction/battle, party,
   level, HP, moves and relevant inventory. Carry unfinished actions into the
   next block. Check the next boundary frame if it is needed to resolve a fade.
   Keep proposed AutoStory behavior separate from observations of this run.
6. Update the coverage table and resume point **only after visual inspection
   and notes are complete**. Keep downloaded media and extracted frames
   outside git.

Current local tooling (temporary cache, not a permanent dependency):

```sh
# Absolute start in seconds: 4500 = video 1:15:00.
/tmp/codex-frlg-video-review-20261101/modern/bin/python \
  /tmp/codex-frlg-video-review-20261101/review_frames.py 4500
# Optional focused pass; give each extraction a distinct output name.
/tmp/codex-frlg-video-review-20261101/modern/bin/python \
  /tmp/codex-frlg-video-review-20261101/review_frames.py 4500 \
  --duration 8 --fps 4 --name detail-7515-example
```

The helper uses FFmpeg to seek into `pokeguy-full-480.mp4`, crop the gameplay
area (`crop=666:466:188:0`), and scale it to 400×280 per frame. Pillow labels
and combines frames into five-column, six-row sheets. Inspect them with an
image viewer; generating the sheets alone does **not** review the block.
Crop/scale coordinates belong to this recording's layout and must be checked
again for another video. Source metadata and media links above allow recovery
if the `/tmp` cache disappears; the helper itself is currently temporary.

## Coverage and resume point

- Completed chunks: **00:00–2:03:57**, the whole video, reviewed in separate five-minute visual passes.
- The run ends at the Hall of Fame (video 1:59:41, in-game clock 1:54); credits follow.
- Local media workspace: `/tmp/codex-frlg-video-review-20261101/`.
  Metadata: `pokeguy-metadata.json`; first clip: `pokeguy-0000-0500.mp4`.
  Second clip: `pokeguy-0455-1000.mp4` (video 04:55–10:00, including overlap).
  Its local time 00:05 corresponds to video 05:00; checked against the
  on-screen run timer (04:53). Clip duration verified as 305 seconds.
  From 10:00 onward: `pokeguy-full-480.mp4` (full 02:03:57 video), with
  timestamped one-second contact sheets in `1000/`, `1500/`, etc.
  `review_frames.py` regenerates a block from its absolute start in seconds.
  This is a temporary cache and may need regeneration.
- Keep media out of git.

| Chunk (video time) | Status |
|---|---|
| 00:00:00–00:05:00 | Reviewed; details below |
| 00:05:00–00:10:00 | Reviewed; details below |
| 00:10:00–00:15:00 | Reviewed; details below |
| 00:15:00–00:20:00 | Reviewed; details below |
| 00:20:00–00:25:00 | Reviewed; details below |
| 00:25:00–00:30:00 | Reviewed; details below |
| 00:30:00–00:35:00 | Reviewed; details below |
| 00:35:00–00:40:00 | Reviewed; details below |
| 00:40:00–00:45:00 | Reviewed; details below |
| 00:45:00–00:50:00 | Reviewed; details below |
| 00:50:00–00:55:00 | Reviewed; details below |
| 00:55:00–01:00:00 | Reviewed; details below |
| 01:00:00–01:05:00 | Reviewed; details below |
| 01:05:00–01:10:00 | Reviewed; details below |
| 01:10:00–01:15:00 | Reviewed; details below |
| 01:15:00–01:20:00 | Reviewed; details below |
| 01:20:00–01:25:00 | Reviewed; details below |
| 01:25:00–01:30:00 | Reviewed; details below |
| 01:30:00–01:35:00 | Reviewed; details below |
| 01:35:00–01:40:00 | Reviewed; details below |
| 01:40:00–01:45:00 | Reviewed; details below |
| 01:45:00–01:50:00 | Reviewed; details below |
| 01:50:00–01:55:00 | Reviewed; details below |
| 01:55:00–02:00:00 | Reviewed; details below |
| 02:00:00–02:03:57 | Reviewed (credits; sampled) |

## 00:00–05:00 — opening, Squirtle and first Route 1 traversal

Timestamps are approximate, normally within one second. The linked position
is a review entry point, not an exact button-edge measurement.

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [00:00–00:07](https://www.youtube.com/watch?v=CbfjWuRWH10&t=0) | Boot/copyright screen, LeafGreen title, then select NEW GAME. | A CONTINUE entry for player A, time 0:01, zero badges is already present. This is not evidence of a blank save cartridge. The opening on-screen timer initially contains an earlier attempt's time. |
| [00:08–00:15](https://www.youtube.com/watch?v=CbfjWuRWH10&t=8) | Advance control explanations and introductory help pages. | No options configuration screen observed here. |
| [00:16–00:30](https://www.youtube.com/watch?v=CbfjWuRWH10&t=16) | Advance Oak's introduction; use the male player character. | New run timer starts at about video 00:07; around video 00:49.9 it reads 00:42.90. Approximate offset: video time minus 7 seconds. |
| [00:31–00:34](https://www.youtube.com/watch?v=CbfjWuRWH10&t=31) | Enter the player name **A**. | Name confirmed again on the Trainer Card. |
| [00:36–00:41](https://www.youtube.com/watch?v=CbfjWuRWH10&t=36) | Choose rival name **RED** from the default-name list. | Quarter-second frames show the default list and the subsequent RED confirmation. This is LeafGreen; our FireRed table currently uses GREEN. |
| [00:42–00:48](https://www.youtube.com/watch?v=CbfjWuRWH10&t=42) | Finish intro and appear in upstairs bedroom; open START menu. | No walk to the bedroom PC before opening the menu. |
| [00:48–00:51](https://www.youtube.com/watch?v=CbfjWuRWH10&t=48) | Open player **A** / Trainer Card, inspect it briefly, close it. | Visible Trainer ID **39569**, money **3000**, game time **0:00**, zero badges. Purpose of the check is not established by silent footage alone. |
| [00:51–01:05](https://www.youtube.com/watch?v=CbfjWuRWH10&t=51) | Walk to stairs, descend, leave house; go west of the house, north around it and east toward Pallet's northern grass exit. | No bedroom PC Potion collection or conversation with mother observed. No exact tile-by-tile coordinates measured. |
| [01:06–01:20](https://www.youtube.com/watch?v=CbfjWuRWH10&t=66) | Trigger Oak's warning at the north exit; advance dialog; follow scripted escort into lab. | Includes automatic movement; do not count it as manually walked route legs. |
| [01:21–01:34](https://www.youtube.com/watch?v=CbfjWuRWH10&t=81) | Walk up inside lab; advance Oak/rival starter explanation. | Rival is already present. |
| [01:35–01:44](https://www.youtube.com/watch?v=CbfjWuRWH10&t=95) | Approach the middle starter ball, interact and select **Squirtle**. | Reception text explicitly confirms Squirtle. Map coordinates are not measured from this video. |
| [01:44–01:47](https://www.youtube.com/watch?v=CbfjWuRWH10&t=104) | Accept nickname entry and name Squirtle **A**. | This differs from the MVP's NO-nickname choice. |
| [01:47–01:56](https://www.youtube.com/watch?v=CbfjWuRWH10&t=107) | Rival takes Bulbasaur; walk away from table into rival battle trigger. | Red's Bulbasaur reception text visible. |
| [01:57–02:15](https://www.youtube.com/watch?v=CbfjWuRWH10&t=117) | Rival battle intro and Oak's battle tutorial. | Opponent: **Bulbasaur Lv.5**. Player: **Squirtle A Lv.5, 20/20 HP**. |
| [02:16–02:40](https://www.youtube.com/watch?v=CbfjWuRWH10&t=136) | Repeatedly choose **Tackle**, winning the tutorial fight. | Five player Tackle actions in the sampled sequence. Bulbasaur's first attack misses; later Tackles reduce A to **11/20 HP** before the level gain. No item use or Tail Whip by the player observed. |
| [02:41–02:49](https://www.youtube.com/watch?v=CbfjWuRWH10&t=161) | Receive **67 EXP**, grow to **Lv.6**, defeat RED and receive **80** prize money. | Level-up display shows **13/22 HP**. Do not confuse this with later full HP. |
| [02:49–03:00](https://www.youtube.com/watch?v=CbfjWuRWH10&t=169) | Finish Oak's end-of-battle explanation and rival departure; leave lab. | No manual healing action shown. |
| [03:01–03:10](https://www.youtube.com/watch?v=CbfjWuRWH10&t=181) | Walk north from lab, leave Pallet, enter Route 1 grass. | First wild encounter transition begins near 03:10. |
| [03:10–03:18](https://www.youtube.com/watch?v=CbfjWuRWH10&t=190) | Enter wild battle against **Rattata Lv.3**. | A is **Lv.6, 22/22 HP**, Tackle **35/35 PP** when the move menu appears. Both HP and Tackle PP have been restored since the lab fight; no nurse or bag action intervened. |
| [03:18–03:44](https://www.youtube.com/watch?v=CbfjWuRWH10&t=198) | Fight Rattata with **Tackle**, rather than fleeing. | Four Tackle uses including one miss; Rattata repeatedly uses Tail Whip. Last hit is critical. Receive **24 EXP**; A remains **Lv.6, 22/22 HP**. Later visible Tackle **31/35** supports the four-use count. |
| [03:45–03:54](https://www.youtube.com/watch?v=CbfjWuRWH10&t=225) | Continue north through Route 1's path/grass sections. | No pickup or conversation observed in this travel interval. Another wild transition starts around 03:54. |
| [03:54–04:01](https://www.youtube.com/watch?v=CbfjWuRWH10&t=234) | Enter battle against **Pidgey Lv.2**. | A starts **Lv.6, 22/22 HP**. |
| [04:01–04:02](https://www.youtube.com/watch?v=CbfjWuRWH10&t=241) | Reorder the move list: exchange Tackle and Tail Whip. | Quarter-second frames show the switch: **Tail Whip becomes slot 1; Tackle becomes slot 2**. Tackle remains selected and is used. Exact controller button for move swapping is not visible. |
| [04:02–04:14](https://www.youtube.com/watch?v=CbfjWuRWH10&t=242) | Use **Tackle three times** to defeat Pidgey. | Pidgey's Tackles reduce A from **22/22 to 19/22, then 16/22 HP**. No healing or capture attempt observed. |
| [04:15–04:22](https://www.youtube.com/watch?v=CbfjWuRWH10&t=255) | Receive **15 EXP**, grow to **Lv.7**, learn **Bubble**. | Level-up HP becomes **19/25**. Bubble is added without a four-move replacement prompt. Its final slot has not yet been read from a later move menu. |
| [04:23–04:39](https://www.youtube.com/watch?v=CbfjWuRWH10&t=263) | Continue north; enter Viridian around 04:29; pass the Center and route east/north to the Mart. | Does not enter the Pokemon Center. |
| [04:40–04:47](https://www.youtube.com/watch?v=CbfjWuRWH10&t=280) | Enter Mart; automatic clerk approach/dialog; receive **Oak's Parcel**. | Explicit item receipt visible around 04:45–04:47. No shopping menu or purchase shown. |
| [04:48–05:00](https://www.youtube.com/watch?v=CbfjWuRWH10&t=288) | Leave Mart; walk west/south around the Center toward southern Viridian / Route 1. | At the chunk boundary the return journey has started. Parcel delivery and Pokedex are **not** covered yet. |

### State carried into the next chunk

- Player: **A**; rival: **RED**; game: **LeafGreen**.
- Traced party: Squirtle nicknamed **A**, last visible **Lv.7, 19/25 HP**.
  No additional party member obtained in the reviewed interval.
- Known moves: **Tackle, Tail Whip, Bubble**. Last verified ordering before
  Bubble was learned: Tail Whip slot 1, Tackle slot 2. Bubble's slot and exact
  current PP require the next visible move menu; do not invent them.
- Oak's Parcel received and not yet delivered. No Pokedex or badges yet.
- No player item use, purchase, capture, manual save or nurse visit observed.
  Bag quantities are not known from this footage; zero item use is not proof
  of an empty bag. Money after prize is derivable as 3080, but not re-read.
- Last observed location: southern Viridian, returning toward Route 1.
- Resume around **04:55** for context, then annotate **05:00–10:00**.

### Explicit uncertainties

- Text speed, battle scene/style and button-mode settings were not inspected
  on an options screen. Do not mark an observed SET_OPTIONS event.
- The overlay shows nature/IV information, but the inspected gameplay does
  not show a corresponding Summary-screen verification or explain how those
  overlay values were obtained. Treat it separately from observed game state.
- Trainer ID check and precise movement may be RNG-related; that purpose is
  an inference and has not been established here. Glitchless does not imply
  manipless.
- Frame sampling can miss short input/menu states. These are action-level
  notes, not a claim that every button press or tile was recovered.

## 05:00–10:00 — Pokedex, Rattata capture and Viridian Forest

Continues the return from Viridian with Oak's Parcel. Same timestamp convention:
video positions, approximate to a second unless a boundary frame is specified.
The one-second visual pass covers the full chunk, with quarter-second passes
around the Rattata bag and nickname sequences. No audio review.

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [05:00–05:23](https://www.youtube.com/watch?v=CbfjWuRWH10&t=300) | Return south along Route 1, using the ledge route; enter Pallet and walk to Oak's lab. | No wild encounter, pickup or healing in this return interval. |
| [05:24–05:32](https://www.youtube.com/watch?v=CbfjWuRWH10&t=324) | Enter lab; approach Oak; talk; deliver **Oak's Parcel**. | Explicit parcel-delivery text appears around 05:30. |
| [05:33–05:49](https://www.youtube.com/watch?v=CbfjWuRWH10&t=333) | Advance scene: RED arrives, Oak collects the devices and gives the **Pokedex**. | Receipt text visible around 05:47–05:48. No new battle with RED. |
| [05:50–06:08](https://www.youtube.com/watch?v=CbfjWuRWH10&t=350) | Receive **five Poke Balls**; advance catching instructions, Oak's request and RED's departure. | Five-ball receipt around 05:52–05:53. RED mentions borrowing a Town Map; the player does not thereby receive one. |
| [06:09–06:30](https://www.youtube.com/watch?v=CbfjWuRWH10&t=369) | Leave lab and head north through Pallet and Route 1 again. | No visit to rival's sister, player house, Mart or Center in this interval. |
| [06:31–06:39](https://www.youtube.com/watch?v=CbfjWuRWH10&t=391) | Wild **Rattata Lv.3** encounter; send A and use **Bubble once**. | A begins **Lv.7, 19/25 HP**. Rattata survives the hit in green HP. This is a second Rattata encounter, distinct from the one defeated in the first chunk. |
| [06:40–06:43](https://www.youtube.com/watch?v=CbfjWuRWH10&t=400) | Rattata uses Tackle; open BAG. | A falls to **16/25 HP**. A's ITEMS pocket is visibly empty in the closer menu pass; this is the player's bag, unlike the old man's later demonstration. |
| [06:44–06:57](https://www.youtube.com/watch?v=CbfjWuRWH10&t=404) | Select and throw **one Poke Ball**; capture Rattata. | Explicit successful-catch message; no failed ball or second throw observed. No defeat/EXP event. |
| [06:58–07:03](https://www.youtube.com/watch?v=CbfjWuRWH10&t=418) | Advance Rattata Pokedex entry and dismiss nickname prompt; return to overworld. | No naming keyboard appears in the quarter-second pass. Treat species-name retention as inferred until a party-name view confirms it; exact cancel button is not visible. |
| [07:03–07:15](https://www.youtube.com/watch?v=CbfjWuRWH10&t=423) | Continue north to Viridian; pass west of the Center and approach the old-man trigger. | No shopping or nurse visit. Rattata has joined the traced party; no switch away from Squirtle observed. |
| [07:16–07:22](https://www.youtube.com/watch?v=CbfjWuRWH10&t=436) | Advance old man's coffee/Pokedex/catching explanation. | Leads directly into his demonstration. |
| [07:23–07:56](https://www.youtube.com/watch?v=CbfjWuRWH10&t=443) | Watch the old man's scripted **Weedle Lv.5** capture demonstration. | His demo bag shows Potion x1 and Poke Ball x1; these are **not** evidence of the player's inventory. Weedle is not a new member of the player's party. |
| [07:57–08:03](https://www.youtube.com/watch?v=CbfjWuRWH10&t=477) | Finish tutorial dialog; receive **Teachy TV**; leave northward. | Explicit receipt and Key Items placement text. |
| [08:04–08:24](https://www.youtube.com/watch?v=CbfjWuRWH10&t=484) | Walk Route 2's western path and grass; enter and cross the gatehouse; enter Viridian Forest. | No trainer battle or item collection in this approach. Forest entry artwork appears around 08:22–08:24. |
| [08:25–08:43](https://www.youtube.com/watch?v=CbfjWuRWH10&t=505) | Move east/north through the forest's lower/right-side paths, passing trainers. | No battle with these passed NPCs observed. Exact tile coordinates are not measured. |
| [08:44–08:46](https://www.youtube.com/watch?v=CbfjWuRWH10&t=524) | Pick up **one Antidote** from an overworld item ball. | Explicit item-found text. Acquisition does not imply it has been used. |
| [08:47–08:59](https://www.youtube.com/watch?v=CbfjWuRWH10&t=527) | Continue through grass; encounter **Caterpie Lv.4** and flee successfully. | A remains **Lv.7, 16/25 HP**; no attack, capture, EXP gain or escape failure observed. |
| [09:00–09:15](https://www.youtube.com/watch?v=CbfjWuRWH10&t=540) | Continue along forest paths/grass; encounter **Kakuna Lv.5** and flee. | Explicit safe-escape text; A still **16/25 HP**. |
| [09:16–09:34](https://www.youtube.com/watch?v=CbfjWuRWH10&t=556) | Continue toward the upper forest path; encounter **Caterpie Lv.5** and flee. | Third forest wild battle in this chunk; all three are fled, not fought for EXP. |
| [09:35–09:51](https://www.youtube.com/watch?v=CbfjWuRWH10&t=575) | Walk into trainer sight line; trainer approaches; begin **Bug Catcher Sammy** battle. | Name and **Weedle Lv.9** shown. A starts **Lv.7, 16/25 HP**. |
| [09:52–09:57](https://www.youtube.com/watch?v=CbfjWuRWH10&t=592) | Select Bubble; Weedle acts first with Poison Sting; A uses Bubble. | Menu confirms **Tail Whip slot 1, Tackle slot 2, Bubble slot 3**, with Bubble **29/30 PP** before use. Poison Sting reduces A to **12/25 HP**; Bubble damages but does not defeat Weedle. |
| [09:58–10:00](https://www.youtube.com/watch?v=CbfjWuRWH10&t=598) | Weedle uses Poison Sting again; A begins a second Bubble. | At exact video **09:59.9**, A is **9/25 HP**, Lv.7, and text says A used Bubble. Its second hit's outcome is not yet visible. No trainer win, EXP, level gain or reward recorded in this chunk. |

### State carried into 10:00–15:00

- **Active trainer battle: Bug Catcher Sammy, Weedle Lv.9.** Continue it;
  do not start another battle entry or assume the win shown in later thumbnails
  happened before this boundary.
- Active Pokemon: Squirtle **A, Lv.7, 9/25 HP** at video **09:59.9**.
  Second Bubble has been announced; its damage/KO outcome remains pending.
  The level indicator is still visible rather than a poison status label at
  this frame; check for later status text before inferring the final effect
  of Poison Sting.
- Move order now directly verified: **Tail Whip, Tackle, Bubble, empty**.
  Bubble was **29/30 PP** at the first Sammy selection. Two uses since that
  read would imply 27/30, but a later menu must confirm the current value.
- Traced party: Squirtle A plus the captured **Rattata Lv.3**. No other catch.
  Rattata's precise HP, move set, party slot and retained name have not been
  read on a party/Summary screen.
- Pokedex obtained; Parcel delivered; Teachy TV obtained; no badge yet.
- One Antidote picked up and no use observed. Five Poke Balls received and
  one spent: **four remaining is bookkeeping inference**, not a post-capture
  inventory read. No shop, nurse, manual save or party switch observed.
- Resume at **09:55** for battle context; next annotation range **10:00–15:00**.

### Chunk-specific uncertainties and boundaries

- Capture intent is not stated. A future Cut-user role for Rattata is plausible
  but not established by this chunk; do not label it an observed HM assignment.
- The fast ball-selection screen does not provide a reliable post-capture
  quantity read. The tutorial's fake bag must not be used to fill that gap.
- The default nickname prompt is dismissed without a naming screen; no claim
  is made about the physical button used or a briefly unseen cursor change.
- Only the Antidote pickup above is observed. Walking past other item balls
  or a sign is not an item or dialog event.

## 10:00–15:00 — Sammy, Pewter Gym, supplies and Running Shoes

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [10:00–10:22](https://www.youtube.com/watch?v=CbfjWuRWH10&t=600) | Finish Sammy's Weedle with Bubble, four uses across the whole fight. Win 108. | The second Bubble is in progress at 10:00. Two more Poison Stings reduce HP 9→6→3; the fourth Bubble finishes it. A grows to Lv.8, 5/27 HP. No poison status or healing observed. |
| 10:24–10:54 | Leave Viridian Forest through the north gate, cross Route 2 into Pewter, enter the Gym. | Pass the Pokemon Center and Mart without entering. |
| 10:56–11:22 | Trigger Camper Liam's sightline. One Bubble defeats Geodude Lv.10. | A begins Lv.8, 5/27 HP, Bubble 25/30 PP; gains 183 EXP and reaches Lv.9, 7/29 HP. |
| 11:24–11:53 | Against Sandshrew Lv.11: Bubble, Bubble, Tackle (miss), Tackle (KO). | Sandshrew uses Sand-Attack, Defense Curl, then Sand-Attack. First Bubble leaves yellow HP, second leaves red. |
| 11:55–12:08 | A reaches Lv.10 and learns Withdraw into the empty fourth slot, then reaches Lv.11. Defeat Liam; win 220. | HP increases to 9/31 then 11/33. Both level increases occur after Sandshrew; do not drop the second one. |
| 12:10–12:35 | Approach Brock; one Bubble defeats Geodude Lv.12. | No intervening healing. A begins Brock at Lv.11, 11/33 HP. |
| 12:40–12:53 | Onix Lv.14 uses Rock Tomb; A answers with one Bubble, defeating it. | Rock Tomb reduces A from 11 to **1/33 HP** and lowers Speed. No Potion used. |
| 12:55–13:12 | A reaches Lv.12 (3/35 HP), then Lv.13 (6/38); replace **Tail Whip with Water Gun**. | Quarter-second menu inspection confirms deletion of slot 1. Before deletion: Tail Whip 30/30, Tackle 26/35, Bubble 20/30, Withdraw 40/40. Water Gun starts 25/25. |
| 13:14–13:37 | Defeat Brock; receive Boulder Badge, 1,400 prize money and TM39. | TM39 receipt visible at 13:30–13:31; continue reward dialogue before leaving. |
| 13:38–13:57 | Leave Gym, circle its north side, enter Pewter Mart and speak to clerk. | No Center stop or healing. |
| [13:58–14:08](https://www.youtube.com/watch?v=CbfjWuRWH10&t=838) | Buy **9 Potions, 3 Awakenings and 3 Repels**. | Quarter-second inspection confirms quantities. Money 4,808→2,108→1,358→308. Potion bag count was 0. The initial fast quantity scroll passes 11 before settling on 9; do not read that transient value as the purchase. |
| 14:09–14:32 | Exit Mart, go east, complete aide interruption and mother's letter. Receive Running Shoes. | Receipt at 14:21–14:22; letter continues after the aide walks away. |
| 14:33–14:48 | Enter Route 3; trigger Bug Catcher Colton. He sends Caterpie Lv.10. | A remains Lv.13, 6/38 HP. No post-shopping heal used. |
| 14:49–15:00 | Water Gun defeats Caterpie; Colton sends Weedle Lv.10. A begins another Water Gun. | Battle continues into next chunk. Do not count Weedle as defeated yet. |

**Boundary state at 15:00:** Route 3, Colton battle, Water Gun against his
second Pokemon (Weedle Lv.10) underway. A is Squirtle Lv.13, 6/38 HP;
moves include Water Gun, Tackle, Bubble and Withdraw (slot order not yet audited).
Rattata remains the second party
member; no switch observed. Boulder Badge, TM39 and Running Shoes obtained.
Purchased inventory is 9 Potions, 3 Awakenings, 3 Repels, all unused here;
previous Antidote and four remaining Poke Balls are carried forward by observation
history, not a fresh bag audit. Money 308. No save/reset observed.

**Automation relevance:** Brock's survival at 1 HP and the later 6 HP Route 3
entry need an explicit risk/healing policy. Move replacement, two-level EXP
jumps, reward dialogue completion and multi-item purchases are distinct actions;
entering the next map alone does not verify their results.


## 15:00–20:00 — Route 3 training, Pidgey and Mt. Moon

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [15:00–15:24](https://www.youtube.com/watch?v=CbfjWuRWH10&t=900) | Finish Colton: Water Gun KOs Weedle Lv.10; one Water Gun KOs his second Caterpie Lv.10 (critical hit). Win 120. | A reaches Lv.14, 8/40 HP. All three Colton opponents fall to one Water Gun each across the chunk boundary. |
| 15:25–16:24 | Move east along upper Route 3 and battle Bug Catcher Greg. | Weedle, Caterpie, Kakuna and Metapod, all Lv.9: one Water Gun each, in that order. A reaches Lv.15, 10/42 HP after Metapod. Prize 108. |
| 16:25–16:59 | Battle Bug Catcher James farther east. | Caterpie Lv.11 and Metapod Lv.11 each fall to one Water Gun. Prize 132; A remains 10/42 HP. No heal between the three Bug Catchers. |
| [17:00–17:35](https://www.youtube.com/watch?v=CbfjWuRWH10&t=1020) | Enter grass below the eastern bend; encounter and capture Pidgey Lv.7. | One **Bubble** leaves yellow HP; Pidgey uses Sand-Attack. One Poke Ball succeeds. Bag shows four balls before use, so three remain by subtraction. Nickname prompt dismissed without opening keyboard; species name retained provisionally. No capture EXP. |
| 17:36–17:49 | Leave grass eastward, follow bend north to Route 4, pass Center, enter Mt. Moon. | No Center interaction, Magikarp purchase or heal observed. |
| 17:50–17:55 | Open bag and use first Repel. | Bag shows Antidote 1, Potions 9, Awakenings 3, Repels 3 before use. |
| 17:56–18:05 | Navigate east/south around cave walls and collect **Rare Candy** from item ball. | Receipt text at 18:04. Leave other visible item balls untouched. No tile coordinates assigned from video. |
| 18:06–18:15 | Double back west, Repel expires; open bag and use second Repel. | Repels 2 and Rare Candy 1 visible before use. |
| 18:16–18:30 | Approach Youngster Josh near a ladder on the western side; start battle. | A still Lv.15, 10/42 HP. |
| 18:31–18:39 | First Rattata Lv.10 uses Quick Attack, reducing A 10→5/42; one Water Gun KOs it. | No Potion despite low HP. |
| 18:40–18:53 | Second Rattata Lv.10 falls to one Water Gun. | Water Gun 15/25 PP before this use. A reaches Lv.16, 7/44 HP. |
| 18:54–19:12 | Zubat Lv.10 takes two Water Guns, using Leech Life between them. Defeat Josh; win 160. | Leech Life reduces A 7→5/44. |
| 19:13–19:37 | Allow Squirtle to evolve into **Wartortle**. | Confirmation visible at 19:34; evolution fully completes before movement resumes. No cancellation. |
| 19:38–19:50 | Descend nearby ladder, traverse narrow intermediate floor, descend another ladder into larger lower floor. | Follow passages and steps; avoid nearby Rocket sightlines during this stretch. |
| 19:51–20:00 | Repel expires; use third Repel, then continue east around lower-floor walls. | Bag shows last Repel 1 before use, Potions 9, Awakenings 3, Antidote 1 and Rare Candy 1. No heal. |

**Boundary state at 20:00:** moving through Mt. Moon's lower floor, third Repel
active. A is Wartortle Lv.16; last pre-evolution HP reading was 5/44, and the
new maximum/current HP has not yet been read. Moves unchanged. Party now includes
Rattata Lv.3 and Pidgey Lv.7; their exact remaining HP is unverified. Three balls
remain by subtraction. Rare Candy 1 collected, no Repels left after this use;
Potions 9, Awakenings 3, Antidote 1 confirmed in bag. Money **828 derived** from
308 + 120 + 108 + 132 + 160; no fresh money display here. No save/reset observed.

**Automation relevance:** distinguish intentional capture grass from ordinary
travel; use a species/role goal rather than assuming the first encounter is Pidgey.
Repel expiry interrupts movement and requires a new bag action in this version.
Treat evolution as its own blocking scene and refresh party state afterward.


## 20:00–25:00 — fossils, move tutoring, Cerulean preparation and Misty

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [20:00–20:18](https://www.youtube.com/watch?v=CbfjWuRWH10&t=1200) | Follow lower-floor passage and trigger Rocket Grunt near fossils. | Wartortle Lv.16 now visibly **10/49 HP**; refreshes the pre-evolution reading. |
| 20:19–20:39 | One Water Gun each defeats Rattata Lv.13 and Zubat Lv.13. Win 416. | No damage or healing; still 10/49 HP. |
| 20:40–20:53 | Approach Super Nerd Miguel at fossil platform and begin battle. | Grimer Lv.12 first. |
| 20:54–21:09 | Two Water Guns defeat Grimer; its Pound reduces A 10→6/49 HP. | A grows to Lv.17, 8/51 HP. |
| 21:10–21:29 | Two Water Guns defeat Voltorb Lv.12. | Voltorb uses Tackle (8→6/51 HP), then Screech before the second Water Gun. |
| 21:30–21:45 | One Water Gun defeats Koffing Lv.12. Win 288. | A grows to Lv.18, 9/54 HP. |
| 21:46–21:56 | Choose **Helix Fossil**, then finish Miguel's fossil-taking scene. | Receipt visible at 21:49; stored in Key Items. Right-hand fossil selected. |
| 21:57–22:06 | Ascend two ladders and exit Mt. Moon to eastern Route 4; approach right-hand move tutor. | No further cave encounter, heal or pickup observed. |
| [22:07–22:25](https://www.youtube.com/watch?v=CbfjWuRWH10&t=1327) | Teach **Mega Kick** to A, replacing **Withdraw**. | Detailed menu inspection confirms Withdraw selected. Menu order is now Tackle / Water Gun / Bubble / Withdraw before replacement; exact earlier reorder input has not been isolated. Water Gun 5/25, Bubble 19/30, Tackle 26/35. Party shows Rattata Lv.3, **10/15 HP** and Pidgey Lv.7, **5/24 HP**. |
| 22:26–22:39 | Jump ledges eastward, stay above grass, enter Cerulean and enter northwest house. | Do not infer that ignored visible item balls were picked up. |
| 22:40–22:53 | Walk through house to backyard, collect **hidden Rare Candy** in flower patch, return through house. | Receipt at 22:46. This is the second Rare Candy, separate from Mt. Moon's item ball. |
| 22:54–23:00 | Pass Center without entering; enter Cerulean Gym. | No nurse heal or PP refill. |
| 23:01–23:14 | Use first Rare Candy on A: Lv.18→19; learn **Bite over Bubble**. | A becomes 11/56 HP. Confirmation names forgotten Bubble. |
| 23:15–23:19 | Use second Rare Candy: Lv.19→20, 14/59 HP. | Rare Candy removed from bag after final use. |
| 23:20–23:28 | Use **two Potions** on A, restoring 14→34→54/59 HP. | Nine Potions before; seven after by two observed uses. Rattata/Pidgey not healed. |
| 23:29–23:51 | Follow Gym path around pool to Misty; bypass both Gym trainers and begin leader battle. | Moves visibly Tackle / Water Gun / Bite / Mega Kick. |
| 23:52–24:04 | Staryu Lv.18 uses Harden; A's **Bite critical hit** KOs it. | A stays 54/59 HP. |
| 24:05–24:29 | Two Bites defeat Starmie Lv.21. | First Swift is a critical hit, 54→25/59 HP; second Swift reduces 25→10. A reaches Lv.21, 12/61 HP after winning. |
| 24:30–24:42 | Receive Cascade Badge, 2,100 prize money and TM03. | Complete badge explanation and TM receipt before opening bag. |
| 24:43–24:56 | Use TM03 to teach **Water Pulse**, replacing **Tackle**. | Forgotten Tackle and learned Water Pulse confirmed. TM39 remains in TM Case. |
| 24:57–25:00 | Start walking back along Gym path toward exit. | No heal after Misty in this chunk. |

**Boundary state at 25:00:** leaving Cerulean Gym, Wartortle A Lv.21,
12/61 HP. Moves now Water Pulse / Water Gun / Bite / Mega Kick in the observed
replacement order; verify the next battle menu before relying on slots.
Rattata Lv.3 10/15 and Pidgey Lv.7 5/24 last read during tutoring/preparation.
Boulder + Cascade Badges; Helix Fossil; TM03 consumed, TM39 retained.
Potions 7, Awakenings 3, Antidote 1, Poke Balls 3; all Repels and both Rare
Candies used. Money **3,632 derived** (828 + 416 + 288 + 2,100).
No save/reset or Center visit observed.

**Automation relevance:** hidden items need an interaction target and receipt
check, not an item-ball detector. A level obtained by Rare Candy can interrupt
the bag workflow with move learning. This exact Misty outcome includes critical
hits on both sides; encode a battle policy, not a fixed assumption of identical
turn count or damage. Keep move identity separate from its current slot.


## 25:00–30:00 — Cerulean rival and Nugget Bridge

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [25:00–25:17](https://www.youtube.com/watch?v=CbfjWuRWH10&t=1500) | Leave Gym; use **one Potion** outside, then go north around the house toward bridge. | A restores 12→32/61 HP at 25:08. No Center visit. Six Potions remain by subtraction. |
| 25:18–25:33 | Rival RED approaches at bridge entrance and starts battle. | A Lv.21, 32/61 HP. Menu confirms Water Pulse / Water Gun / Bite / Mega Kick. |
| 25:34–25:45 | Pidgeotto Lv.17 uses Quick Attack; A uses Bite, then Water Pulse to KO. | HP 32→24/61. Bite is selected initially, then Water Pulse on the second turn; do not summarize as two Water Pulses. |
| 25:46–25:59 | Bulbasaur Lv.18: Bite causes flinch, then Mega Kick KOs. | No attack from Bulbasaur after the flinch; no further damage. |
| 26:00–26:13 | One Water Pulse KOs Abra Lv.16. | A reaches Lv.22, 27/64 HP. Visible totals: Attack 38, Defense 39, Sp. Atk 44, Sp. Def 44, Speed 37. |
| 26:14–26:28 | Rattata Lv.15 uses Quick Attack; one Water Pulse KOs it. Win 288. | HP 27→22/64. Opponent order is Pidgeotto, Bulbasaur, Abra, Rattata. |
| 26:29–26:48 | Finish rival dialogue; he walks away, returns, gives **Fame Checker**, then leaves again. | Receipt at 26:42. Do not resume bridge navigation after only the first departure. |
| 26:49–27:44 | Enter Route 24 and battle bridge trainer 1, Bug Catcher Cale. | Caterpie, Weedle, Metapod, Kakuna, all Lv.10, each defeated with one **Bite**. Prize 120. No healing/damage. |
| 27:45–28:32 | Battle bridge trainer 2, Lass Ali. | Pidgey Lv.12: one Bite; Oddish Lv.12: one Mega Kick; Bellsprout Lv.12: one Bite, **critical hit**. A reaches Lv.23, 24/66 HP. Prize 192. |
| 28:33–29:09 | Battle bridge trainer 3, Youngster Timmy. | Sandshrew Lv.14: one Bite; Ekans Lv.14: Intimidate triggers, then one Water Pulse KOs it. Prize 224. |
| 29:10–29:42 | Battle bridge trainer 4, Lass Reli. | Nidoran male Lv.16 then Nidoran female Lv.16: one Water Pulse each. Prize 256. A stays 24/66 HP. |
| 29:43–30:00 | Begin bridge trainer 5, Camper Ethan, against Mankey Lv.18. Use Water Pulse. | Water Pulse has 14/20 PP before this use. At 29:59 Mankey's HP bar has emptied; finish faint/EXP/victory sequence in next chunk. |

**Boundary state at 30:00:** Camper Ethan's Mankey battle ending; do not count
his reward until its receipt. A Wartortle Lv.23, 24/66 HP; no party switch.
Water Pulse 13/20, Bite 13/25, Mega Kick 3/5 **derived from observed uses**;
Water Gun last read 5/25 before Misty and unused since. Fame Checker added.
Potions 6; other tracked consumables unchanged. Money **4,712 derived**
(3,632 + 288 + 120 + 192 + 224 + 256), before Ethan's reward.
No save/reset or nurse heal observed.

**Automation relevance:** the rival's return-and-reward scene is a multi-stage
script. Bridge progression needs persistent defeated-trainer state across five
separate encounters; move choice varies by opponent rather than one attack
being held for the whole bridge.


## 30:00–35:00 — bridge reward, Route 25 items and Bill

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [30:00–30:07](https://www.youtube.com/watch?v=CbfjWuRWH10&t=1800) | Complete Ethan's Mankey faint/EXP/win sequence. Prize 360. | Same Water Pulse and same opponent as the end of the preceding chunk. |
| 30:08–30:20 | Talk to mystery trainer at bridge north end; receive **Nugget**, then recruitment dialogue leads to battle. | Receipt at 30:11 precedes the battle, not its victory reward. |
| 30:21–30:59 | Rocket Grunt: Ekans Lv.15 and Zubat Lv.15 each fall to one Water Pulse. Win 480. | Ekans triggers Intimidate; A reaches Lv.24, 27/69 HP after it. Finish post-battle recruitment dialogue. |
| 31:00–31:08 | Go east onto Route 25; interact with ground near northwest cliff to obtain **hidden Elixir**. | Receipt at 31:07; nearby item ball farther west is not collected. |
| 31:09–31:36 | Weave between shrubs and trainers; battle Hiker Wayne. | His Onix Lv.17 falls to one Water Pulse. Prize 612; no damage. |
| 31:37–32:11 | Continue east and battle Picnicker Kelsey. | Nidoran male Lv.15, then female Lv.15, one Water Pulse each. Prize 300. |
| 32:12–32:51 | Trigger Camper Flint's sightline and battle him. | Rattata Lv.14: one Water Pulse; Ekans Lv.14: Intimidate then one Water Pulse, **critical hit**. Prize 280. A still 27/69 HP. |
| 32:52–32:56 | Collect **hidden Oran Berry** near flower patch. | Receipt and Berry Pouch storage text visible; no giving/using it here. |
| 32:57–33:18 | Start Lass Haley battle near Bill's approach; first Oddish Lv.13 falls to one Mega Kick, critical hit. | A reaches Lv.25, 30/72 HP. |
| 33:19–33:25 | Decline learning **Rapid Spin**. | “A did not learn RAPID SPIN” confirmation. Keep existing moves. |
| 33:26–33:49 | Second Oddish Lv.13 falls to one Mega Kick. Pidgey Lv.13 uses Quick Attack, then falls to one Bite. Win 208. | Quick Attack reduces HP 30→27/72. Both Mega Kicks hit; this does not establish a guaranteed hit policy. |
| 33:50–33:56 | Pass Bill's house front, collect **hidden Ether** east of its door, return to enter. | Receipt at 33:54; distinct from hidden Elixir earlier. |
| [33:57–34:10](https://www.youtube.com/watch?v=CbfjWuRWH10&t=2037) | Speak to transformed Bill, agree to help, wait for him to enter machine, interact with PC. | Player initiates the separation sequence; dialogue alone is insufficient. |
| 34:11–34:27 | Wait through machine animation and Bill emerging/walking; approach human Bill. | No movement during the automatic machine sequence. |
| 34:28–34:38 | Speak to restored Bill; receive **S.S. Ticket** and complete explanation. | Receipt at 34:31–34:32. |
| 34:39–34:54 | Exit, take southern return corridor west across Route 25, then descend the east side of Nugget Bridge. | Return route bypasses the defeated-trainer shrub maze. |
| 34:55–35:00 | Open bag, move Potion down the item list, then **use Ether on A**. | Bag confirms Potions 6, Awakenings 3, Antidote 1, Nugget 1, Elixir 1, Ether 1. A remains 27/72 HP. Denser inspection confirms Ether selected at 34:57.5 and PP-restored message at 34:59.9. The move-selection cursor moves too quickly to identify its final target directly; subsequent PP use supports Mega Kick. |

**Boundary state at 35:00:** PP-restoration confirmation beside the southern end
of the bridge; A Wartortle Lv.25, 27/72 HP, Ether used.
Moves unchanged; Water Pulse **6/20**, Bite **12/25** derived
from observed uses. Mega Kick was 1/5 before Ether; restoration to 5/5 is
inferred from subsequent uses and the unchanged Water Pulse PP. Rapid Spin declined. Nugget, Elixir, Ether, Oran Berry and
S.S. Ticket obtained. Ether consumed; all six Potions retained. Other party
members remain Rattata Lv.3 10/15, Pidgey Lv.7 5/24 in the party view.
Money **6,952 derived**. No save/reset or Center heal observed.

**Automation relevance:** bag ordering changes independently of item identity.
Bill requires separate conversation, machine-entry wait, PC interaction,
animation wait, second conversation and ticket-receipt checks. Track hidden
pickups individually, including the unused PP-restoring items carried onward.


## 35:00–40:00 — Dig Rocket, Vermilion supplies and S.S. Anne

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [35:00–35:09](https://www.youtube.com/watch?v=CbfjWuRWH10&t=2100) | Finish Ether confirmation, close bag, enter robbed Cerulean house and exit through rear hole. | No Potion used at this boundary; A remains 27/72 HP. Ether details corrected in preceding block after dense menu review. |
| 35:10–35:30 | Speak to Rocket behind house; one Water Pulse defeats Machop Lv.17. | No damage. |
| 35:31–35:49 | Drowzee Lv.17: Mega Kick misses; Drowzee uses Confusion; second Mega Kick hits and KOs. Win 544. | HP 27→21/72. Explicit miss/retry demonstrates why a fixed successful-hit sequence is insufficient. |
| 35:50–35:55 | Recover **TM28** from Rocket; finish scene and leave east/south. | Receipt before departure; no return to house owner. |
| 35:56–36:22 | Run down Route 5's east edge, enter Underground Path, traverse south and emerge on Route 6. | Bypass Day Care and Saffron gate. No underground item interaction observed. |
| 36:23–36:29 | Loop north/west of Route 6's underground entrance and obtain **hidden Rare Candy**. | Receipt at 36:27. Third Rare Candy of run. |
| 36:30–36:47 | Follow route around pond and trigger Camper Jeff on southern approach. | Other visible trainers bypassed; no wild encounter. |
| 36:48–37:14 | Spearow Lv.16: one Water Pulse. Raticate Lv.16: Quick Attack, then one Water Pulse KOs. | HP 21→13/72, then A grows Lv.26, 15/74 HP. Reward text missed in one-second samples; next money total reconciles with a 320 reward, marked derived. |
| 37:15–37:23 | Enter Vermilion; go directly to Mart, passing Center. | No nurse heal. |
| [37:24–37:37](https://www.youtube.com/watch?v=CbfjWuRWH10&t=2244) | Buy **5 Super Potions, 3 Parlyz Heals, 3 Repels**. | First two quantities visible in baseline frames; Repels confirmed with quarter-second inspection. Money 7,816→4,316→3,716→2,666. No selling Nugget. |
| 37:38–37:54 | Go to harbor, show ticket at sailor, board S.S. Anne, turn west and take stairs. | No optional cabin/trainer or onboard healing visit. |
| 37:55–37:59 | Use **one Potion** on A. | 15→35/74 HP; bag Potions 6→5. This is the first Potion after the one outside Misty's Gym. |
| 38:00–38:05 | Use Route 6 Rare Candy: A Lv.26→27, 37/76 HP. | Rare Candy consumed; no new move prompt observed. |
| 38:06–38:28 | Traverse upper corridor east, then north toward captain; rival approaches and starts battle. | Water Pulse **3/20 PP** visible before choosing first attack; no PP refill on boarding. |
| 38:29–38:43 | Pidgeotto Lv.19: Bite, then Water Pulse (critical hit) KOs. | Opponent's Gust between them is also a critical hit, reducing HP 37→20/76. |
| 38:44–38:57 | Ivysaur Lv.20: Bite causes flinch, then Mega Kick KOs. | Same two-move pattern as prior rival Bulbasaur. |
| 38:58–39:08 | One Water Pulse KOs Raticate Lv.16. | Battle text confirms Water Pulse despite menu cursor briefly passing Water Gun. |
| 39:09–39:29 | Kadabra Lv.18 uses Disable on Water Pulse; one Bite KOs it. Win 720. | A grows Lv.28, 23/79 HP. Opponent order Pidgeotto, Ivysaur, Raticate, Kadabra. |
| 39:30–39:38 | Finish rival departure, climb stairs and approach captain. | No heal between rival and captain. |
| 39:39–39:54 | Speak to captain, back-rubbing scene; receive **HM01 Cut**. | Receipt at 39:49–39:51, then finish Cut explanation. |
| 39:55–40:00 | Leave captain room, open TM Case, teach **Cut to Rattata**. | Learning confirmation starts at 39:59; complete text/bag exit in next chunk. No move-deletion scene seen, consistent with an empty move slot. |

**Boundary state at 40:00:** S.S. Anne upper stairs/corridor, Rattata Cut-learning
confirmation. A Wartortle Lv.28, 23/79 HP; moves unchanged. Water Pulse **1/20**,
Water Gun **5/25** (read at 39:00), Bite **9/25**, Mega Kick **2/5** (last two
counts derived, with Ether-to-Mega-Kick inference above). Rattata/Pidgey HP
unchanged. Potions 5, Super Potions 5, Parlyz Heals 3, Repels 3, Awakenings 3,
Antidote 1, Elixir 1, Nugget 1, Oran Berry 1. No Ether or Rare Candy left.
HM01 retained; TM28 and TM39 carried. Money **3,386 derived** (2,666 + 720).
No save/reset or Center/cabin heal observed.

**Automation relevance:** teach field moves to the intended party member and
check their learned-move receipt. A new move can be disabled mid-battle; the
battle policy needs alternatives. Budget both HP and PP before optional travel;
this run is nearly out of Water Pulse despite having purchased healing supplies.


## 40:00–45:00 — ship heal, Surge switches, Thunder Badge and Bicycle

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [40:00–40:13](https://www.youtube.com/watch?v=CbfjWuRWH10&t=2400) | Confirm Rattata learned Cut; backtrack west along upper corridor, down stairs, east along main corridor, enter healing cabin. | No other onboard trainer battle. |
| 40:14–40:24 | Speak to cabin woman, accept rest, wait through healing fade, leave room. | First observed full-party healing stop since the early scripted restoration. A's next battle shows **79/79 HP**; Water Pulse PP also restored. Other party members' post-heal numeric HP not reopened here. |
| 40:25–40:49 | Exit ship; wait through S.S. Anne departure before walking north up gangway. | Departure animation consumes roughly 20 seconds; do not treat it as stalled navigation. |
| 40:50–41:04 | Leave harbor, go west to Gym tree, use Rattata's Cut, enter Gym. | Tree removed only after confirmation and field-move animation. |
| [41:05–41:19](https://www.youtube.com/watch?v=CbfjWuRWH10&t=2465) | Search trash cans through lower/left/upper portions of Gym; multiple “only trash” failures. | Brief menu opening around 41:16 does **not** show a completed save. Exact can-by-can coordinates and the purpose of this menu are not established by this action-summary pass. |
| 41:20–41:43 | Trigger Gentleman Tucker by upper-right barrier; one Water Pulse defeats Pikachu Lv.23. Win **1,656**. | Reward confirmed with quarter-second inspection. A remains Lv.28, 79/79 HP. Only this ordinary Gym trainer fought here. |
| 41:44–42:01 | Resume searching cans, down right side then back into central area; open/close bag briefly. | Bag at 42:00 confirms Potions 5, Super Potions 5, Parlyz Heals 3, Repels 3, Nugget 1, Elixir 1. No item use shown. |
| 42:02–42:04 | Find first switch and open first electric lock. | Receipt text confirmed with dense review; player stands among middle-row cans. Do not prescribe this run's random switch location as a fixed route tile. |
| 42:05–42:16 | **Save game** after finding first switch. | Warns that a different save already exists; confirm overwrite, wait through SAVING, then “A saved the game.” Visible save summary: player A, 2 badges, Pokedex 4, time 0:40. First completed save observed in reviewed footage. |
| 42:17–42:23 | Interact with adjacent can, open second lock; brief bag/menu opening, then walk through disabled barriers. | Both barriers visibly off by 42:22. No failed second-switch attempt or reset observed. Saving before the attempt is observed; its intended recovery strategy is an inference. |
| 42:24–42:40 | Approach Lt. Surge, advance introduction, begin battle. | A Lv.28, 79/79 HP. |
| 42:41–42:52 | Voltorb Lv.21 uses Screech; one Water Pulse KOs it. | No HP loss; Defense harshly lowered. |
| 42:53–43:02 | One Water Pulse KOs Pikachu Lv.18. | Water Pulse 18/20 PP before use, consistent with cabin refill and one use each on Tucker and Voltorb. |
| 43:03–43:12 | Raichu Lv.24 uses Double Team; A lands Water Pulse, leaving it green HP. | No miss on this Water Pulse. |
| 43:13–43:28 | Raichu uses Shock Wave; A lands Mega Kick and KOs it. Win 2,400 and Thunder Badge. | Shock Wave reduces A **79→43/79 HP**. Mega Kick hits despite raised evasion. No level increase. |
| 43:29–43:35 | Finish badge explanation and receive **TM34**. | Receipt at 43:32–43:33; no teaching it here. |
| 43:36–43:49 | Leave Gym, Cut the respawned entrance tree again, head north/west. | Tree exists again after leaving interior; previous Cut does not permanently clear it. |
| [43:50–44:16](https://www.youtube.com/watch?v=CbfjWuRWH10&t=2630) | Enter Pokemon Fan Club; speak to chairman and hear Rapidash story; receive **Bike Voucher**. | Receipt at 44:04–44:07; complete remaining explanation, then leave. |
| 44:17–44:41 | Head north through Route 6, cross small grass strip, enter Underground Path and traverse north to Route 5. | No wild battle, Repel use or new pickup observed. Do not infer that a brief grass crossing is guaranteed encounter-free. |
| 44:42–44:53 | Run north along Route 5's east path; Cut southern Cerulean tree and enter city. | Rattata field-move text at 44:49. This shortcut avoids returning through robbed house. |
| 44:54–45:00 | Enter Cerulean Bike Shop and exchange voucher for **Bicycle**. | Receipt text fully visible by 44:59.5 and still displayed at exact 45:00. Registration/riding has not yet been reviewed. |

**Boundary state at 45:00:** inside Cerulean Bike Shop at Bicycle-exchange
confirmation. A Wartortle Lv.28, **43/79 HP**, moves Water Pulse / Water Gun /
Bite / Mega Kick. PP after cabin heal and observed Gym uses: Water Pulse **16/20**,
Water Gun **25/25**, Bite **25/25**, Mega Kick **4/5**, derived except the
intermediate Water Pulse reading. Rattata Lv.3 knows Cut; Pidgey Lv.7 remains
in party. Cabin rest is expected to restore their HP, but no post-rest numeric
party audit was captured; prior injured values should not be carried forward
as current readings. Three badges; TM34 added; Bicycle obtained, voucher
exchanged. Potions 5, Super Potions 5, Parlyz Heals 3, Repels 3, Awakenings 3,
Antidote 1, Nugget 1, Elixir 1, Oran Berry 1; no further uses in this block.
Money **7,442 derived** (3,386 + 1,656 + 2,400). One completed save at 42:16;
no reset observed. **Next review starts at 45:00, during this receipt dialogue.**

**Automation relevance:** the trash puzzle is a search/state problem with
first-lock, second-lock and optional save/retry states, not a fixed walking
macro. This run succeeds on its observed second-lock attempt; it supplies no
visual example of failure recovery. A cabin heal, departure animation, Cut
animation, save write and lengthy chairman dialogue each require different
completion evidence. Field obstacles may reappear after map transitions.


## 45:00–50:00 — Bicycle, Route 9 and dark Rock Tunnel

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [45:00–45:11](https://www.youtube.com/watch?v=CbfjWuRWH10&t=2700) | Finish Bicycle receipt, leave shop, reorder Key Items and register Bicycle; mount outside. | TM Case moved lower in list; Bicycle shows SELECT registration marker. |
| 45:12–45:26 | Cut southern Cerulean tree again, cycle east/north around city, Cut Route 9 entrance tree. | Rattata performs both field moves. |
| 45:27–46:23 | Battle Picnicker Alicia. Oddish Lv.18: Mega Kick; Bellsprout Lv.18: Bite; second Oddish Lv.18: Mega Kick; second Bellsprout Lv.18: Bite. | One hit each. A reaches Lv.29, 45/81 HP after first Bellsprout. Prize 360. |
| 46:24–46:42 | Drop ledge, cycle east around rock formations, trigger Bug Catcher Conner. | Other visible trainers bypassed. |
| 46:43–47:19 | Caterpie Lv.20 and Weedle Lv.20 each fall to one Bite. Venonat Lv.20: Mega Kick misses, Supersonic confuses A, second Mega Kick hits and KOs. | No self-hit or HP loss; Mega Kick shows 1/5 before retry and is exhausted afterward. Prize 240. |
| 47:20–47:26 | Collect **hidden Rare Candy** at northeast rock corner, then drop ledge eastward. | Receipt at 47:23. |
| 47:27–47:37 | Reach Route 10, bypass Center, Cut tree west of Center and enter Rock Tunnel. | Tunnel remains dark; no Flash use. |
| 47:38–47:46 | Use **Elixir on A**, then first newly purchased Repel; mount Bicycle. | Elixir receipt restores PP. Party now confirms Rattata **15/15**, Pidgey **24/24**, validating the earlier cabin heal. A 45/81. |
| 47:47–48:32 | Pokemaniac Ashton: Cubone Lv.23 falls to one Water Pulse; Slowpoke Lv.23 takes two Bites, using Tackle between them. | A loses 6 HP, then grows Lv.30, **42/84 HP**. Prize 1,104. |
| 48:33–48:51 | Continue through dark passages, descend ladder and trigger Pokemaniac Winston. | Small light radius only; exact path tiles not transcribed. |
| 48:52–49:11 | Slowpoke Lv.25 takes two Bites; Water Gun between them reduces A 42→39/84 HP. | Win; reward text not captured in baseline frames, amount unverified here. |
| 49:12–49:45 | Picnicker Martha: Oddish Lv.22 and Bulbasaur Lv.22 each fall to one Mega Kick. Prize 440. | Mega Kick 5/5 before Oddish, 4/5 before Bulbasaur, confirms Elixir refill. |
| 49:46–49:59 | Repel expires; use second Repel (two in bag before use), continue and trigger Hiker. | Battle transition underway at 50:00; trainer identity not yet displayed. |

**Boundary at 50:00:** dark Rock Tunnel, new Hiker battle starting. A Wartortle
Lv.30, **39/84 HP**; moves unchanged, Mega Kick 3/5, Bite 21/25 and Water Pulse
19/20 derived after Elixir and observed uses. Rare Candy 1 unused; Repels 1 left,
second application active; Elixir consumed. Other medicine unchanged from 45:00.
No healing, save or reset in this block. Winston's prize remains unverified;
do not carry a falsely precise money total beyond that point.

**Automation relevance:** Bicycle registration/mounting and moving Key Items
are separate menu actions. This dark-tunnel route would need map-local position
and transition evidence; the tiny visible radius cannot justify invented video
coordinates. Confusion and an exhausted move require battle-policy branches.


## 50:00–55:00 — Rock Tunnel exit, Lavender supplies and Route 8

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [50:00–50:20](https://www.youtube.com/watch?v=CbfjWuRWH10&t=3000) | Hiker Allen's Geodude Lv.25 falls to one Water Pulse. Prize 900. | A remains 39/84 HP. Water Pulse 19/20 before use confirms preceding count. |
| 50:21–51:23 | Traverse ladder and battle Hiker Oliver: first Onix Lv.20 takes two Bites, using Harden between; second Onix Lv.20 falls to Water Pulse; Geodude Lv.20 falls to Bite. | A reaches Lv.31, 41/86 HP, after second Onix; **decline Protect**. Prize 720. |
| 51:24–51:38 | Traverse another ladder, use last ordinary Repel, approach Picnicker Sofia. | Repel 1 visible before use; no ordinary Repels afterward. |
| 51:39–52:19 | Sofia: Jigglypuff, Pidgey and Meowth, all Lv.21, each defeated with one Water Pulse. Prize 420. | Menu at 51:48 shows **Water Gun / Water Pulse / Bite / Mega Kick**; the two water moves have swapped relative to earlier menus. Exact swap input not isolated. |
| 52:20–53:09 | Hiker Dudley: two Geodude Lv.21, one Bite each; Graveler Lv.21, one **Water Gun**. Prize 756. | A reaches Lv.32, 44/89 HP. Different water move deliberately selected for Graveler; no damage. |
| 53:10–54:01 | Traverse another ladder, battle Picnicker Dana: Meowth Lv.20, Bite; Oddish Lv.20, Mega Kick; Pidgey Lv.20, Bite after its Quick Attack. | One hit each; HP 44→39/89. Reward text missed; amount unverified in this pass. |
| 54:02–54:17 | Exit Rock Tunnel south to Route 10; cycle past trainers into Lavender and enter Mart. | No Center heal or Pokemon Tower visit. |
| [54:18–54:29](https://www.youtube.com/watch?v=CbfjWuRWH10&t=3258) | Buy **4 Escape Ropes and 7 Super Repels**, then leave. | Quarter-second review confirms final quantities, not transient scrolling values. Money **13,982→11,782→8,282**. No Nugget sale. |
| 54:30–54:50 | Cycle west onto Route 8; trigger Gamer Rich. Growlithe Lv.24 triggers Intimidate. | A still Lv.32, 39/89 HP. |
| 54:51–55:00 | One **Water Gun** defeats Growlithe. Rich battle continues. | 24/25 Water Gun PP before use. Next opponent not yet displayed. |

**Boundary at 55:00:** Route 8, Gamer Rich fight, Growlithe defeated and next
send-out pending. A Lv.32, **39/89 HP**; observed move order Water Gun / Water
Pulse / Bite / Mega Kick. PP derived: 23/25, 14/20, 14/25, 2/5 respectively.
Rare Candy 1 unused; Escape Ropes 4, Super Repels 7, ordinary Repels 0. Last
ordinary Repel may still be active; expiry not yet observed. Other medicine
unchanged. Money **8,282 directly read** reconciles prior unknown rewards
without inventing their individual amounts. No heal/save/reset in this block.

**Automation relevance:** decline unwanted level-up moves without accidentally
replacing a useful one. The runner conserves stronger-water-move PP with Water
Gun and Bite on selected opponents; fixed-slot or single-attack policies miss
both the reorder and this resource management.


## 55:00–1:00:00 — Celadon Rocket Hideout and Arbok trouble

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [55:00–55:15](https://www.youtube.com/watch?v=CbfjWuRWH10&t=3300) | Gamer Rich sends Vulpix Lv.24; one Water Gun KOs it. Finish battle. | Prize text not captured in baseline; no damage. |
| 55:16–55:46 | Cycle west past Route 8 trainers, enter east–west Underground Path, emerge Route 7 and enter Celadon Game Corner. | No grass battle, heal or Saffron gate visit. |
| 55:47–56:25 | Fight poster guard: Raticate Lv.20, one Mega Kick; Zubat Lv.20, one Water Pulse. Prize 640. | A remains Lv.32, 39/89 HP. |
| 56:26–56:38 | Press switch behind poster; descend newly revealed stairs, then B2F and B3F stairs. | Poster interaction is separate from defeating guard. |
| 56:39–56:50 | Traverse B3F arrow-floor puzzle; detour south to collect **BlackGlasses**, then take B4F stairs. | Receipt at 56:47. Not equipped yet. Exact arrow entry tiles not transcribed; source frames preserve route. |
| 56:51–57:30 | Fight Lift Key Grunt: Koffing Lv.21 and Zubat Lv.21, one Water Pulse each. Prize 672. | A reaches Lv.33, 42/92 HP after Koffing. |
| 57:31–57:38 | Finish dropped-key dialogue; pick up **Lift Key** from floor, return upstairs. | Receipt at 57:33. Winning alone does not obtain key. |
| 57:39–57:48 | Collect **Rare Candy** beside B3F arrow tiles, return to B2F. | Receipt 57:42; now two Rare Candies carried, including Route 9 pickup. |
| 57:49–58:09 | Traverse B2F arrow maze, collect **Moon Stone**, reach lift and select B4F. | Moon Stone receipt at 57:52. Stops/arrow rides are observed; no tile/input sequence asserted. |
| 58:10–59:01 | Fight left door guard: Sandshrew Lv.23, Bite; Sandslash Lv.23, Water Gun; Ekans Lv.23, Water Pulse. Prize 736. | One hit each; Ekans Intimidate. No damage. |
| 59:02–59:43 | Start right door guard: Ekans Lv.23 falls to Water Pulse; Sandshrew Lv.23 to Water Gun; Arbok Lv.23 enters and triggers Intimidate. | Water Pulse 10/20 before Ekans, 9/20 afterward. |
| 59:44–59:59 | Bite damages Arbok. Its Glare paralyzes A; subsequent Bite makes A flinch, then Poison Sting damages A while full paralysis prevents the selected attack. | HP 42→34 from Bite, then 34→29 from Poison Sting. Water Pulse selected but no successful use in these turns. Another Poison Sting begins at 59:59. Fight unresolved. |

**Boundary at 1:00:00:** right B4F door guard's Arbok fight, A Wartortle Lv.33,
**paralyzed**, last settled HP **29/92** with another Poison Sting underway.
Arbok remains green HP. PP last evidenced/derived: Water Gun 20/25, Water Pulse
9/20, Bite 12/25, Mega Kick 1/5. Lift Key, BlackGlasses, Moon Stone and a second
Rare Candy obtained. BlackGlasses not yet given. No healing, save or reset in
this block. Resume the same troubled battle; do not count it as won.

**Automation relevance:** trainer victory, dropped-key dialogue and physical
key pickup are distinct events. Forced arrow motion needs stop-tile recognition.
Paralysis, flinching and prevented attacks make both turn count and PP accounting
conditional; selected moves are not necessarily executed moves.


## 1:00:00–1:05:00 — Blastoise, Giovanni, Fly and Tower rival setup

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [1:00:00–1:00:24](https://www.youtube.com/watch?v=CbfjWuRWH10&t=3600) | Finish Arbok fight: Poison Sting reduces HP 29→24; full paralysis; Bite reduces 24→16 and flinches A; Arbok uses Leer, then A finally lands Water Pulse and KOs. Prize 736. | A reaches Lv.34, 18/94 HP, still paralyzed. Same battle as preceding block. |
| 1:00:25–1:00:38 | Use **one Parlyz Heal**, then both Rare Candies: Lv.34→35→36. | Paralysis cured; two Parlyz Heals remain. HP becomes 21/97 then 23/99. |
| 1:00:39–1:01:04 | Allow Wartortle to evolve into **Blastoise**. | Next battle confirms **38/114 HP**; no Potion used. |
| 1:01:05–1:02:04 | Battle Giovanni: Onix Lv.25 and Rhyhorn Lv.24 each fall to one Water Gun; Kangaskhan Lv.29 takes two Water Pulses, using Tail Whip between. Prize 2,900. | No HP loss; A stays Lv.36, 38/114. |
| 1:02:05–1:02:17 | Finish Giovanni departure, pick up **Silph Scope**, use **Escape Rope**. | Scope receipt 1:02:10; Rope exits to Game Corner front. Three Ropes remain. |
| 1:02:18–1:02:41 | Cycle to Department Store, use lift to 5F and approach battle-item clerk. | No Center heal. |
| 1:02:42–1:02:55 | Buy **6 X Speeds, 21 X Specials, 5 X Accuracies and 2 Guard Specs.** | Quantities supported by subsequent bag counts and money: 15,694→13,594→6,244→1,494→94. No sale. |
| 1:02:56–1:03:31 | Lift to 1F, exit, cycle west, Cut Route 16 northern tree, traverse gatehouse and enter secluded house. | Bypass Snorlax using northern route. |
| 1:03:32–1:03:49 | Receive **HM02 Fly**, leave house, teach **Fly to Pidgey**. | Receipt and learned Fly confirmed; no move-deletion scene. |
| 1:03:50–1:03:58 | Use **one Super Repel**, give **BlackGlasses to A**, **throw away Moon Stone**. | Bag Super Repels 7→6; held-item confirmation; explicit discard-one Moon Stone message. |
| 1:03:59–1:04:11 | Select Pidgey's Fly and Lavender destination; arrive outside Center, walk into Pokemon Tower. | Landing outside Center is not a nurse heal. |
| 1:04:12–1:04:35 | Climb to rival floor; trigger RED, who sends Pidgeotto Lv.25. | A begins 38/114 HP. |
| 1:04:36–1:04:54 | Use **two X Specials**, taking Gust then Quick Attack (38→30→22/114). One Water Gun KOs Pidgeotto. | X Special 21→20 shown between uses; boosts persist for this battle. |
| 1:04:55–1:05:00 | Ivysaur Lv.25 enters; A uses Water Pulse. | Damage animation underway at boundary; do not count Ivysaur defeated yet. |

**Boundary at 1:05:00:** Tower rival battle, Water Pulse against Ivysaur underway.
A Blastoise Lv.36, **22/114 HP**, BlackGlasses held, two X Special boosts active.
Water Gun / Water Pulse / Bite / Mega Kick remain moves. Water Gun 17/25 and
Water Pulse 5/20 after this announced use are derived; Bite 12/25, Mega Kick 1/5.
Pidgey knows Fly; Rattata knows Cut. Rare Candies 0, Parlyz Heals 2, Escape Ropes 3,
Super Repels 6 (one active), X Specials 19, X Speeds 6, X Accuracies 5, Guard Specs 2.
Moon Stone discarded; Silph Scope obtained. Money 94 before unfinished rival win.
No save/reset or full heal in this block.

**Automation relevance:** stat increases from candies/evolution are not healing
item uses. Sequence evolution, held-item setup and battle boosts explicitly;
route goals now include Fly destinations and the Silph Scope story gate.


## 1:05:00–1:10:00 — Tower rival finish, Marowak and upstairs Rockets

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [1:05:00–1:05:07](https://www.youtube.com/watch?v=CbfjWuRWH10&t=3900) | Water Pulse finishes Ivysaur Lv.25 from previous block. | One attack total against it; no damage to A. |
| 1:05:08–1:05:29 | Gyarados Lv.23 triggers Intimidate; two Bites defeat it, first causing flinch. | A grows Lv.37, **24/116 HP**. |
| 1:05:30–1:06:01 | Growlithe Lv.22: one Bite after Intimidate. Kadabra Lv.20: one Water Gun. Win 900; finish rival departure dialogue. | No further damage. Opponent order across blocks: Pidgeotto, Ivysaur, Gyarados, Growlithe, Kadabra. |
| 1:06:02–1:06:35 | Climb through grave mazes, avoiding initial Channelers and central healing area. | No wild encounter or heal; next battle still shows 24/116 HP. Stair transitions visible. |
| 1:06:36–1:06:59 | Channeler Tammy: Haunter Lv.23 falls to one Water Gun. | No damage; reward text not captured. |
| 1:07:00–1:07:27 | Channeler Jennifer: Gastly Lv.24 falls to one Water Gun. Prize 768. | Water Gun 15/25 before use. |
| 1:07:28–1:07:52 | Channeler Emilia: Gastly Lv.24 falls to one Water Gun. | No damage; reward text not captured. |
| 1:07:53–1:08:03 | Collect **Rare Candy** in southern grave pocket; approach blocked staircase. | Receipt at 1:07:57. One carried. |
| 1:08:04–1:08:31 | Silph Scope reveals ghost as **Marowak Lv.30**; one Water Pulse KOs it. Finish spirit-departure dialogue. | A grows Lv.38, **27/119 HP**. No capture attempt. |
| 1:08:32–1:09:19 | Climb top floor; first Rocket Grunt: two Zubat Lv.25, one Water Gun each; Golbat Lv.25, one Water Pulse. Prize 800. | No damage; Water Pulse 4/20 before Golbat. |
| 1:09:20–1:10:00 | Trigger next Rocket Grunt: Koffing Lv.26, one Water Gun; Drowzee Lv.26, one Water Pulse. Win 832. | Water Gun 11/25 before Koffing, Water Pulse 3/20 before Drowzee. Fading back to overworld at boundary. |

**Boundary at 1:10:00:** top Tower floor, second Rocket just defeated; Fuji and
another Rocket remain ahead. A Blastoise Lv.38, **27/119 HP**, BlackGlasses held;
PP Water Gun **10/25**, Water Pulse **2/20**, Bite **9/25**, Mega Kick **1/5**,
last two derived. Rare Candy 1. No item use, full heal, save or reset in this
block. X Special boosts applied to the rival fight are not carried into later
battles. Other tracked inventory unchanged.

**Automation relevance:** distinguish wild encounters from the scripted
uncatchable Marowak encounter and its departure dialogue. The runner bypasses
healing while preserving low HP; copying the route needs an explicit policy.
Silph Scope possession is observable through ghost identification, not merely
having reached the Tower staircase.


## 1:10:00–1:15:00 — Fuji, Tea, Cycling Road and Safari rewards

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [1:10:00–1:10:11](https://www.youtube.com/watch?v=CbfjWuRWH10&t=4200) | Second Rocket departs; trigger third Rocket near Fuji. | Continues previous block's victory. |
| 1:10:12–1:11:03 | Third Rocket: two Zubat Lv.23, one Bite each; Rattata Lv.23, one critical Bite; Raticate Lv.23, one critical Water Gun. Win **736**. | Rattata's Quick Attack reduces A **27→21/119 HP**. Water Gun 10/25 before Raticate. |
| 1:11:04–1:11:24 | Talk to Fuji, teleport to his Lavender house; approach and talk again to receive **Poke Flute**. | Receipt around 1:11:17–20. The Tower conversation alone does not give the reward. |
| 1:11:26–1:11:39 | Pidgey uses Fly to **Celadon**; mount bike, head west. Repel wears off. | Land outside Center; no nurse visit. |
| 1:11:40–1:11:55 | Enter Celadon Mansion, talk to old lady at table, receive **Tea**, leave. | Receipt around 1:11:47–49. |
| 1:11:57–1:12:26 | Approach Route 16 Snorlax, play flute through interaction; enter **Snorlax Lv.30** battle and **RUN successfully**. Finish its departure text. | No attack or capture. Overworld obstacle disappears after fleeing. |
| 1:12:27–1:12:42 | Pass Cycling Road gate, cycle downhill along eastern side. Brief menu open/close near bikers. | No actual save observed; reason for menu is unresolved. |
| 1:12:43–1:12:53 | Collect hidden **Rare Candy**, then hidden **Max Elixir** farther downhill. | Two Rare Candies carried including Tower pickup. No trainer battles on this road. |
| 1:12:54–1:13:23 | Continue east through Route 18 to Fuchsia; enter Safari Zone, pay **500**, receive **30 Safari Balls**. | Money directly visible **5634→5134**; reconciles intervening unread prizes. |
| 1:13:24–1:13:47 | Take eastern Safari path; menu shows **548/600 steps** remaining. Use **Super Repel**; continue by bike and collect **Full Restore**. | Super Repels **6→5**. Pickup near rest house around 1:13:45–47. |
| 1:13:48–1:14:14 | Continue through grass, ponds and stairways; collect **Gold Teeth** around 1:14:07–09; enter Secret House. | No Safari encounter/capture observed. Exact tile path not measured here. |
| 1:14:15–1:14:37 | Talk to attendant, receive **HM03 Surf**; leave, use Pidgey's **Fly to Fuchsia**. | HM receipt around 1:14:19–21. Exit by Fly rather than waiting for 600 steps or selecting Retire. |
| 1:14:38–1:14:54 | Bike directly into Fuchsia Gym; open bag along right-side route. Use **Oran Berry** on A, **21→31/119 HP**. | Bag shows Parlyz Heal 2, Escape Rope 3, Super Repel 5, X Speed 6, X Special 19, X Accuracy 5. Berry Pouch had one Oran Berry. |
| 1:14:55–1:15:00 | Open TM Case and begin teaching **Surf** to A. | Move replacement is still pending at boundary; do not assume which move was deleted yet. |

**Boundary at 1:15:00:** inside Fuchsia Gym, Surf teaching in progress.
A Blastoise Lv.38, **31/119 HP**, BlackGlasses held. Before the pending
replacement: Water Gun **9/25**, Water Pulse **2/20**, Bite **6/25**, Mega Kick
**1/5** (PP derived from previous boundary and attacks). Rare Candy 2,
Max Elixir 1, Full Restore 1; Gold Teeth, Tea, Poke Flute and HM03 obtained.
Super Repel 5, Oran Berry 0, money **5134**. No full heal, save or reset
observed. Gold Teeth have not yet been returned; Strength is not yet obtained.

**Automation relevance:** flute obstacle handling can end in a successful
flee, rather than a mandatory KO/capture. Fuji needs a second conversation
after teleportation. Safari travel has its own step/ball counters and can end
with Fly; it does not require a general wild-battle/capture policy. Hidden
road items need explicit pickup targets. Preserve the distinction between
receiving Surf and successfully teaching it.


## 1:15:00–1:20:00 — Koga at low HP, Strength and Cinnabar approach

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [1:15:00–1:15:13](https://www.youtube.com/watch?v=CbfjWuRWH10&t=4500) | Finish teaching **Surf**, replacing **Water Gun**. Close menus. | Next battle confirms move slots **Surf / Water Pulse / Bite / Mega Kick**. Surf starts 15 PP. |
| 1:15:14–1:16:17 | Juggler Kirk: Drowzee Lv.31, Drowzee Lv.31, Kadabra Lv.31, Drowzee Lv.31. **One Surf each**; Kadabra hit is critical. | A grows **Lv.39, 34/122 HP** after first Drowzee. No damage. Prize number not captured. |
| 1:16:18–1:16:33 | Follow the Gym's invisible-wall corridor around upper edge, then approach Koga from west. | Other trainers bypassed; no additional battle. |
| 1:16:34–1:17:23 | Koga: **Koffing Lv.37**, one Surf; **Muk Lv.39**, two Surfs with enemy **Minimize** between them; second **Koffing Lv.37**, one Surf. | A grows **Lv.40, 38/126 HP** after Muk; neither Surf misses after Minimize. No player setup item used. |
| 1:17:24–1:17:42 | **Weezing Lv.43** survives Surf in red HP; its **Sludge** reduces A **38→4/126 HP**. Koga uses **Hyper Potion**; A uses **Bite** on that turn. | Weezing is restored to full before Bite damage. No player healing or status cure. |
| 1:17:43–1:18:03 | **Critical Surf** defeats Weezing. Win **4300**; receive **Soul Badge** and **TM06**. | A survives at **4/126 HP**. This is the observed outcome, not a generally safe battle recipe. |
| 1:18:04–1:18:24 | Retrace wall corridor, leave Gym, cycle east to Warden's house. | Pass Center without entering. |
| 1:18:25–1:18:39 | Give **Gold Teeth** to Warden; receive **HM04 Strength**, leave. | Receipt 1:18:34–36. No Strength teaching, boulder push or house item collection in this visit. |
| 1:18:40–1:18:59 | Use **Super Repel** (**5→4**), then **Potion** on A (**4→24/126 HP**, Potions **5→4**). Pidgey uses **Fly to Pallet**. | No full heal. Other party HP visible: Rattata 15/15, Pidgey 24/24. |
| 1:19:00–1:19:23 | Bike to Pallet's water edge, activate **Surf**, travel south on **Route 21**, reach Cinnabar, enter **Pokemon Mansion**. | Avoid swimmers; no wild battle. Surf works after Soul Badge. No visit to mother or Cinnabar Center. |
| 1:19:24–1:19:39 | Walk Mansion stairs up twice; press statue's secret switch on upper floor. | Explicit switch interaction at 1:19:37–38. Tile path not measured. |
| 1:19:40–1:19:59 | Use **Super Repel** (**4→3**); take floor drop, descend staircase to basement; press basement statue switch. | Drop around 1:19:46–48; switch 1:19:56–57. Trainers avoided. |

**Boundary at 1:20:00:** Mansion basement, moving west from the statue after
pressing its switch. A Blastoise **Lv.40, 24/126 HP**, BlackGlasses held.
Moves Surf / Water Pulse / Bite / Mega Kick; derived PP **5/15, 2/20, 5/25,
1/5**. Rare Candy 2, Max Elixir 1, Full Restore 1, Potions 4, Super Repel 3,
Escape Rope 3. HM04 obtained; Gold Teeth delivered. Koga is defeated; no
Secret Key yet. No full heal, save or reset observed.

**Automation relevance:** Koga's Hyper Potion changes the required attack
sequence; low-HP survival and a critical hit make this unsuitable as an
unconditional scripted win. Gym navigation needs wall geometry and trainer
avoidance. Mansion navigation adds switch-dependent doors and a deliberate
floor drop. Obtaining Strength does not establish that it has been taught.


## 1:20:00–1:25:00 — Secret Key, Blaine and return for Erika

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [1:20:00–1:20:23](https://www.youtube.com/watch?v=CbfjWuRWH10&t=4800) | Traverse Mansion basement; collect hidden **Elixir** (1:20:06–07), press another statue switch (1:20:09–10), collect **TM14** (1:20:15) and **Secret Key** (1:20:21–22). | No trainer or wild battle in Mansion. |
| 1:20:24–1:20:36 | Use **Escape Rope** to leave Mansion; bike into Cinnabar Gym. | Escape Ropes **3→2**. No Center stop or healing. |
| 1:20:37–1:21:43 | Answer all six quiz machines correctly, opening doors and bypassing every Gym trainer. | In order: Caterpie→Metapod **YES**; nine badges **NO**; Poliwag evolves three times **NO**; Electric effective against Ground **NO**; same kind/level Pokemon not identical **YES**; TM28 Tombstony **NO**. Prompts and successful door passage observed; some cursor changes occur between sampled frames. |
| 1:21:44–1:22:13 | Trigger Blaine; **Growlithe Lv.42** uses Intimidate, then falls to one **Surf**. | A begins **Lv.40, 24/126 HP**. No setup items. |
| 1:22:14–1:22:42 | **Ponyta Lv.40**, one Surf; **Rapidash Lv.42**, one Surf. | A grows **Lv.41, 26/128 HP** after Ponyta. Surf **3/15** before Rapidash. |
| 1:22:43–1:23:09 | **Arcanine Lv.47** uses Intimidate, falls to one Surf. Receive **Volcano Badge**, **4700**, and **TM38 Fire Blast**. | No enemy damaging turn or player item use; A remains 26/128 HP. Surf now 1/15, derived. |
| 1:23:10–1:23:44 | Walk back through open quiz rooms and exit. Bill approaches outside Gym; **decline** his island invitation, finish departure. | No Sevii trip. Refusal evident in his response and remaining on Cinnabar; choice cursor not captured in one-second pass. |
| 1:23:45–1:24:10 | Use Pidgey's **Fly to Celadon**, bike south, **Cut** entrance tree and enter Celadon Gym. | No Center visit. |
| 1:24:11–1:24:29 | Take west aisle; Beauty Tamia approaches and triggers battle. | Her sightline triggers on the way toward the northern area. |
| 1:24:30–1:24:59 | Tamia: two **Bellsprout Lv.24**, **one Bite each**. Grow **Lv.42, 29/131 HP**; decline **Rain Dance**; win **1728**. | Bite 5/25 before first, 4/25 before second. No damage. Overworld fade at boundary. |

**Boundary at 1:25:00:** Celadon Gym west aisle after Tamia; Erika not yet
challenged. A Blastoise **Lv.42, 29/131 HP**, BlackGlasses held. Moves remain
Surf / Water Pulse / Bite / Mega Kick; derived PP **1/15, 2/20, 3/25, 1/5**.
Elixir 1 and TM14 newly collected; Secret Key obtained, Blaine defeated.
Rare Candy 2, Max Elixir 1, Full Restore 1, Potions 4, Super Repel 3,
Escape Rope 2. No save, reset or full heal observed.

**Automation relevance:** the badge order is **Koga, Blaine, then Erika** here.
Gym quizzes can avoid six battles, but require question-aware answers and door
confirmation. Bill's invitation needs an explicit refusal branch. Mansion
completion includes confirmed key receipt and Escape Rope exit; reaching the
key room alone is insufficient. PP is now nearly exhausted across all moves,
so carry this state into the next menu sequence.


## 1:25:00–1:30:00 — Erika, Saffron access and Silph rival setup

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [1:25:00–1:25:29](https://www.youtube.com/watch?v=CbfjWuRWH10&t=5100) | Use both **Rare Candies** on A: **Lv.43, 32/134 HP**, then **Lv.44, 35/137 HP**. Teach **TM14 Blizzard**, replacing **Mega Kick**. | Before replacement: Surf 1/15, Water Pulse 2/20, Bite 3/25, Mega Kick 1/5 directly visible. Blizzard starts 5/5. |
| 1:25:30–1:26:03 | Cut inner Gym tree; Beauty Lori approaches. **Exeggcute Lv.24**, one **Bite**, wins. | No damage. Prize number missed in sampled text. |
| 1:26:04–1:26:41 | Challenge Erika: **Victreebel Lv.29**, one **Surf**; **Tangela Lv.24**, one **Bite**. | No damage. Last Surf PP consumed; Bite 2/25 before Tangela. |
| 1:26:42–1:27:06 | **Vileplume Lv.29**, one **Blizzard**; win **2900**, receive **Rainbow Badge** and **TM19**. | Blizzard hits; A remains **35/137 HP**. No setup items or healing. |
| 1:27:07–1:27:29 | Leave Gym; use **Fly to Celadon** to land at Center entrance, then bike east through Route 7. | Same-city Fly avoids walking back around the Cut entrance. No nurse visit. |
| 1:27:30–1:27:43 | Gate guard takes **Tea** and allows passage. Enter **Saffron**, bike directly into **Silph Co.** | Tea obtained much earlier, at 1:11. No Fighting Dojo or Sabrina visit here. |
| 1:27:44–1:28:14 | Climb stairs to **3F**; use warp pads via **5F**, briefly **9F**, then back to 5F corridor. Collect **Card Key** at 1:28:11–12. | No trainer battle. Warp routing bypasses guard rather than defeating him. Exact pad coordinates not measured. |
| 1:28:15–1:28:27 | Use **Max Elixir** on A, then **Super Potion**, restoring **35→85/137 HP**. | Max Elixir consumed; Elixir and Full Restore remain. Super Potions **5→4**, confirmed bag after use. All four moves' PP restored; HP restoration is separate. |
| 1:28:28–1:28:53 | Warp briefly to 9F and back to 5F; open Card Key door. Warp to 3F, open another door, take pad to rival's **7F** room. | Brief bag opening at 1:28:37–38; no additional consumed item established. No heal on 9F. |
| 1:28:54–1:29:10 | Rival dialogue and battle; **Pidgeot Lv.37** first. | A starts **Lv.44, 85/137 HP**. |
| 1:29:11–1:29:27 | Use **two X Specials**, then **one X Accuracy**, while Pidgeot repeatedly uses **Wing Attack**. | Bag X Specials **19→18→17**; X Accuracy **5→4**. HP after the three enemy turns **85→65→47→27/137**; intermediate animation values should not be counted as separate attacks. |
| 1:29:28–1:29:38 | **Critical Water Pulse** KOs Pidgeot. | One attack against it after setup. No additional damage. |
| 1:29:39–1:29:54 | **Venusaur Lv.40**, one **Blizzard** KOs; A grows **Lv.45, 30/140 HP**. | Water Pulse 19/20 directly visible before changing selection. Blizzard hits after X Accuracy. |
| 1:29:55–1:30:00 | **Gyarados Lv.38** sent out; **Intimidate** resolves. | Rival battle continues; Gyarados is still at full HP and A has not attacked it. |

**Boundary at 1:30:00:** Silph Co. 7F rival battle, Gyarados Lv.38 just sent
out after Pidgeot and Venusaur were defeated. A Blastoise **Lv.45, 30/140 HP**,
BlackGlasses held, **two X Special boosts and one X Accuracy active**.
Moves Surf / Water Pulse / Bite / Blizzard; PP **15/15, 19/20, 25/25, 4/5**
(restore and subsequent uses reconciled). Rare Candy 0, Max Elixir 0,
Elixir 1, Full Restore 1, Potions 4, Super Potions 4, Parlyz Heal 2,
Super Repel 3, Escape Rope 2, X Speed 6, X Special 17, X Accuracy 4,
Guard Spec. 2. Card Key obtained; Erika defeated. No full HP heal, save or
reset observed. Next block must continue this fight, not count a rival win yet.

**Automation relevance:** same-city Fly is a useful travel edge. Silph routing
needs a warp graph, key-controlled doors and correct destination recognition;
visiting a floor does not imply fighting or healing there. Distinguish
PP-restoring items from HP medicine and account for move replacement before
restoration. The rival setup deliberately spends HP on three enemy turns;
copying it requires damage margins and recovery branches, especially with only
30 HP left for Gyarados. Critical hits are observations, not prerequisites to
assume for an automated route.


## 1:30:00–1:35:00 — Silph rival finish, Giovanni, Sabrina

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [1:30:00–1:30:09](https://www.youtube.com/watch?v=CbfjWuRWH10&t=5400) | Continue the 7F rival fight: one **Blizzard** KOs **Gyarados Lv.38**; gain 1741 EXP. | Gyarados's Intimidate resolved at 1:29:59. A stays **30/140 HP**; Gyarados did not attack. |
| 1:30:10–1:30:24 | **Growlithe Lv.35** (Intimidate again); one **Bite** KOs it; gain 682 EXP. | Bite 25/25 before use (move menu 1:30:16). |
| 1:30:25–1:30:34 | **Alakazam Lv.35**; one **Surf** KOs it; gain 1395 EXP. | No damage taken in the whole rival fight after the setup turns. |
| 1:30:35–1:30:49 | Rival defeated; win **1440**. Post-battle dialogue; rival leaves. | A remains **Lv.45, 30/140 HP**. |
| 1:30:50–1:30:59 | Warp to **11F** (label at 1:30:50); a Rocket grunt stops the player ("Stop right there!"). | Exact pad sequence from 7F not measured. |
| 1:31:00–1:31:41 | Grunt: **Cubone Lv.32** (Bite), **Marowak Lv.32** (Water Pulse), **Drowzee Lv.32** (Water Pulse), one hit each. A grows **Lv.46, 33/143 HP**. Win **1024**. | EXP 595, 849, 699. No damage taken. |
| 1:31:42–1:31:53 | **Card Key** opens the boardroom door; Giovanni's pre-battle dialogue. | Player walks up to Giovanni beside the president's desk. |
| 1:31:54–1:32:50 | **Giovanni (Silph)**: **Nidorino Lv.37** Surf, **Nidoqueen Lv.41** Surf, **Kangaskhan Lv.35** Surf, **Rhyhorn Lv.37** Water Pulse; one hit each. Win **4100**. | Surf 12/15 before Kangaskhan (move menu 1:32:26). A stays **33/143 HP** throughout. |
| 1:32:51–1:32:55 | Giovanni leaves. **No president / Master Ball visit observed**; the player turns away at once. | Do not infer the Master Ball or Lapras gift. |
| 1:32:56–1:32:59 | Use **Escape Rope** from the bag. | Bag before use: Parlyz Heal 2, Escape Rope 2, Super Repel 3, X Speed 6, X Special 17, X Accuracy 4 (Guard Spec. 2, Full Restore 1 visible a frame earlier). |
| 1:33:00–1:33:08 | Land in **Saffron**; bike straight to **Saffron Gym** and enter. | No Center visit, no heal. |
| 1:33:08–1:33:29 | Warp-pad route through the Gym to **Sabrina**; no Gym trainer battles seen. | Pad sequence not measured. |
| 1:33:30–1:34:05 | **Sabrina**: **Kadabra Lv.38**, one Surf (A → **Lv.47, 36/146**); **Mr. Mime Lv.37**, one Surf. | EXP 1180, 1077. |
| 1:34:07–1:34:22 | **Venomoth Lv.38**: use one **X Speed** (6→5); Venomoth's **Psybeam** takes A **36→25/146**; one Surf KOs. | Surf **9/15** before the attack (1:34:14). |
| 1:34:23–1:34:38 | **Alakazam Lv.43**, one Surf; Sabrina defeated, win **4300**, **Marsh Badge**. | EXP 1713. A **Lv.47, 25/146 HP**. |
| 1:34:39–1:34:49 | Receive **TM04** (Calm Mind) and Sabrina's closing text. | |
| 1:34:50–1:34:53 | Use the last **Escape Rope** (1→0); appear outside in Saffron. | Bag at 1:34:50 shows Escape Rope 1 before use. |
| 1:34:54–1:34:59 | START → POKéMON → **Fly** (field move on Pidgey); town map open. | Party: A Lv.47 25/146; Rattata Lv.3 15/15; Pidgey Lv.7 24/24. |

**Boundary at 1:35:00:** Fly map open in Saffron. A Blastoise **Lv.47, 25/146
HP**, BlackGlasses held. Surf about 7/15 after Sabrina (derived), Water Pulse
~16/20, Bite 24/25, Blizzard 4/5. Escape Rope 0, X Speed 5, X Special 17,
X Accuracy 4, Guard Spec. 2, Full Restore 1, Elixir 1, Super Potion 4,
Parlyz Heal 2, Super Repel 3. Badges: all except Earth. No heal or save seen.

**Automation relevance:** Silph's boss route skips the president's gift
entirely; a default route may want it. Two Escape Ropes are spent as fast exits
from Silph and the Gym, so an automation needs either the item or a walk-out
route. One X Speed is used only against Venomoth; per-opponent item use is a
battle-plan detail, not a general rule.

## 1:35:00–1:40:00 — Viridian Gym, Earth Badge and Route 22

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [1:35:00–1:35:04](https://www.youtube.com/watch?v=CbfjWuRWH10&t=5700) | Fly (Pidgey animation) to **Viridian City**; land at the Center. | |
| 1:35:05–1:35:13 | Bike south-east to the **Viridian Gym** and enter. | No Center visit. |
| 1:35:13–1:35:22 | Ride the spinner tiles; **Black Belt Takashi** stops the player. | Spinner path not measured. |
| 1:35:23–1:36:07 | Takashi: **Machoke Lv.38**, **Machop Lv.38**, **Machoke Lv.38**, one Surf each. A grows **Lv.48, 28/149 HP**. | EXP 1188, 715, then the third. No damage taken. |
| 1:36:08–1:36:20 | More spinners; **Cooltrainer Warren** sees the player (!) and approaches. | |
| 1:36:21–1:37:30 | Warren: **Marowak Lv.37** ×2, **Rhyhorn Lv.38**, **Nidorina Lv.39**, **Nidoqueen Lv.39**, one **Water Pulse** each. | Water Pulse **13/20** before Nidorina (1:37:07). EXP 982, 982, —, 976, 1620. A stays **28/149**. |
| 1:37:31–1:37:46 | Spinners to **Giovanni**; dialogue. | |
| 1:37:47–1:38:59 | **Giovanni (Gym)**: **Rhyhorn Lv.45**, **Dugtrio Lv.42**, **Nidoqueen Lv.44**, **Nidoking Lv.45**, **Rhyhorn Lv.50**, one Water Pulse each. A grows **Lv.49, 32/153 HP** after the first. | Water Pulse **9/20** before Nidoqueen (1:38:24) and **7/20** before the last Rhyhorn (1:38:51). No damage taken. |
| 1:39:00–1:39:21 | Giovanni defeated: **Earth Badge**, win **5000**, receive **TM26** (Earthquake). | |
| 1:39:22–1:39:32 | Spinner back to the entrance; leave. | |
| 1:39:33–1:39:40 | Bike west out of Viridian to **Route 22**. | |
| 1:39:41–1:39:42 | Use a **Super Repel** (3→2). | |
| 1:39:43–1:39:48 | Use **Full Restore** on A: **32→153/153** ("restored by 121"). | Full Restore 1→0. Guard Spec. viewed at 1:39:49, not used. |
| 1:39:50–1:39:59 | Bike west; the **rival** walks over ("You collected all the BADGES… warm-up") and the battle starts. | |

**Boundary at 1:40:00:** Route 22 rival battle starting. A Blastoise
**Lv.49, 153/153 HP** (full), BlackGlasses. Water Pulse about 6/20 (derived),
Surf ~7/15, Bite 24/25, Blizzard 4/5. Full Restore 0, Elixir 1, Super Repel 2,
X Speed 5, X Special 17, X Accuracy 4, Guard Spec. 2, Super Potion 4,
Parlyz Heal 2. All eight badges.

**Automation relevance:** Viridian Gym is spinner routing plus two forced
trainers; it needs a spinner model, not walk-to-wall legs. The run heals once,
with its only Full Restore, right before the Route 22 rival, not at a Center.

## 1:40:00–1:45:00 — Route 22 rival, Badge Gates and Victory Road

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [1:40:00–1:40:09](https://www.youtube.com/watch?v=CbfjWuRWH10&t=6000) | Route 22 rival: **Pidgeot Lv.47** sent out. | A starts **Lv.49, 153/153 HP**. |
| 1:40:10–1:40:28 | Use **one X Accuracy** (4→3) and **one X Special** (17→16). Pidgeot uses **Wing Attack** (153→126), **FeatherDance** ×2 (Attack harshly fell). | Bag lists confirm both counts. |
| 1:40:26–1:40:39 | Pidgeot's Wing Attack (126→99). One **Surf** KOs Pidgeot; A grows **Lv.50, 102/156 HP**. | Surf **4/15** before use (1:40:29). Stats at Lv.50 shown: HP 156, Atk 105, Def 107, SpA 121, SpD 122, Spe 105. |
| 1:40:43–1:40:55 | **Venusaur Lv.53**: one **Blizzard**, KO. | EXP 2361. |
| 1:40:57–1:41:08 | **Rhyhorn Lv.45**: one **Bite**, KO. | EXP 1300. |
| 1:41:08–1:41:29 | **Gyarados Lv.45** (Intimidate): Bite (not very effective); Gyarados's **Hydro Pump** 102→88; one **Blizzard** KOs. | Bite **21/25** before the Blizzard selection (1:41:21). |
| 1:41:30–1:41:56 | **Growlithe Lv.45** (Intimidate): use **X Speed** (5→4); Growlithe uses Agility; one **Bite** KOs. A grows **Lv.51, 91/159**. | X Special now 15 in the bag view: a second X Special was used at an unsampled moment (16→15). |
| 1:41:57–1:42:10 | **Alakazam Lv.47**: one **Surf**, KO. Rival defeated; win **1908**. | Surf about 3/15 left (derived). |
| 1:42:11–1:42:22 | Rival leaves; bike west and north through the Route 22 gate and **Route 23**. | No Center visit. |
| 1:42:23–1:43:19 | **Badge checks** at the Route 23 gates: Boulder, Cascade, Thunder, Rainbow, Soul (Surf across water between gates), Marsh, Volcano, Earth. | Each guard's check is a forced dialogue on a fixed tile. |
| 1:43:20–1:43:21 | Enter **Victory Road**. | |
| 1:43:24–1:43:41 | Open the bag, then the **TM case**: teach **HM04 Strength**. The move-forget screen shows Surf **2/15**, Water Pulse **6/20**, Bite **20/25**, Blizzard **1/5**; one move is forgotten ("1, 2 and… Poof!"). | The forgotten slot is not readable at 1 fps. The Lorelei move menu at 1:47:13 shows **Surf / Strength / Bite / Blizzard**, so **Water Pulse** was replaced. |
| 1:43:44–1:44:59 | Strength boulder puzzle: push the first boulder onto its switch, climb, a second boulder, ladders; use **Super Repel** (2→1) at 1:44:43. Avoid trainers. | No battles seen. Exact pushes not measured. |

**Boundary at 1:45:00:** Victory Road, mid-route after the second boulder.
A Blastoise **Lv.51, 91/159 HP**, moves Surf 2/15 / Strength 15/15 /
Bite 20/25 / Blizzard 1/5. Super Repel active (1 left). X Speed 4,
X Special 15, X Accuracy 3, Guard Spec. 2, Elixir 1, Super Potion 4,
Parlyz Heal 2. Full Restore 0.

**Automation relevance:** Water Pulse is deleted for Strength right before the
Elite Four, and PP is very low (Surf 2, Blizzard 1). The Elixir and the League's free heal are
the planned recovery. Badge gates are fixed forced dialogues; Victory Road is a
boulder puzzle that needs a push model.

## 1:45:00–1:50:00 — Victory Road exit, Indigo Plateau, Lorelei, Bruno starts

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [1:45:00–1:45:37](https://www.youtube.com/watch?v=CbfjWuRWH10&t=6300) | Finish the Victory Road boulder puzzle: more Strength pushes (1:45:18–1:45:35), ladders between floors. | No battles. The Super Repel runs out on the last floor ("REP…" message at 1:45:42); not renewed. |
| 1:45:44–1:45:52 | Exit to **Route 23**'s top; bike up to the **Indigo Plateau**. | |
| 1:45:53–1:45:56 | Enter the Center; walk past the nurse to the PC. | Party HP is **full** when Lorelei starts (159/159) and Bite is **25/25**, so a nurse heal happened here, unsampled. |
| 1:45:57–1:46:09 | **Bill's PC → Deposit**: put **Pidgey** and **Rattata** in Box 1. | Party is now only **A**. |
| 1:46:15–1:46:21 | Plateau Mart: buy **Full Restores** (₽3000 each). | Money **₽41,818** before. Quantity not readable at 1 fps; the bag shows **Full Restore 13** at 1:48:36, so about **13** were bought (if the count was 0 before). |
| 1:46:22–1:46:33 | Walk into **Lorelei**'s room; her dialogue. | No save observed before the Elite Four. |
| 1:46:37–1:46:47 | Lorelei: **Dewgong Lv.52**. Open the bag on turn 1 (cursor on **X Speed**, 4 left); whether it was used is not readable. | Dewgong uses **Surf**: **159→152→140** (not very effective). |
| 1:46:53–1:47:09 | Use **X Special** twice (15→13→12, "A's Sp. Atk rose!"). Dewgong's **Ice Beam** (140→121), **Surf** (121→101), then **Safeguard**. | Bag counts read at 1:47:01 (13) and 1:47:08 (12). |
| 1:47:13–1:47:27 | Dewgong: **Bite** ×2 (one flinch), KO. | Move menu at 1:47:13: **Surf / Strength / Bite / Blizzard**, Bite **25/25**. EXP 1960. |
| 1:47:28–1:47:37 | **Cloyster Lv.51**: Bite, KO. | EXP 2218. |
| 1:47:40–1:47:54 | **Slowbro Lv.52**: Bite (critical hit), KO. | EXP 1827. |
| 1:47:55–1:48:09 | **Jynx Lv.54**: Surf, KO. A grows **Lv.52, 104/162**. | Safeguard ends. EXP 1584. |
| 1:48:14–1:48:29 | **Lapras Lv.54**: Bite; Lapras's **Body Slam** ×2 (104→71, **paralysis**; 71→33). Lapras's Sitrus Berry restores it. | Bite **21/25** before (1:48:16). |
| 1:48:34–1:48:39 | Use **Full Restore** (13→12): **33→162/162**, paralysis cured. | X Special **11** in the same bag view (one more X Special used earlier, unsampled). |
| 1:48:41–1:48:53 | Lapras's Body Slam (162→130); one Bite (critical hit) KOs. Lorelei defeated; win **5400**. | |
| 1:48:57–1:49:09 | Lorelei's closing text; walk to **Bruno**'s room; his dialogue. | |
| 1:49:15–1:49:33 | Bruno: **Onix Lv.51**. Use **X Accuracy**? (bag on X Accuracy 3 at 1:49:25) then **X Special** (11→10→9). Onix's **Rock Tomb** (130→118→114, Speed fell) and **Earthquake** (114→79→42). | Counts read at 1:49:34 (10) and 1:49:40 (9). Whether X Accuracy was used is not certain. |
| 1:49:45–1:49:55 | One **Surf** KOs Onix. | A stays **42/162**. |
| 1:49:56–1:49:59 | **Hitmonchan Lv.53** sent out; A uses Surf. | Outcome in the next block. |

**Boundary at 1:50:00:** Bruno's battle, Hitmonchan Lv.53, A's Surf in
progress. A **Lv.52, 42/162 HP**, two to three X Special boosts active.
Moves Surf / Strength / Bite / Blizzard. Full Restore 12, X Special 9,
X Speed 4, X Accuracy 3 (or 2), Guard Spec. 2, Elixir 1, Super Potion 4,
Parlyz Heal 2. Party: A only.

**Automation relevance:** the Plateau stop is heal, deposit everything except
the starter (so no other Pokémon can be sent in), and bulk Full Restores. The
Elite Four plan is X Special stacking on the first opponent, then one hit per
Pokémon, healing with Full Restore when HP or status demands it.

## 1:50:00–1:55:00 — Bruno, Agatha and Lance

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [1:50:00–1:50:07](https://www.youtube.com/watch?v=CbfjWuRWH10&t=6600) | The Surf from the last block KOs **Hitmonchan Lv.53**. | EXP 1590. A **42/162**. |
| 1:50:08–1:50:18 | **Hitmonlee Lv.53**: one Surf, KO. | |
| 1:50:20–1:50:34 | **Machamp Lv.56**: one Surf, KO. A grows **Lv.53, 45/165**. | |
| 1:50:35–1:50:49 | **Onix Lv.54**: one Surf, KO. Bruno defeated; win **5600**. | EXP 1249. No damage taken after the first Onix. |
| 1:50:51–1:51:08 | Walk to **Agatha**'s room; her dialogue. | No heal between Bruno and Agatha (A still 45/165). |
| 1:51:12–1:51:31 | Agatha: **Gengar Lv.54**. Use **X Speed** (3→2) and **X Special** (9→8, "Sp. Atk rose"). Gengar uses **Double Team** twice. | Counts read at 1:51:18 (X Speed 3, USE) and 1:51:25 (X Speed 2). |
| 1:51:33–1:51:39 | One **Surf** KOs Gengar. | EXP 2197. |
| 1:51:43–1:51:54 | **Golbat Lv.54**: one Surf, KO. | EXP 1978. |
| 1:51:55–1:52:14 | **Arbok Lv.56** (Intimidate): one Surf (critical), KO. A grows **Lv.54, 48/168**. | Stats at Lv.54: HP 168, Atk 114, Def 116, SpA 130, SpD 131, Spe 113. |
| 1:52:16–1:52:26 | **Gengar Lv.58**: one Surf, KO. | Surf **6/15** before use (1:52:17). EXP 2361. |
| 1:52:28–1:52:39 | **Haunter Lv.53**: one Surf, KO. Agatha defeated; win **5800**. | A **48/168**. |
| 1:52:42–1:52:57 | Walk to **Lance**'s room. | |
| 1:52:59–1:53:05 | Before talking to Lance: **Full Restore** (12→11) on A, **48→168/168**. | Bag at 1:52:59 shows X Speed 2, X Special 8, X Accuracy 3, Guard Spec. 2, Elixir 1; Full Restore 11 at 1:53:05. The "restored by 12…" text is cut off at 1 fps. |
| 1:53:07–1:53:23 | Lance's dialogue; battle starts. **Gyarados Lv.56** (Intimidate). | |
| 1:53:31–1:53:36 | A uses **Bite**; Gyarados uses **Dragon Rage** (168→128). | |
| 1:53:38–1:53:44 | Bag opened on **Guard Spec.** (2); not used. Gyarados's **Bite** (128→121→89, critical). | |
| 1:53:46–1:53:50 | Use **X Accuracy** (3→2). Dragon Rage (89→64→49). | Bag at 1:53:52 shows X Accuracy 2. |
| 1:53:54–1:53:56 | **Full Restore** (11→10): **49→168/168** ("restored by 119"). | |
| 1:53:58–1:54:01 | Gyarados's **Hyper Beam**: **168→78/168**. | |
| 1:54:02–1:54:14 | A uses **Blizzard** (Gyarados must recharge), then again; Gyarados KO. | EXP 2568. |
| 1:54:16–1:54:29 | **Dragonair Lv.54**: one Blizzard, KO. | EXP 1665. |
| 1:54:30–1:54:42 | Second **Dragonair Lv.54**: one Blizzard, KO. A grows **Lv.55, 81/171**. | |
| 1:54:43–1:54:50 | A tries to learn **Skull Bash**; answer **Yes** to "Stop learning Skull Bash?" | Moves unchanged. |
| 1:54:51–1:54:59 | **Dragonite Lv.60** sent out; A uses **Blizzard** (super effective). | Outcome in the next block. A **81/171**. |

**Boundary at 1:55:00:** Lance's battle, Dragonite Lv.60 just hit by Blizzard.
A Blastoise **Lv.55, 81/171 HP**, moves Surf / Strength / Bite / Blizzard,
X Accuracy boost active. Full Restore 10, X Speed 2, X Special 8,
X Accuracy 2, Guard Spec. 2, Elixir 1, Super Potion 4, Parlyz Heal 2.

**Automation relevance:** Blizzard's 5 PP are reserved for Lance's dragons,
with one **X Accuracy** right before them; earlier opponents use Surf or Bite.
Full Restore is used at a threshold (49 HP against Gyarados) and once before
Lance at 48 HP. Refusing an unwanted move must answer "Stop learning? Yes".

## 1:55:00–2:00:00 — Lance finish, Champion, Hall of Fame

| Video time | Observed action / outcome | Detail or qualification |
|---|---|---|
| [1:55:00–1:55:05](https://www.youtube.com/watch?v=CbfjWuRWH10&t=6900) | The Blizzard from the last block KOs **Dragonite Lv.60**. | |
| 1:55:06–1:55:19 | **Aerodactyl Lv.58**: its **AncientPower** takes A **81→74→37/171**; one **Surf** KOs it. | Surf **4/15** before use (1:55:08). |
| 1:55:20–1:55:29 | Lance defeated; win **6000**. Lance's closing text. | |
| 1:55:30–1:55:39 | **Full Restore** (10→9) on A: **37→171/171**. | Bag at 1:55:34. |
| 1:55:44–1:55:58 | Walk to the Champion room; rival's speech; battle starts. | No save. |
| 1:56:02–1:56:09 | **Champion**: **Pidgeot Lv.59**. | A **Lv.55, 171/171**. |
| 1:56:11–1:56:12 | Use **Guard Spec.** (2→1): "Ally became shrouded in Mist". | |
| 1:56:13–1:56:24 | Pidgeot's **Aerial Ace**: **171→128→84** (critical), **84→82**. | |
| 1:56:20–1:56:21 | Bag on **X Accuracy** (2); a use is not readable at 1 fps. | |
| 1:56:25–1:56:34 | Use **X Speed** (2→1) and **X Special** (7→6); Pidgeot's Sand-Attack blocked by Mist. A at **42/171**. | Counts at 1:56:27 and 1:56:33. |
| 1:56:35–1:56:37 | Pidgeot's **Aerial Ace**: **42→6→3/171**. | No heal: the run continues at **3 HP**. |
| 1:56:38–1:56:46 | One **Surf** KOs Pidgeot. | EXP 2173. |
| 1:56:49–1:56:54 | **Venusaur Lv.63**; Mist wears off. Use **X Special** (6→5) and **X Accuracy** (2→1? "A's accuracy rose!"). | Bag at 1:56:53 shows X Special 5, then 1:56:54 "A's accuracy rose!" with X Accuracy 2 listed before the use. |
| 1:56:57–1:57:09 | Venusaur takes in sunlight (SolarBeam); **Blizzard** (5/5 shown at 1:56:59) KOs it first. A grows **Lv.56, 6/174**. | Survives at 3 HP because Venusaur charges. |
| 1:57:15–1:57:27 | **Alakazam Lv.57**: one **Surf** (critical), KO. | Surf **12/15** (1:57:18): the Elixir was used at an unsampled moment between Lance and here. |
| 1:57:30–1:57:43 | **Rhydon Lv.59**: one Surf, KO. | |
| 1:57:44–1:57:56 | **Gyarados Lv.59** (Intimidate). **Full Restore** (9→8): **6→174/174** ("restored by 168"). | Bag at 1:57:51 shows X Accuracy **1**. |
| 1:57:58–1:58:11 | Gyarados's **Hydro Pump misses**. One **Blizzard** (critical) KOs it. | Bite 25/25 shown while choosing (1:58:01). |
| 1:58:14–1:58:31 | **Arcanine Lv.61** (Intimidate): **ExtremeSpeed** 174→155→131; one **Surf** KOs. A grows **Lv.57, 134/177**. | |
| 1:58:38–1:58:42 | Champion defeated ("My reign is over already?"). | |
| 1:58:43–1:59:20 | Rival and Oak's scene; walk with Oak to the **Hall of Fame** room. | Fixed scripted walk. |
| 1:59:22–1:59:27 | **SAVING… DON'T TURN OFF THE POWER** (automatic Hall of Fame save). | |
| 1:59:28–1:59:41 | Hall of Fame: **A / BLASTOISE, Lv.57, IDNo.39569**; "Welcome to the HALL OF FAME!". Trainer card: **TIME 1:54** (in-game clock). | The run timer stops at the Hall of Fame. |
| 1:59:43–2:00:00 | Credits begin outside the Indigo Plateau. | |

**Boundary at 2:00:00:** run finished; credits rolling. Final: A Blastoise
**Lv.57, 134/177 HP**, the only party member. Full Restore 8, X Special 5,
X Speed 1, X Accuracy ~1, Guard Spec. 1, Super Potion 4, Parlyz Heal 2,
Elixir 0 (used, unsampled).

## 2:00:00–2:03:57 — credits

Staff credits over map flyovers and Pokémon cameos (Venusaur, Blastoise),
then **THE END** at 2:03:52. No inputs matter. Sheets for 2:00:00–2:00:59,
2:01:30–2:01:59, 2:02:30–2:02:59 and 2:03:30–2:03:56 were inspected; the others
were skipped as credits only.

**Automation relevance (Elite Four and Champion, 1:45–2:00):**
- One Pokémon, deposited down to a party of one at the Plateau, so nothing
  else can be sent in; a fainted A is a loss.
- Healing is threshold-based with Full Restore (about 13 bought; 5 used), plus
  the one Elixir for PP. The run still stands at 3 HP against Venusaur:
  copying it needs a stricter heal rule than the speedrunner's.
- Stat items are spent on the first opponent of each Elite Four member
  (X Special/X Speed/X Accuracy, Guard Spec. against Pidgeot). Effects end on
  switch, which never happens with a party of one.
- Move choice is per opponent type: Bite for Lorelei's waters, Surf for
  Bruno/Agatha, Blizzard (5 PP, held back) for Lance's dragons and the
  Champion's Venusaur and Gyarados.

## AutoStory implications

These are proposals derived from the observations, not additional video facts:

- Add explicit short-name entry to the route profile if reproducing this run;
  do not silently change the MVP's no-nickname/default-name choices.
- Define a **level/move goal** for early training, such as obtaining Bubble,
  rather than assuming two encounters or these exact opponents always occur.
  This run fights Rattata and Pidgey on the parcel journey; our MVP initially
  flees Route 1 wilds and trains in the forest instead.
- Track move identities and reordering explicitly. Fixed slot assumptions
  become wrong at the observed Pidgey menu action, before Bubble is learned.
- A tutorial win is needed for this exact EXP route. Our MVP permits losing
  that fight; a loss needs an alternative training plan rather than continuing
  with assumed Lv.6/7 and Bubble.
- Observe script-provided HP/PP restoration rather than sending the player
  to an unnecessary nurse or carrying the pre-heal HP forward.
- Bind symbolic starter selection to the appropriate version/map script
  and verify the received species; do not derive tile coordinates from the
  scaled video image alone.
- Keep RNG setup as a separate researched route choice. The Trainer Card
  check is worth investigating, not enough to prescribe a seed manipulation.
- Add a separate **capture policy** between training and ordinary travel if
  following this route: one Bubble then a successful capture is observed, but
  an automation needs bounded outcomes for misses, knockouts, escapes and ball
  failures. A captured Pokemon is not a defeated one and grants no assumed EXP.
- Keep tutorial battle state separate from player party/inventory state.
  The old man's Potion, single ball and captured Weedle must not mutate the
  tracked player bag or party.
- After obtaining Bubble and the selected capture, this run flees all three
  forest wild encounters seen here. Separate training, capture and flee modes;
  do not infer that every forest encounter should be fought.
- Explicitly reconcile healing/status policy before the Sammy fight. This
  run begins it at 16/25 HP and reaches 9/25 while continuing to attack. That
  is an observed speedrun choice, not a recommended default safety margin.
- Use observed key-item receipts and completed scene boundaries for the
  Parcel/Pokedex and Teachy TV events. An entered lab or ended tutorial fade
  alone does not establish all rewards were received.

## Next-pass checklist

For each chunk, retain linked timestamps and explicit unknowns. At the end,
record party/moves/HP/level/items that are actually visible, the player's last
observed location, and the next in-progress action. When a battle crosses the
boundary, continue the same fight rather than summarizing it twice as two wins.
