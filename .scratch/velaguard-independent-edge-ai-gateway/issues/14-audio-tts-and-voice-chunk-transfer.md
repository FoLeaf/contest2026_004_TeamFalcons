# Audio, TTS, and Voice Chunk Transfer

Status: incomplete

## Parent

.scratch/velaguard-independent-edge-ai-gateway/PRD.md

## What to build

Complete the audio enhancement path without weakening the Local Safety Loop. VelaGuard should play local alarm sounds, optionally request TTS for AI diagnosis, and support short H750B-DK voice capture upload through MQTT chunks with bounded memory use.

## Acceptance criteria

- [ ] Local alarm sounds play for configured alarm severities.
- [ ] Audio failure is shown/logged but does not affect visual alarms or Modbus acquisition.
- [ ] Operator can mute audio without acknowledging or resolving alarms.
- [ ] AI diagnosis can request TTS playback through MQTT.
- [ ] TTS failure leaves the text diagnosis available and logs the failure.
- [ ] H750B-DK short voice capture can start, chunk, end, and report result over MQTT topics.
- [ ] Voice chunks are bounded in size and use QoS 1.
- [ ] The board controls chunk pacing or backpressure so voice transfer does not starve acquisition or UI.
- [ ] Voice upload includes session ID, sequence, duration, codec, total chunks, and checksum metadata.
- [ ] Phone audio upload is not implemented as the primary path; phone speech should be converted to text before submission.

## Blocked by

- .scratch/velaguard-independent-edge-ai-gateway/issues/03-local-safety-loop-with-modbus-sensor.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/07-mqtt-identity-auth-status-and-topic-contract.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/09-ai-bridge-diagnosis-loop.md
