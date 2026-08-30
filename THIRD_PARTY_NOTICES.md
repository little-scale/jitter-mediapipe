# Third-party notices

JitTrackers includes or links with the components listed below. The original
`jit.handpose`, `jit.pose`, and `jit.facemesh` wrapper code is licensed under
the MIT License in the package root. Third-party components remain under their
own licenses; the MIT License does not replace or modify those terms.

Exact license texts are included in the `licenses` directory. This inventory
was generated from the configured Bazel dependency graph and the final linker
inputs used to build `libmediapipe.dylib`.

## Google MediaPipe and model bundles

- MediaPipe — Apache License 2.0 — `licenses/MediaPipe-Apache-2.0.txt`
- Hand Landmarker model bundle — Apache License 2.0
- Pose Landmarker Lite model bundle — Apache License 2.0
- Face Landmarker model bundle, including BlazeFace, Face Mesh V2, and face
  blendshapes — Apache License 2.0

The model files, official download locations, model cards, and SHA-256 values
are documented in `MODEL_PROVENANCE.md`. MediaPipe and the model names are used
only to identify upstream technology. This distribution is not affiliated
with, sponsored by, or endorsed by Google LLC.

## Native inference runtime

- Google LiteRT — Apache License 2.0 — `licenses/LiteRT-Apache-2.0.txt`
- TensorFlow — Apache License 2.0 — `licenses/TensorFlow-Apache-2.0.txt`
- XLA — Apache License 2.0 — `licenses/XLA-Apache-2.0.txt`
- TensorFlow System Libraries — Apache License 2.0 —
  `licenses/TSL-Apache-2.0.txt`
- Abseil — Apache License 2.0 — `licenses/Abseil-Apache-2.0.txt`
- FlatBuffers — Apache License 2.0 —
  `licenses/FlatBuffers-Apache-2.0.txt`
- gemmlowp — Apache License 2.0 — `licenses/gemmlowp-Apache-2.0.txt`
- ruy — Apache License 2.0 — `licenses/ruy-Apache-2.0.txt`
- utf8_range — Apache License 2.0 —
  `licenses/utf8_range-Apache-2.0.txt`
- KleidiAI — Apache License 2.0 and BSD 3-Clause components —
  `licenses/KleidiAI-Apache-2.0.txt` and
  `licenses/KleidiAI-BSD-3-Clause.txt`
- Protocol Buffers — BSD 3-Clause — `licenses/Protobuf-BSD.txt`
- XNNPACK — BSD 3-Clause — `licenses/XNNPACK-BSD.txt`
- cpuinfo — BSD 3-Clause — `licenses/cpuinfo-BSD.txt`
- clog — BSD 3-Clause — `licenses/clog-BSD.txt`
- pthreadpool — BSD 3-Clause — `licenses/pthreadpool-BSD.txt`
- gflags — BSD 3-Clause — `licenses/gflags-BSD.txt`
- glog — BSD 3-Clause — `licenses/glog-BSD.txt`
- EasyEXIF — BSD 2-Clause — `licenses/easyexif-BSD.txt`
- FP16 — MIT — `licenses/FP16-MIT.txt`
- FXdiv — MIT — `licenses/FXdiv-MIT.txt`
- FarmHash — MIT — `licenses/FarmHash-MIT.txt`
- stb_image — MIT or Public Domain — `licenses/stb-LICENSE.txt`
- Ooura FFT — permissive project license —
  `licenses/Ooura-FFT-LICENSE.txt`
- zlib — zlib License — `licenses/zlib-LICENSE.txt`
- Eigen — primarily Mozilla Public License 2.0, with files under compatible
  BSD, Apache 2.0, and MINPACK terms — `licenses/Eigen-COPYING.README`,
  `licenses/Eigen-MPL-2.0.txt`, `licenses/Eigen-BSD.txt`,
  `licenses/Eigen-Apache-2.0.txt`, and `licenses/Eigen-MINPACK.txt`

The exact Eigen source used by the binary is included under
`third_party_sources`; see `SOURCE_OFFER.md`.

## Dynamic libraries

- OpenCV 3.4.11 `core` and `imgproc` — BSD 3-Clause —
  `licenses/OpenCV-BSD-3-Clause.txt`

## Host SDK

The externals were built with the Cycling '74 Max SDK. The SDK grants broad
permission to use and distribute software built with it provided its notice is
retained. The SDK itself and the Max application are not bundled.

- Cycling '74 Max SDK — permissive license —
  `licenses/Max-SDK-LICENSE.txt`

Max, MSP, and Jitter are trademarks of Cycling '74. Their names are used only
to describe compatibility. This distribution is not affiliated with,
sponsored by, or endorsed by Cycling '74.
