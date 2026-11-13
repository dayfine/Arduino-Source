"""Stage and validate macOS lab apps without launching them or opening devices."""

import hashlib
from pathlib import Path
import plistlib
import re
import shutil
import subprocess

from lab_profiles import lab_profile, read_json, write_json


def sha256(path):
    with Path(path).open("rb") as stream:
        digest = hashlib.sha256()
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def app_files(app):
    """Require the executable basename used by the existing macOS profiles."""
    app = Path(app)
    plist = app / "Contents" / "Info.plist"
    with plist.open("rb") as stream:
        info = plistlib.load(stream)
    if info.get("CFBundleExecutable") != "SerialPrograms":
        raise ValueError("Expected executable SerialPrograms; renaming it changes settings paths.")
    binary = app / "Contents" / "MacOS" / "SerialPrograms"
    if binary.is_symlink() or plist.is_symlink() or not binary.is_file():
        raise ValueError("Executable and plist must be regular files inside the app.")
    if binary.resolve().parent != app.resolve() / "Contents" / "MacOS":
        raise ValueError("Executable directory must not redirect outside the app.")
    return plist, binary, info


def preflight(app, profile):
    """Check the original linked artifact before any packaging/signature modifications."""
    profile = lab_profile(profile)
    _, binary, _ = app_files(app)
    receipt = read_json(str(binary) + ".lab-build.json")
    if receipt.get("schema") != 1 or receipt.get("profile") != profile:
        raise ValueError("Missing or mismatched compiled lab profile receipt.")
    if receipt.get("source_clean") is not True:
        raise ValueError("Build receipt requires a clean committed source tree.")
    if not re.fullmatch(r"[0-9a-f]{40}", receipt.get("source_revision", "")):
        raise ValueError("Build receipt requires a full source revision.")
    if sha256(binary) != receipt.get("binary_sha256"):
        raise ValueError("Binary differs from the build receipt; rebuild before staging.")
    return receipt


def run_codesign(arguments):
    subprocess.run(["/usr/bin/codesign", *arguments], check=True, capture_output=True)


def distinct_slots(candidate, peers):
    """Reject collisions with the deployment manifests supplied by the caller."""
    for peer in peers:
        for field in ("profile", "console", "bundle_id"):
            if candidate[field] == read_json(peer)[field]:
                raise ValueError("Lab deployment conflicts with peer: " + field)


def stage(app, output, profile, console, bundle_id, signing_identity, peers=(), signer=run_codesign):
    """Create an unused staging directory and signed app; never replace an active app."""
    receipt = preflight(app, profile)
    if not re.fullmatch(r"com\.dayfine\.pokemonautomation\.lab[A-Za-z0-9_-]*", bundle_id):
        raise ValueError("Use a dedicated com.dayfine.pokemonautomation.lab... bundle ID.")
    if console not in ("A", "B") or not signing_identity.strip() or signing_identity == "-":
        raise ValueError("Select console A/B and a persistent signing identity, not ad-hoc '-'.")
    manifest = {
        "schema": 1, "profile": profile, "console": console, "bundle_id": bundle_id,
        "source_revision": receipt["source_revision"], "app": "SerialPrograms.app",
        "hardware_ownership_enforced": False,
    }
    distinct_slots(manifest, peers)
    output = Path(output)
    if output.resolve() == Path(app).resolve() or Path(app).resolve() in output.resolve().parents:
        raise ValueError("Staging directory must be separate from the source app.")
    output.mkdir(mode=0o700)  # Exclusive; an existing deployment is never overwritten.
    try:
        staged = output / manifest["app"]
        shutil.copytree(app, staged, symlinks=True)
        plist, binary, info = app_files(staged)
        info.update({
            "CFBundleIdentifier": bundle_id,
            "CFBundleName": "PokemonAutomation Lab " + console,
            "CFBundleDisplayName": "PokemonAutomation Lab " + console + " (" + profile + ")",
        })
        with plist.open("wb") as stream:
            plistlib.dump(info, stream)
        signer(["--force", "--deep", "--sign", signing_identity, str(staged)])
        signer(["--verify", "--deep", "--strict", str(staged)])
        # Signing can change Mach-O bytes: bind the final artifact separately.
        manifest["binary_sha256"] = sha256(binary)
        manifest["plist_sha256"] = sha256(plist)
        write_json(output / "lab-deployment.json", manifest)
    except BaseException:
        shutil.rmtree(output)
        raise
    return output / "lab-deployment.json"


def verify(manifest_path, profile, peers=(), signer=run_codesign):
    """Validate a staged package's identity, hashes and signature without launching it."""
    manifest_path = Path(manifest_path)
    manifest = read_json(manifest_path)
    if manifest.get("schema") != 1 or manifest.get("profile") != lab_profile(profile):
        raise ValueError("Deployment profile does not match the requested lab.")
    if manifest.get("app") != "SerialPrograms.app":
        raise ValueError("Unexpected app path in deployment manifest.")
    app = manifest_path.parent / manifest["app"]
    plist, binary, info = app_files(app)
    receipt = read_json(str(binary) + ".lab-build.json")
    if (receipt.get("schema") != 1 or receipt.get("profile") != manifest["profile"]
            or receipt.get("source_revision") != manifest.get("source_revision")
            or receipt.get("source_clean") is not True):
        raise ValueError("Deployment identity differs from the packaged build receipt.")
    if info.get("CFBundleIdentifier") != manifest["bundle_id"]:
        raise ValueError("Bundle identifier differs from the deployment manifest.")
    if sha256(binary) != manifest["binary_sha256"] or sha256(plist) != manifest["plist_sha256"]:
        raise ValueError("Staged app changed after packaging.")
    distinct_slots(manifest, peers)
    signer(["--verify", "--deep", "--strict", str(app)])
    return manifest
