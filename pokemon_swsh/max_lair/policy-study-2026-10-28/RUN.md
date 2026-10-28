# Reproducing the Strong Boss policy study

Run from a checkout of the fork's `agents` notes branch. This is offline native
C++ simulation, not an application test or a live Switch run. Python only
prepares historical observations and summarizes the native CSV output.

Use a separate code checkout at PR #55 head
`4bf135d85fa001071dbc9b5550d0a08bb28c5c08`. The simulator links its real
`PokemonSwSh_MaxLair_PathPolicy.cpp` and the shared GUI-free CoreLib. It does
not copy the policy or require a GUI build, game resources, or hardware.

```sh
MAX_LAIR_CODE=/absolute/path/to/code-checkout
MAX_LAIR_CORE=/tmp/max-lair-study-core
MAX_LAIR_OUT=pokemon_swsh/max_lair/policy-study-2026-10-28

cmake -S "$MAX_LAIR_CODE/SerialPrograms" -B "$MAX_LAIR_CORE" -G Ninja \
  -DPA_CORE_ONLY=ON -DCOMMAND_LINE_EXE=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build "$MAX_LAIR_CORE" --target CoreLib -j4

c++ -std=c++23 -O2 -Wall -Wextra -Werror \
  -I "$MAX_LAIR_CODE" -I "$MAX_LAIR_CODE/SerialPrograms/Source" \
  -isystem "$MAX_LAIR_CODE/3rdParty-Core" \
  tools/max_lair_policy_simulator.cpp \
  "$MAX_LAIR_CODE/SerialPrograms/Source/VideoGames/PokemonSwSh/MaxLair/Framework/PokemonSwSh_MaxLair_PathPolicy.cpp" \
  "$MAX_LAIR_CORE/libCoreLib.a" -o /tmp/max-lair-policy-simulator

/tmp/max-lair-policy-simulator --selftest
/tmp/max-lair-policy-simulator "$MAX_LAIR_OUT/models.tsv" \
  "$MAX_LAIR_OUT/results.csv" "$MAX_LAIR_OUT/comparisons.csv" 500 2000
python3 tools/summarize_max_lair_policy_study.py "$MAX_LAIR_OUT"
```

The checked-in `models.tsv` is sufficient to reproduce the numerical experiment;
it includes all empirical duration samples. To regenerate that input from the
local historical archive instead:

```sh
python3 tools/prepare_max_lair_policy_study.py \
  --history pokemon_swsh/max_lair/history-2026-10-28 \
  --out pokemon_swsh/max_lair/policy-study-2026-10-28
```

Training seeds are 1,000,000 through 1,000,499; held-out seeds are 9,000,000
through 9,001,999. Every scenario/policy uses the same potential route sequence,
within-route battle outcomes and timing draws for a seed. Different policies
can abandon a route at different points. They cannot return to an abandoned
route or see its true quality. Partial attempts at the time limit do not count
as wins. Candidate thresholds are selected using training results only.

The `999-ore-no-replenishment` variant assumes a mature 10-ore fee per reset,
forces continue when fewer than 10 ore remain, and does not credit ore earned
from completed adventures. It is deliberately a simplified budget stress case.
The simulation does not claim to reproduce the full ore economy.

Checks and compiler: `native-checks.txt` records six deterministic native
simulation checks. Built with Apple Clang 21.0.0, C++23, `-O2 -Wall -Wextra
-Werror`. The production policy file and header were verified unchanged in the
code checkout. No app was launched or Switch input sent.
