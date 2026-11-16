#!/bin/sh
# hold.sh: the user's hold on a lab console ("when I am using the console, don't touch it").
#
#   hold.sh on [REASON]   put the hold on by hand (reason hold_sh by default)
#   hold.sh off           take it off: removes Lab/hold, and ends the lab server's user lease
#   hold.sh status        print the holder, or "free"
#   hold.sh check         exit 0 when free; else say who holds it and exit 4
#
# The hold is the file Lab/hold in the console's profile ($LAB_PROFILE, else lab-B's):
# owner=, reason=, acquired_at= lines (app Lab.md "User hold", lab/server/pa_lab/hold.py).
# The app writes it when the user clicks Start or Stop or presses through its window; while it is on,
# the app's lab channel refuses presses and starts, and labctl.sh, labrun.sh, labB.sh and keepawake.sh
# refuse too. Nothing takes it off by itself: only "hold.sh off" or "pa_lab lease release-user".
# "off" also runs release-user when the lab server is configured ($PA_LAB_CONFIG, default
# ~/.config/pa-lab/server.json), with $PA_LAB_PYTHON and pa_lab from $PA_LAB_SERVER.
P=${LAB_PROFILE:-$HOME/Library/Application Support/SerialPrograms/Profiles/lab-B}
F=$P/Lab/hold
field(){ sed -n "s/^$1=//p" "$F" 2>/dev/null | tr -d '\r' | head -1; }
held(){ [ -n "$(field owner)" ]; }
describe(){ echo "held by $(field owner) since $(field acquired_at) ($(field reason))"; }

case $1 in
on)
    if held; then echo "already $(describe)"; exit 0; fi
    mkdir -p "$P/Lab" || exit 1
    printf 'owner=user\nreason=%s\nacquired_at=%s\n' "${2:-hold_sh}" "$(date -u +%Y-%m-%dT%H:%M:%S.000Z)" > "$F.part" \
        && mv "$F.part" "$F" || exit 1
    describe;;
off)
    if held; then was=$(describe); else was=; fi
    rm -f "$F"
    CFG=${PA_LAB_CONFIG:-$HOME/.config/pa-lab/server.json}
    if [ -f "$CFG" ]; then
        PY=${PA_LAB_PYTHON:-$HOME/.venvs/lab-claude/bin/python}
        [ -x "$PY" ] || PY=python3
        PYTHONPATH=${PA_LAB_SERVER:-$HOME/Projects/Arduino-Source-agents/lab/server} \
            "$PY" -m pa_lab lease --config "$CFG" release-user >/dev/null 2>&1 \
            || echo "hold.sh: the lab server's lease was not released (not held there, or pa_lab not found)" >&2
    fi
    echo "free${was:+ (was $was)}";;
status)
    if held; then describe; else echo free; fi;;
check)
    held || exit 0
    echo "REFUSED: console $(describe): the user is on the console; ~/BDSPLab/hold.sh off releases it" >&2
    exit 4;;
*)
    echo "usage: hold.sh on [REASON] | off | status | check" >&2; exit 2;;
esac
