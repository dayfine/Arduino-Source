"""Shared Claude client setup for the lab scripts (triage_failure.py, watchdog_shot.py).

Uses the official `anthropic` SDK. Credentials: zero-arg Anthropic() reads ANTHROPIC_API_KEY or the
`ant auth login` profile. The SDK is imported lazily so --dry-run and the tests run without it.
"""

import base64
import io
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

FALLBACK_BETA = "server-side-fallback-2026-07-01"
MAX_SIDE = 640


def make_client():
    import anthropic  # lazy: not needed for --dry-run
    return anthropic.Anthropic()


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
    client = client or make_client()
    api = client.beta.messages if beta else client.messages
    response = api.create(**request)
    if response.stop_reason == "refusal":
        raise RuntimeError(f"model refused: {getattr(response, 'stop_details', None)}")
    if response.stop_reason == "max_tokens":
        raise RuntimeError("answer cut off at max_tokens")
    text = next(block.text for block in response.content if block.type == "text")
    return json.loads(text)


def dry_run_print(request):
    print(json.dumps(redact_images(request), indent=2))
