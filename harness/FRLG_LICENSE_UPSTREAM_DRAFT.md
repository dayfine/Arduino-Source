# FRLG RNG: optionally wait for online-license startup

Related: PokemonAutomation/Arduino-Source#1788.

FRLG RNG resets can press HOME before an online-license launch finishes, then retry and repeatedly increase the launch delay. Add a “Handle Online License Check” checkbox in FRLG Game Settings. It is off by default; enabling it waits for the game's startup black screen before pressing HOME.

## How to review

Read the private launch helper and its three call sites. Settings registration moves unchanged into a private method to keep the constructor within its code-health baseline.

## Validation

Mac, Windows clang and Ubuntu build/native tests passed on fork head `6b4ba3706`; code health and whitespace checks passed. The simplified path has not been rerun on hardware.

---

Publication handoff (remove this section from the upstream PR body):

- Fork draft: https://github.com/dayfine/Arduino-Source/pull/59
- Clean branch: `dayfine:upstream/frlg-online-license-optional`
- Clean commit: `bf995917c`, based on upstream `cc89d7a8e`.
- Exactly three FRLG files, +57/−14, byte-identical to the tested fork head.
- Clean-branch CI dispatched:
  - Mac: https://github.com/dayfine/Arduino-Source/actions/runs/41244809495
  - Windows clang: https://github.com/dayfine/Arduino-Source/actions/runs/41244811671
  - Ubuntu: https://github.com/dayfine/Arduino-Source/actions/runs/41244813680
- Pending clean-branch CI completion and a slot under upstream's two-open-PR
  limit, drafts included. At preparation time #1754, #1759 and #1767 were open.
- User requested reference #1778, but that is an unrelated merged audio PR;
  the matching license bug is issue #1788. Corrected in the fork draft and here.
- No upstream PR opened. Do not cherry-pick the original fork ancestry;
  it contains fork-only tooling/design commits. Use the clean branch.

## Publication update

The user explicitly instructed “upstream it -- as one commit / clean PR”.
Published upstream draft https://github.com/PokemonAutomation/Arduino-Source/pull/1791
using the wrapper's authorized-user override. Verified one commit `bf995917c`,
three files, +57/−14, draft state. References issue #1788. The user had already
been informed that the upstream account had three open PRs before directing
publication. Clean-branch CI remained running at publication; hardware
validation of the simplified path remains outstanding and is stated in the PR.
