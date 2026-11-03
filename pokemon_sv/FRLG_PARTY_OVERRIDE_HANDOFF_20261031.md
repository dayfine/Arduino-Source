# PR #61: redundant virtual keyword

User requested removing the confusing redundant `virtual` from
`RngHelper::free_party_slots_needed() const override`.

Ready for Claude to cherry-pick onto `frlg/party-room-check`:

- Commit: `27bdae40e` on local branch `frlg/party-override-clarity`.
- Parent: PR head `24113fb5f7056b30a0831395b6c7fa39112a4918`.
- Only the redundant `virtual` keyword is removed. `override` and dynamic
  dispatch remain; no target-dependent party-slot behavior changes.
- CRLF preserved. `git -c core.whitespace=cr-at-eol diff --check` passed.
- No rebuild: this is a declaration-only semantic no-op.
- Existing PR branch was not pushed, per ownership rules.

Also closed fork PR #65 at the user's request after verifying upstream
PokemonAutomation/Arduino-Source#1771 was merged (merge commit
`6fc9145b08f1982d2abf6f1baf7f328e06925198`).
