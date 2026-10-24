# Kitakami video review: stationary funnels and bounded movement

Incorporated 2026-10-24 PDT. Source: Austin John Plays,
[26 EASY Shiny Hunts in Teal Mask](https://www.youtube.com/watch?v=TVlLDhGyJ-U),
published 2023-09-19, duration 23:36. Read the complete English auto-caption
transcript and inspected 16 timestamped frames, including detailed views of
the stationary cave examples. This was not continuous real-time playback or
a live verification on our Switch/current game version.

## What changes in our plan

Add a **stationary spawn-funnel** strategy alongside boundary shuttles, riding
loops and picnic resets. A useful spot concentrates reachable spawns ahead of
the lead while the player stays at an anchor. Clearing can then permit fresh
spawns without a travel/reset cycle. That is a hypothesis to measure at each
anchor, not a property of all caves or a guarantee of continuous clearing.

The existing prototype already supplies the short clearing/acquisition probe:
prepared spot, kill counts, refusal detection, bounded ZR attempt and shared
battle handling. **Sentret's cave is the strongest new comparison for that
mechanism.** Chespin and Charcadet remain the first user-priority targets;
Sentret becomes an optional prepared-spot control before expanding profiles.
No route or new program behavior was implemented during this review.

## Most useful demonstrations

| Target | Timestamp | Method to inspect | Main field question |
| --- | --- | --- | --- |
| Sentret family | [16:31](https://www.youtube.com/watch?v=TVlLDhGyJ-U&t=991s) | Wall position in the cave below/west of Kitakami Hall, marked by Tera Sandslash; spawns gather opposite | Does clearing replenish reachable targets from this exact wall/camera anchor? |
| Cleffa/Clefairy | [6:36](https://www.youtube.com/watch?v=TVlLDhGyJ-U&t=396s) | Stand at an Oni Mountain tunnel endpoint, facing inward | Can our lead cover the distant patch? Carbink means this is a mixed pool. |
| Ekans | [20:05](https://www.youtube.com/watch?v=TVlLDhGyJ-U&t=1205s) | Reuse the tunnel geometry with Poison power | Same movement strategy can serve a different pool; Salandit/Glimmet remain competitors. |
| Mienfoo | [4:13](https://www.youtube.com/watch?v=TVlLDhGyJ-U&t=253s) | Chilling Waterhead floor-to-floor circuit | Attractive local pool, but drops, multiple levels and post-battle recovery make navigation harder. |
| Carbink | [7:54](https://www.youtube.com/watch?v=TVlLDhGyJ-U&t=474s) | Side chamber; picnic reset and prompt edge positioning | Lower floors consume spawn capacity. Count reachable targets rather than every visible spawn. |

The creator reports 11 of 15 spawns in the desired Carbink chamber in one
sequence (about 73%). This is one creator observation, not a measured target
rate for our bot. Reported minutes to individual shinies elsewhere in the
video are anecdotes and do not establish expected shiny/hour performance.

Visual evidence: [timestamped contact sheet](kitakami-contact.jpg). It shows
the two tunnel viewpoints, the Sentret corner, the multi-level Mienfoo route,
and selected map/terrain frames. Exact fly-point distance, approach time,
slope and return-to-anchor reliability remain unmeasured.

## Candidate catalog and source cautions

[KITAKAMI_VIDEO_CANDIDATES.csv](KITAKAMI_VIDEO_CANDIDATES.csv) records 26
headline target profiles with timestamps, sandwich type, locality, movement,
version, pool/condition caveats and the field question. This is a scouting
catalog, not 26 independently verified exclusive pools or 26 new evolution
families added to our coverage totals. Related stages are retained together
where relevant; incidental species are recorded as competition.

- **Mixed pools are still potentially useful.** The Sandshrew, Wooper, Phantump,
  Koffing, Ekans and Sudowoodo examples include other families. Score target
  yield and acquisition success rather than relabeling these as pure isolation.
- **Time belongs in the spot profile.** The Fire examples change with Litwick
  availability; the Ghost examples also have time conditions. A changing pool
  can invalidate a profile during one sandwich. Verify the exact time windows.
- **Terrain layers matter.** Water/land boundaries and floors below a cave can
  consume the encounter budget. Feebas is a visibility/crowding trick, not
  demonstrated type-exclusive isolation. Aquatic and airborne targets still
  need an acquisition strategy the lead can execute.
- **Forms and versions matter.** Record Johtonian Wooper, not the Paldean form.
  Morpeko is the Violet example; Gligar is Scarlet. Lotad is available in both
  games' Kitakami expansion, per its [game locations](https://bulbapedia.bulbagarden.net/wiki/Lotad_(Pok%C3%A9mon)#Game_locations).
- **Do not copy the Flying explanation literally.** The narration mentions
  Volbeat/Illumise as later competitors. Volbeat is [Bug, not Flying](https://bulbapedia.bulbagarden.net/wiki/Volbeat_(Pok%C3%A9mon)).
  Verify the actual night pool and sandwich behavior before implementing a
  time restriction based on this explanation. Visual presence alone does not
  identify which encounter rule produced a spawn.
- **Hoothoot is excluded from the implementation queue.** At
  [15:38](https://www.youtube.com/watch?v=TVlLDhGyJ-U&t=938s), the creator proposes
  a daytime shiny-only spawn idea but reports no success in roughly 15 minutes.
  Treat this as an unverified hypothesis, not a working exploit or a hunt to
  automate from this source.
- **Do not generalize the pack-spawn discussion.** The introductory comparison
  suggests group availability differs from Paldea. We need local data and
  observed group sizes; do not encode one blanket Kitakami pack multiplier.
- **Historical event references are not prerequisites.** The September 2023
  Clefairy outbreak promotion mentioned in the video is historical. An outbreak
  and ordinary sandwich hunting are separate configurations in our catalog.

## Next experiment and acceptance

1. Keep the Chespin/Charcadet priority. If testing the stationary mechanism in
   Kitakami, manually reach and photograph Sentret's exact wall anchor first.
2. Record five approaches and at least ten observed spawn/clear cycles, under
   documented sandwich, time, version, lead, camera and outbreak conditions.
3. Measure newly reachable targets, kills, battle entries, refusal events,
   acquisition timeouts and time without progress. Check that cleared slots
   replenish; a still frame or a kill sound alone cannot establish this.
4. Inspect any refusal before refreshing the room. Distinguish shiny protection,
   health/type reluctance and pathfinding failure through battle confirmation
   and observations, not the sweat icon alone.
5. Compare stationary clearing against one bounded local refresh at the same
   anchor. Only add a movement profile after its distance, reset and recovery
   have been demonstrated repeatedly.
6. Stop after the short probe and inspect positioning after any battle. A
   stationary player does not guarantee that the lead or camera stays anchored.
   Proceed to sustained validation only after acquisition and recovery work.

Kitakami remains separate from the Paldea mining and the 24-starter survey.
There is no combined regional/family total yet. Raw captions, video and frames
stay in `/tmp/sv-video-TVlLDhGyJ-U`; the repository retains the derived catalog,
contact sheet and [source manifest](kitakami_video_manifest.json).
