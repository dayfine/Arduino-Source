# Platinum decompilation and our BDSP AutoStory MVP

Saved from the discussion on 2026-10-31.

I checked [BDSP MVP #68](https://github.com/dayfine/Arduino-Source/pull/68), the [navigation design](https://github.com/dayfine/Arduino-Source/blob/design/autostory/docs/design/AutoStory-Navigation-Stack.md), [event-model design #78](https://github.com/dayfine/Arduino-Source/pull/78), and newer `bdsp/grid-sim` lab code.

**The biggest additional value is making interactions and navigation predictable.** We already know the story order; the decomp helps specify exactly what each step requires and changes.

**Map and NPC data**

Yes, NPC placement is a substantial gain, and we’ve already started: the newer Sandgem grid uses Platinum’s collision data, signs, and doors. That corrected errors introduced by tracing a map image.

The event files also supply initial positions, facing, movement types, wandering ranges, visibility flags, and script references. That lets us distinguish a permanent wall from a person who might move. However, “placement for free” means **initial placement plus movement rules**—a wanderer’s current position still needs observation. Scripts can also reposition objects when entering a map. [Route 202 events](https://github.com/pret/pokeplatinum/blob/main/res/field/events/events_route_202.json), [Jubilife scripts](https://github.com/pret/pokeplatinum/blob/main/res/field/scripts/scripts_jubilife_city.s).

There’s already a useful discrepancy: #68 models four Route 202 trainers, while Platinum defines three trainers and a separate wandering youngster. We should resolve that against BDSP before importing everything.

**Events and story flags**

I agree their value for discovering the overall story is modest. Their value for **execution, verification, and recovery** is much higher:

| What scripts reveal | Where it helps our design |
|---|---|
| Exact trigger rectangles and activation conditions | Plan into Barry’s encounter deliberately instead of searching neighboring rows |
| NPC visibility and relocation conditions | Apply the correct obstacles for the current story stage |
| Player/NPC movements during scenes | Update the pose after a scene rather than continuing from the old position |
| Items, flags, or world changes produced | Give each event a meaningful `expect` check |
| Already-completed branches | Design safe `skip_if` checks and checkpoint resumption |

For example, Platinum’s Route 203 encounter uses a **one-by-four trigger rectangle**, conditioned on the rival-state variable, with different movement sequences depending on the player’s row. Knowing “Barry battles here” doesn’t capture those navigation details. [Events](https://github.com/pret/pokeplatinum/blob/main/res/field/events/events_route_203.json), [script](https://github.com/pret/pokeplatinum/blob/main/res/field/scripts/scripts_route_203.s).

Importantly, the game’s internal flags aren’t observable through our controller/video interface. We’d use them to understand dependencies, then verify visible effects. That fits our existing `require → act → expect` model and its rule that run flags come from confirmed effects.

A concrete weakness in the newer lab code: some clown events set `coupon_1`/`coupon_3` when a dialogue opened and closed. **That doesn’t prove we received the coupon.** Script knowledge can identify the actual success branch; our observer should confirm the item or a distinctive completion message.

**Other useful material, ranked for our MVP**

1. **Interaction identities and dialogue branches.** Distinguish a sign, rock, ordinary NPC, and intended story interaction. This directly addresses our Roark-versus-rock failures.
2. **Warp destinations, arrival positions, and elevation.** Improve door/cave transitions and prevent treating visually adjacent paths on different levels as connected. Route 203’s event data includes elevation and its Gate warp.
3. **Scene side effects.** Roark’s Platinum script removes the smashed rock and Roark, then sets his gym-return flag. That suggests concrete world updates and completion checks. [Roark script](https://github.com/pret/pokeplatinum/blob/main/res/field/scripts/scripts_oreburgh_mine_b2f.s).
4. **Trainer teams, encounters, and item rewards.** Useful inputs for training, healing, and resource planning—but these need BDSP-specific validation. [Trainer data](https://github.com/pret/pokeplatinum/blob/main/res/trainers/data/leader_roark.json).

I’d start with **Jubilife’s president and clowns**: combine their placement/movement data with script-derived success conditions, then validate them in BDSP using our existing event runner. That tests both major benefits on a current trouble spot. Keep the shared runner unchanged; add per-game data and observers. Platinum should supply candidate facts, with verified BDSP differences recorded alongside them.
