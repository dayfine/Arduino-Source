# Issue tracker: GitHub

Issues and PRDs for this repo live as GitHub issues on the fork, `dayfine/Arduino-Source`. Use the `gh` CLI (`~/.local/bin/gh`, first on PATH) for all operations.

**Never file, comment on or edit issues on `PokemonAutomation/Arduino-Source`.** Anything meant for upstream goes to the user.

## Conventions

Every `gh` command carries `-R dayfine/Arduino-Source`. This clone has an `upstream` remote, so `gh`'s repo inference is unsafe.

- **Create an issue**: `gh issue create -R dayfine/Arduino-Source --title "..." --body "..."`. Use a heredoc for multi-line bodies.
- **Read an issue**: `gh issue view <number> -R dayfine/Arduino-Source --comments`, filtering comments by `jq` and also fetching labels.
- **List issues**: `gh issue list -R dayfine/Arduino-Source --state open --json number,title,body,labels,comments --jq '[.[] | {number, title, body, labels: [.labels[].name], comments: [.comments[].body]}]'` with appropriate `--label` and `--state` filters.
- **Comment on an issue**: `gh issue comment <number> -R dayfine/Arduino-Source --body "..."`
- **Apply / remove labels**: `gh issue edit <number> -R dayfine/Arduino-Source --add-label "..."` / `--remove-label "..."`
- **Close**: `gh issue close <number> -R dayfine/Arduino-Source --comment "..."`

## Area labels

Apply one or more alongside the triage label (see `triage-labels.md`). All exist on the fork.

- `game:bdsp`, `game:frlg`, `game:sv`, `game:swsh`, `game:shared`
- `autostory`, `navigation`, `rng`, `detector`
- `lab-tooling`, `refactor`

## When a skill says "publish to the issue tracker"

Create a GitHub issue on `dayfine/Arduino-Source`.

## When a skill says "fetch the relevant ticket"

Run `gh issue view <number> -R dayfine/Arduino-Source --comments`.
