# Distribution guide

JitTrackers may be distributed free of charge or commercially. The original
wrapper code is MIT licensed; bundled third-party components retain the terms
listed in `THIRD_PARTY_NOTICES.md`.

## Required release contents

Do not remove these items from a binary release:

- `LICENSE`
- `THIRD_PARTY_NOTICES.md`
- `MODEL_PROVENANCE.md`
- `SOURCE_OFFER.md`
- `PRIVACY.md`
- the complete `licenses` directory
- `third_party_sources/eigen-*.tar.gz`
- the three unmodified `.task` model bundles

Publish a matching source archive containing the MIT-licensed wrapper source
and build metadata, including the runtime build patches, alongside the binary
archive.

## Branding

Use the neutral product name JitTrackers. MediaPipe, Google, Max, MSP, Jitter,
and Cycling '74 may be named factually to describe upstream technology or host
compatibility. Do not use their logos or imply affiliation, sponsorship, or
endorsement without permission.

## macOS signing

Development releases are ad-hoc signed. This preserves bundle integrity but
does not establish an identified developer and cannot be notarized. A browser
or file-sharing service may add a quarantine attribute, causing Gatekeeper to
block the externals.

Users who trust the archive may remove quarantine from this package only:

```sh
xattr -dr com.apple.quarantine "$HOME/Documents/Max 9/Packages/JitTrackers"
```

Do not recommend disabling Gatekeeper globally. A later public release can be
signed with a Developer ID Application certificate and notarized without
changing the software licences.

## Release checks

Before publishing:

1. Build the package, then perform all signing as the final mutation.
2. Verify each `.mxo` with `codesign --verify --deep --strict`.
3. Verify the runtime dylibs and confirm every Mach-O file is arm64.
4. Extract the final ZIP into a fresh directory and repeat signature checks.
5. Run one frame through the Hand, Pose, and Face task APIs.
6. Verify model and archive SHA-256 values.
7. Confirm the release includes every required legal file listed above.

This guide is a practical compliance checklist, not legal advice.
