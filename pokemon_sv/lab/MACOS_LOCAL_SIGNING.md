# Persistent local macOS signing

Configured 2026-10-24 to retain camera/audio consent across local rebuilds.
The release installation is separate and was not modified by this setup.

## Agent quick start and diagnosis

Read this before opening or rebuilding a local capture app. The lab identity is
separate from the installed release app; profiles isolate settings but do not
create a separate macOS permission identity.

1. Reuse the existing login-keychain certificate and the configured lab build.
   Do not recreate/import a certificate on each build or use ad-hoc signing.
2. Preserve the lab bundle ID, certificate and stable app path below. Keep the
   release bundle ID and its Developer ID signature unchanged. Do not reuse
   `PokemonAutomation.SerialPrograms` for local lab builds.
3. Close only the confirmed stopped lab instance before building. Complete
   deployment first, then sign; helpers/resources added afterward invalidate it.
4. Run strict verification and inspect the designated requirement:

   ```sh
   codesign --verify --strict --verbose=2 build-prep/app/SerialPrograms.app
   codesign -d -r- --verbose=2 build-prep/app/SerialPrograms.app
   /usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' build-prep/app/SerialPrograms.app/Contents/Info.plist
   shasum -a 256 build-prep/app/SerialPrograms.app/Contents/MacOS/SerialPrograms
   ```

5. Launch the same app with its named profile. Verify actual video and audio,
   rather than treating successful signing as proof of device access.

If permission prompts return, record which app requested which permission and
compare both apps' bundle IDs/designated requirements before changing anything.
A linker ad-hoc signature can bind identity to a changing binary hash; a stable
certificate-backed requirement avoids that specific rebuild identity change.
The old lab also shared the release bundle ID, a plausible source of grant
conflicts. That diagnosis is not proof of every TCC prompt's cause.

First-time grants or OS/keychain changes can still need normal user consent.
Do not reset/edit TCC databases, re-sign the installed release, request broader
privacy access, or promise that every future macOS prompt is eliminated. Preserve
normal macOS approval. These changes target camera/microphone identity; they do
not automatically grant Accessibility, Screen Recording or unrelated permissions.

Reusable CMake implementation: code branch `build/macos-local-signing`, adopted
on the isolated prototype. Bootstrap helper lives on agents at
`tools/setup_macos_local_signing.py`; it is setup orchestration, not an application
test. Existing-Mac builds need no bootstrap. New-machine setup requires an
explicit request and the normal keychain/OS authorization.

## Identity and build

Local certificate: `PokemonAutomation Local Development`, in the login keychain.
Private key imported non-extractable, with codesign access; temporary plaintext
key and PKCS#12 removed. No keys or certificates are stored in Git. Certificate
trust approval completed through macOS; trust was limited to code signing in
the user domain. A trust override was not needed to produce/verify the initial
local signature; the reusable setup helper does not request one.

The reusable CMake change is on independent fork branch
`build/macos-local-signing`, head `58298f6b0` (commits `207155bf2`, `9d7e551ca`,
`58298f6b0`). Adopted on `prototype/sv-isolated-hunt`, head `6ca85664b`.
No PR was created. Default builds remain unchanged unless signing is configured.

For the configured isolated worktree:

```sh
cmake -S SerialPrograms -B build-prep/app \
  -DPA_MACOS_SIGNING_IDENTITY='PokemonAutomation Local Development' \
  -DPA_MACOS_BUNDLE_IDENTIFIER=com.dayfine.pokemonautomation.lab \
  -DPA_MACOS_BUNDLE_NAME='PokemonAutomation Lab'
cmake --build build-prep/app -j 4
```

Existing Qt/OpenCV/SDK configure values remain in this build's cache; a fresh
build still needs the normal configuration from CONTROL.md.

Stable app path:
`/Users/difan/Projects/Arduino-Source-isolated/build-prep/app/SerialPrograms.app`.
Keep this path, bundle ID and certificate consistent. The cache stores the
certificate name, never a password/private key. Do not re-create the identity
for each worktree or rebuild. First-time setup on another Mac, only when
explicitly authorized: `python3 tools/setup_macos_local_signing.py`.

A post-link step signs explicit binary target builds; a default ALL target
also signs metadata-only builds (plist changes do not necessarily relink).
For an explicit refresh use `--target SerialProgramsLocalSignature`.
Close the owning app before a rebuild. Do not mutate a signed bundle afterward.
Native test helper now lives at build-prep/app/IsolatedTests, outside the bundle;
all seven groups passed from that location. Its local orchestration script was
updated accordingly. Later tests must not put helpers back inside the app.

## Verification

Initial signed binary SHA256:
`a12b362591e2e5f69d09212d134a8f43eb6614af1eda5727f553bec010704794`.
Second build after app-name metadata change, signed SHA256:
`3ebd4a927ee609d37243d7ed05a49740bf8b166a112d5a9d28031a14f4f9e55f`.
Binary hash changed; both designated requirements were exactly:

```text
identifier "com.dayfine.pokemonautomation.lab" and certificate leaf = H"fe8b18fa9fe7d2924112749d5bb216bcab01fe56"
```

Both signatures verified strictly. The second app launch showed working game
video and a nonzero audio spectrum with no new UserNotificationCenter permission
prompt observed. This verifies this rebuild/relaunch sequence, not every future
macOS update or manual certificate/path change. No TCC database edits or privacy
check bypasses were made. A read-only TCC query was denied; no added database
access was requested. Evidence uses actual capture and signing inspection.

Raw logs/comparison are under isolated worktree build-prep/isolated-live-2026-10-24:
`build-signed-first.log`, `build-signed-second.log`, `signing-comparison.json`.
UI evidence `/tmp/signing-first-ui.png`, `/tmp/signing-second-ui.png`.

A later live reopen prompted for microphone access. Release and lab IDs and
certificate requirements were verified distinct. After approval, release -> lab
-> release showed no additional permission prompt. The earlier ad-hoc lab used
the release bundle ID, so stale grants are plausible but not proven as the cause.
See the current location/handback record for device state; the prototype is now
closed and release AZP is open/stopped without a game picture.

References:
- https://developer.apple.com/documentation/technotes/tn3127-inside-code-signing-requirements
- https://developer.apple.com/library/archive/documentation/Security/Conceptual/CodeSigningGuide/Procedures/Procedures.html


## Evening isolated code build

Prototype `5db7dfe1b` built and passed strict signature verification with the
same identity and lab bundle ID. Binary SHA256: `d835a99571957987338e19ff45e35d22ccd95540201e2dfa7bff01ead9f91018`.
Native helper SHA256: `a8ddfa0a03cfcfaf05987931816ad3dad9d4ec633d20682f463c2a5b98aec6d4`; helper remains outside the bundle.
No app relaunch/permission switching check occurred in this background task.
See [run record](../runs/isolated-2026-10-24/RESUME_CODE_FIX.md). Updated notes are
local on agents; only the prototype branch was pushed.

## Current corridor build verification

Prototype `be6f865de` retained the same lab ID/certificate and passed strict
app/executable signature verification. SHA256:
`315d7d9eddc4acf7c41066670794e026ea98c270040240e0592bb98e777ef230`.
On 2026-10-25 the signed app was opened at the stable path with profile
`sv-isolated-live-20261024`; game video and device connection were available.
No new consent prompt was observed in this launch. This is a bounded observation,
not a claim that future rebuilds or OS changes can never prompt. Current device
state belongs in run records, not the historical handback paragraphs above.
