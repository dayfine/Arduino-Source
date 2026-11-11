"""Shared Claude client setup for the lab scripts (triage_failure.py, watchdog_shot.py).

Uses the official `anthropic` SDK. Credentials: zero-arg Anthropic() reads ANTHROPIC_API_KEY or the
`ant auth login` profile. The SDK is imported lazily so --dry-run and the tests run without it.
"""

import base64
import datetime
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

FALLBACK_BETA = "server-side-fallback-2026-07-01"
MAX_SIDE = 640


# Budget: the lab pays from the Console org's API credit, $120/month (user, 2026-11-11), paced
# per day. Every call is priced from response.usage and appended to a local ledger; a call is
# refused once today's or this month's spend reaches its cap. Override with the env vars.
MONTHLY_USD = float(os.environ.get("LAB_CLAUDE_MONTHLY_USD", "120"))
DAILY_USD = float(os.environ.get("LAB_CLAUDE_DAILY_USD", "4"))
LEDGER = Path(os.environ.get("LAB_CLAUDE_LEDGER", str(Path.home() / "BDSPLab" / "claude-spend.jsonl")))

# USD per million tokens: (input, output, cache read). Unknown models price as Opus 5.5.
PRICES = {
    "claude-opus-5-5": (4.00, 20.00, 0.20),
    "claude-sonnet-5-5": (2.00, 10.00, 0.20),
    "claude-haiku-5-5": (0.10, 0.50, 0.01),
}


class BudgetExceeded(RuntimeError):
    pass


def cost_usd(model, usage, batch=False):
    inp, out, cache = PRICES.get(model, PRICES["claude-opus-5-5"])
    cost = (getattr(usage, "input_tokens", 0) * inp
            + (getattr(usage, "cache_creation_input_tokens", 0) or 0) * inp * 1.25
            + (getattr(usage, "cache_read_input_tokens", 0) or 0) * cache
            + getattr(usage, "output_tokens", 0) * out) / 1e6
    return cost / 2 if batch else cost


def spent(ledger=None, today=None):
    """(today's USD, this month's USD) from the ledger."""
    ledger = Path(ledger or LEDGER)
    today = today or datetime.date.today()
    day = month = 0.0
    if ledger.exists():
        for line in ledger.read_text().splitlines():
            entry = json.loads(line)
            date = datetime.date.fromisoformat(entry["date"])
            if (date.year, date.month) == (today.year, today.month):
                month += entry["usd"]
                if date == today:
                    day += entry["usd"]
    return day, month


def check_budget(ledger=None, today=None):
    day, month = spent(ledger, today)
    if month >= MONTHLY_USD:
        raise BudgetExceeded(f"monthly Claude budget used: ${month:.2f} of ${MONTHLY_USD:.2f}")
    if day >= DAILY_USD:
        raise BudgetExceeded(f"daily Claude budget used: ${day:.2f} of ${DAILY_USD:.2f}")


def record_spend(model, usage, script, batch=False, ledger=None, today=None):
    ledger = Path(ledger or LEDGER)
    ledger.parent.mkdir(parents=True, exist_ok=True)
    entry = {"date": (today or datetime.date.today()).isoformat(), "script": script,
             "model": model, "usd": round(cost_usd(model, usage, batch), 6)}
    with ledger.open("a") as f:
        f.write(json.dumps(entry) + "\n")
    return entry["usd"]


# The lab's own API key, from a key made in the Console's `pa-lab` workspace, so lab spend is
# tracked and capped apart from other usage. Read from a file (not ANTHROPIC_API_KEY) so the key
# never shadows the user's `ant` login elsewhere. No fallback to other credentials.
KEY_FILE = Path(os.environ.get("LAB_CLAUDE_KEY_FILE",
                               str(Path.home() / ".config" / "pokemonautomation" / "claude-lab.key")))


def make_client():
    import anthropic  # lazy: not needed for --dry-run
    if not KEY_FILE.exists():
        raise RuntimeError(f"no lab API key at {KEY_FILE}; see lab/scripts/README.md (One-time setup)")
    return anthropic.Anthropic(api_key=KEY_FILE.read_text().strip())


def downscale_png(path, max_side=MAX_SIDE):
    """PNG bytes no larger than max_side px on the long side (Pillow, else sips, else as is)."""
    path = Path(path)
    try:
        from PIL import Image
        with Image.open(path) as image:
            image.thumbnail((max_side, max_side))
            out = io.BytesIO()
            image.save(out, format="PNG")
            return out.getvalue()
    except ImportError:
        pass
    if shutil.which("sips"):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "small.png"
            result = subprocess.run(["sips", "-Z", str(max_side), str(path), "--out", str(target)],
                                    capture_output=True)
            if result.returncode == 0 and target.exists():
                return target.read_bytes()
    print(f"warning: could not downscale {path}; sending as is", file=sys.stderr)
    return path.read_bytes()


def image_block(png_bytes):
    return {"type": "image", "source": {"type": "base64", "media_type": "image/png",
                                        "data": base64.standard_b64encode(png_bytes).decode("ascii")}}


def redact_images(request):
    """Copy of a request with image data shortened, for --dry-run printing."""
    def walk(value):
        if isinstance(value, dict):
            if value.get("type") == "base64" and "data" in value:
                return {**value, "data": f"<{len(value['data'])} base64 chars>"}
            return {k: walk(v) for k, v in value.items()}
        if isinstance(value, list):
            return [walk(v) for v in value]
        return value
    return walk(request)


def json_format(schema):
    return {"format": {"type": "json_schema", "schema": schema}}


def send(request, client=None, beta=False):
    """Send a request built by a script; return the parsed JSON verdict.

    beta=True goes through client.beta.messages (needed for `betas` / `fallbacks`).
    Raises RuntimeError on a refusal or a cut-off answer.
    """
    check_budget()
    client = client or make_client()
    api = client.beta.messages if beta else client.messages
    response = api.create(**request)
    record_spend(request["model"], response.usage, Path(sys.argv[0]).name)
    if response.stop_reason == "refusal":
        raise RuntimeError(f"model refused: {getattr(response, 'stop_details', None)}")
    if response.stop_reason == "max_tokens":
        raise RuntimeError("answer cut off at max_tokens")
    text = next(block.text for block in response.content if block.type == "text")
    return json.loads(text)


def dry_run_print(request):
    print(json.dumps(redact_images(request), indent=2))
