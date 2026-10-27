**Title:** BBQ Farmer: run unattended for hours (status and PR plan)

---

**What this is.** On `main`, BBQ Farmer can stall for hours on some quest boards and spends much of its time on quests it can't finish. I've been fixing that on a Switch 1 (Traditional Chinese): run the farmer for 2 hours, look at what failed, fix it, repeat. My test build now runs 2-hour sessions without stalls at about 550 net BP per hour, where `main` made about 400 and then stalled.

**What changes, in plain terms**
- **It doesn't get stuck.** A quest that keeps failing is rerolled once its retries run out, instead of being retried every round forever. Quests the farmer can't do are rerolled as soon as they appear.
- **It checks where it landed.** After each flight it reads the place name and flies again if it's in the wrong spot.
- **It finds its target.** For catches it locks on to a nearby Pokémon (ZL) instead of throwing blind, and for photos it turns the camera toward the target first.
- **It picks better quests.** Quests are ordered and rerolled by their measured BP per minute.
- **Auto Battle quests are shorter.** It stops at the kill target and heals only when HP is low.

**How it will arrive.** About 60 small PRs, each one change with unit tests, in this order:
1. **Small, independent pieces:** a crash fix in error reporting, splitting the test file, new detectors with their test images in CommandLineTests.
2. **Shared routines other programs can use:** lock-on, leaving a battle, Terarium flights. These get extra testing because other programs call them.
3. **The farmer's own logic:** quest choice, retries, Auto Battle. Most of these are already written as PRs on my fork.
4. **Routes for individual quests,** each tested on the Switch.
5. **The program's docs page,** listing the known gaps below.

I keep at most two PRs open here and open the next one when one merges. Behavior changes get their own PR and say so in the title. The full plan, with sizes and the risk of each PR, is [here](https://github.com/dayfine/Arduino-Source/blob/notes/pr-plan/pokemon_sv/PR_PLAN.md).

**Where your input helps most**
- Where shared routines should live, for example a new `PokemonSV_OverworldControls` file for lock-on and riding.
- Whether a species → type table belongs in Resources, like Max Lair's `rental_pokemon.json`.

**Known gaps:** photographing flying Pokémon is unreliable; rain shortens glides; lock-on takes the nearest Pokémon even when it's the wrong species.

<details><summary>Original report (2026-10-20)</summary>

(original body, unchanged)

</details>
