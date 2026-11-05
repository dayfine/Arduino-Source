# Reviewed nav-core parent refresh

Pending explicit approval for one atomic parent-only push that bypasses inherited lint debt. No bypass has occurred.

| PR | Branch | Old head | Prepared head |
|---|---|---|---|
| #122 | `nav/story-steps` | `24444f0fc38c819440b1112fcafc292bce2e0b53` | `bb6db58990bd528ef9f0c54534ea3492dbbcf1e3` |
| #213 | `frlg/stage1-core` | `891b8f97e2e1399011431942c18079c532a19b5d` | `482067e9260d33b88c73405822bccfc8b753ac47` |
| #214 | `frlg/stage2-rng` | `326daebe7758771293904a259a18217b57559341` | `6096678ac2e425f8aa1eaa6d03e8a903ed48e529` |
| #215 | `frlg/stage3-mtmoon` | `5b9b65a752dc124403501f30cf281d4c7f9cd29c` | `e36b1b50cc86215c12f7c53c641ff54919b5b5b3` |
| #216 | `frlg/stage4-cerulean` | `831a22c423fc0ab1918fba85fb667d18c31c1691` | `4e3cf21ebd0c94f71196c7468d0606cf773ef5b5` |
| #219 | `frlg/stage5-bill` | `360701b053f294cae5e652e2bcfb60fd25608306` | `7b6a28cebae30896aa10d00378d84ebba6322d6b` |
| #222 | `frlg/stage6-vermilion` | `b48afbc830f4b4c34cf94eb1aba85c2654a69938` | `130344db1cb2a394693019d734145f8c295c589b` |
| #224 | `frlg/stage7-celadon` | `51c0ce6128e5981da56b370a770874b96a1e129f` | `cc43650ef87c91aa4a6ac049c37114cfddf82d36` |
| #134 | `bdsp/maps-v4` | `20b518e5a23388683c151ba32e3acd5623d9994b` | `7e1bc25ebb37c16870937026e1b099e7c189c4a5` |
| #127 | `bdsp/nav-v4` | `e531694c589464510d7720f4b571073aade2c118` | `8805523710a2ed6298583cc53652316116f0fc6e` |
| #129 | `bdsp/walk-v4` | `b22e9471c35fb171daada221b6d467c1dc043387` | `89f673f74988cea2333e7dd157136f3aa5f39f0e` |
| #152 | `bdsp/detectors-v4` | `139d483ba9c1fae7383aa97e5ca9bb0b5536ebc0` | `f5a3c42abf063fcc366f024a9a7986a9c97dd836` |
| #148 | `bdsp/actions-v4` | `76e3649f56a9432cf1cc137db19045956bb0c570` | `101c5b8172d5df14f8d4f029e64150b4c448c789` |
| #128 | `bdsp/autostory-v4` | `040693c62486439b4b473732860b0d4c6632dc2a` | `7f16d8cab644e842a2d30a9adf89c56da7615a33` |
| #167 | `bdsp/sim` | `df6b1e47472f2448861eb72f4c432ead1d7c408c` | `b6f2f4d1ba993670dafc1daa06fdc83b3744032b` |
| #226 | `bdsp/autostory-mine-floaroma-wip` | `f8bdd62b9df4dbf285f93853147295e0552a0367` | `e65fd909297b58068e2dd92e534e969b2acc086d` |

All 16 commits preserve the old head as an ancestor. Every old-to-new diff contains only the same six reviewed nav-core files from #227. Shared header/test bytes exactly match nav/shared @ 546b3c0d1d44f361bc29e6cfa1ad2e12d5cd3de2. Each incremental lint run passes.

Staged #224 base makes #228 a one-file FRLG adapter diff. Staged #226 base removes shared nav-core files from #229. The child source trees are unchanged, preserving their 58/50 passing native cases.

The ordinary #122 push was rejected for 23 existing style findings in its AutoStory files. tools/hooks/pre-push explicitly requires user OK for --no-verify; the previous bootstrap approval does not apply. Publishing has not bypassed the hook.

Proposed action: one atomic fast-forward-only push for these 16 exact refs, with a preflight check that every remote still equals its recorded old SHA. No rebases, force pushes, PR merges, active worktree edits or runtime changes.

Implementation: /tmp/publish-nav-parent-forward.py. It requires --approved-parent-only-refresh and must only be run after explicit approval in this session.

Original #122 was checked with the same canonical linter against its original parent: the same 23 findings existed before this refresh. The new shared core also passed full Windows Clang (42093374031) and macOS (42093379011) CI.

Child branches have been pushed normally with ancestry-only merges (identical source trees): #228 `01cd46204bd5646b3cf341e7c9d43c02b4e6de6c`, #229 `e35a3a5a7d1299b3eac844b0facfd50ee92f5a56`. Base refs are still unchanged pending approval.

Approval requirement: [tools/hooks/pre-push](https://github.com/dayfine/Arduino-Source/blob/agents/tools/hooks/pre-push) says “Bypass (git push --no-verify) only with the user’s OK for that push.” The existing bootstrap approval was already consumed and does not cover this batch.


## Published after explicit approval

The user approved this exact batch. All 16 parent refs were updated atomically, with no force pushes or PR merges. #224 now points at `cc43650ef`, #226 at `e65fd9092`. GitHub confirms #228 contains only `PokemonFRLG_OverworldReanchor.cpp`, and #229 no longer contains shared nav-core files. The one-time bypass approval was consumed.

The remaining legacy lint work is inventoried in [the parent-PR lint handoff](parent-pr-lint-handoff-2026-11-05.md).
