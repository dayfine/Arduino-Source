# M0 local baseline — 2026-10-20

- Time zone: America/Los_Angeles (PDT).
- Start requested: 12:26:50; BBQSoloFarmer entered at 12:26:55.
- Local build: v0.71.2-user. Current process uses the default profile, not `bbq-dev`.
- Active log: `/Users/difan/Library/Application Support/SerialPrograms/SerialPrograms.log`.
- Camera Support was changed from On to Off and the game confirmed settings were changed before this run. Other required in-game options were visually checked.
- Existing quest exclusions retained: catch Dark, Poison, Fairy, Water, Ice, Bug, Steel; Terastallize to defeat a wild Pokémon. Each action is Reroll.
- Visible configuration before start: Traditional Chinese, 1,000 quests, save/reset every 50 attempts, normal flight, Quick Ball disabled, Ultra Ball, 30 eggs, reroll when out of eggs.
- Initial BP balance has not been recorded. Do not infer it from estimated quest rewards.
- Initial log progress: overworld detected; map and fly prompts detected normally.
- The previous development window disappeared during program switching around 12:24:47. The former profile log stops during console/video teardown. A new process was subsequently observed with the default profile; no SerialPrograms crash report was found at inspection. Cause remains unconfirmed.

Compare with the overnight run with care: application version and Camera Support differ. Leave settings unchanged during this baseline.

## First 37 minutes

At 13:03:45, five completions were logged. Successful task spans (dispatch to completion, including internal verification) were auto battle 7m05s, coastal/grass/dragon photo 1m23s, auto battle 3m40s, make TM 1m09s, and swimming/water/polar photo 1m13s. These are route-family labels, not necessarily exact quest slugs.

Three completed catch-any/normal/fire failure batches consumed 4m53s each (12:41:03–12:45:56, 12:47:39–12:52:33, 12:55:39–13:00:36), about 14m42s total, or 40% of elapsed run time. Each batch attempted twice and logged failure to enter battle on both attempts. A fourth batch began at 13:02:32 and logged another battle-entry failure at 13:03:45. Thus skipping after retries does not prevent the same outstanding quest from being selected again in later batches.

The swimming/water/polar photo additionally failed twice over 2m26s before succeeding in a later batch. Two configured reroll actions were logged. Completion count alone is insufficient for net BP accounting. This sample supports prioritizing persistent failure handling and the battle-entry timeout/recovery path; it does not establish that the catch route never works or that automatic rerolling is always profitable.
