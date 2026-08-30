# Privacy

JitTrackers processes Jitter matrices locally inside the Max application.

- It does not make network requests.
- It does not transmit camera frames, landmarks, or metadata.
- It does not write camera frames or inference results to disk.
- It retains only the newest waiting frame in memory and replaces or releases
  that frame as processing continues.
- The bundled face model estimates landmarks and facial expressions; it does
  not perform identity recognition.

Patches built by users may record, transmit, or otherwise process the object's
inputs and outputs. The creator and operator of such a patch are responsible
for obtaining appropriate consent and meeting applicable privacy obligations.
