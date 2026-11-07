# Uncrustify configuration and main-tree trial

User authorized revising fork #244 to Uncrustify and running formatting against
main. Tooling changes are in `tooling/clang-format-config`, commit
`c0f34a31e50947e4d68485702c76db5906db273a`. No bulk C++ formatting is included
in the PR. The old `.clang-format` proposal was removed.

## Inputs and scope

- Main snapshot: `2a692970b6ac4480f2af405d0adc964192df04e0`.
- Uncrustify: `0.83.0_f` locally; CI builds release 0.83.0 from the verified
  upstream archive, SHA-256
  `1d4d3ab3a991c66eb24cb0a3e690573b4882fd98bc6ec64a01c283998c20dc87`.
- Final `.uncrustify.cfg` SHA-256:
  `0abf6bea5ca3b316c62965096663d692d3cebd02a2cc556f91d366dd86762f1f`.
- Inventory: all 3,169 Git-tracked `.h`, `.c`, `.cpp`, `.tpp` files under Common
  and SerialPrograms, excluding outside third-party trees and build outputs.
  No generated-file markers were found in that inventory.
- Isolated detached preview: `~/Projects/wt/main-format-preview`. Main and all
  user/other-agent source worktrees were left unchanged by this trial.

## Final results

| Check | Result |
|---|---|
| Files visited | 3,169 |
| Files changed | 2,839 |
| Diff size | 54,021 insertions / 49,302 deletions |
| Formatter errors or stderr diagnostics | 0 |
| Non-idempotent outputs on second pass | 0 |
| Include target/order changes | 0 |
| LF/CRLF changes | 0 |
| Changes after removing ASCII whitespace | 0 |
| Significant Clang raw-token comparisons | 2,839 equal / 0 different |

The Clang raw lexer compares token kind/spelling, excluding comments and
whitespace, without preprocessing includes or compiling the app. This catches
identifier/operator/literal token changes but is not a full native build or a
proof of macro/preprocessor equivalence. The bulk patch remains an uncommitted
preview, not an approved formatting migration. No app, console or save actions.

## What the trial caught

- An initial `sp_after_type=remove` setting merged conversion operators such as
  `operator const Type` into `operator constType`. Rejected it; final setting is
  `ignore`, with a dedicated regression. Do not reinstate the aggressive setting
  to obtain compact standalone class braces.
- Default nested-template spacing split `>>`. Final config preserves the
  original spelling; the lexical check now passes for every changed file.
- Attaching function braces across commented constructor initializers moved
  braces relative to comments. Preserve function-definition brace line breaks;
  regression checks constructor-comment ordering.
- Inherited class/struct declarations can still gain a space before `{`.
  Standalone class spacing is preserved. This limitation is explicit in the
  README and tests; the formatter is not a complete style-compliance validator.

Control and namespace braces produce `if (condition){`, `}else{` and
`namespace Name{` directly. No regex compatibility pass is used. Includes,
comments and source line-ending style are preserved by the conservative config.

## Tests and review

12 local configuration tests pass, including exact brace spelling, macros,
raw strings, templates/designated initializers, conversion operators, comments,
includes, line endings and idempotence. CI builds the checksum-pinned tool and
runs these same tests:
https://github.com/dayfine/Arduino-Source/actions/runs/42346765877 — passed.
Code Health 42346765871 and tabs 42346765870 also passed. Full native builds
were not required for this tooling-only PR; the formatted app tree is unbuilt.

Review the bulk output locally:

```sh
git -C ~/Projects/wt/main-format-preview diff --stat
git -C ~/Projects/wt/main-format-preview diff -- Common/Cpp/Containers/Pimpl.h
```

Artifacts (local, outside tracked source):

- `/tmp/main-uncrustify-preview.patch` — complete preview, SHA-256
  `2558db3096469a952dfa313cabc665a90c59f94344a90adc977377c66beb3d27`.
- `/tmp/main-uncrustify-audit.json` — per-file formatter checks.
- `/tmp/main-uncrustify-token-audit.json` — final lexical comparisons.
- `/tmp/main-uncrustify-numstat.tsv` — per-file diff counts.
- `/tmp/audit-main-format.py` and `/tmp/token-audit-main-format.py` — trial
  commands, using the isolated worktree and exact PR config, four workers.

Do not bulk-apply this preview by merging the tools PR. If a broad formatting
migration is desired, review it separately and run native compilation/tests on
that exact formatted revision. Current PR enables no source-wide format gate.
