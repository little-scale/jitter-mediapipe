# Corresponding source availability

The distributed `libmediapipe.dylib` contains code from Eigen, which is
primarily licensed under the Mozilla Public License 2.0. To make the exact
corresponding source immediately available, this package includes:

```text
third_party_sources/eigen-ea13a98decd497a8c5588fb5de71b57bcf10d864.tar.gz
```

- Upstream commit: `ea13a98decd497a8c5588fb5de71b57bcf10d864`
- Upstream archive: https://gitlab.com/libeigen/eigen/-/archive/ea13a98decd497a8c5588fb5de71b57bcf10d864/eigen-ea13a98decd497a8c5588fb5de71b57bcf10d864.tar.gz
- Bundled archive SHA-256: `5d01cd814226350cc65254e005de78a78af62c6f090436e718d7cd1858346bf1`

The complete MediaPipe source corresponding to the runtime is available at:

- Repository: https://github.com/google-ai-edge/mediapipe
- Base commit: `251c0cb9687230682929a64d413751f1a4f8a6d5`

The runtime build adds a combined shared-library target that exports the Hand,
Pose, and Face Landmarker C APIs. It also applies a local Bazel rules_java
compatibility adjustment. These changes affect build configuration rather than
the MediaPipe inference implementation. Exact patches are included in the
`patches` directory.

The JitTrackers wrapper source is distributed under the MIT License. A source
release should accompany each binary release and contain the `source`, `help`,
`patches`, top-level CMake, package metadata, documentation, and license
directories.
