# Manual Profile Configuration Flow

Status: incomplete

## Parent

.scratch/velaguard-independent-edge-ai-gateway/PRD.md

## What to build

Add the Manual Profile path for sensors whose register information comes from uploaded documentation. The phone or Web side uploads the raw manual to cloud parsing, VelaGuard receives a structured Manual Profile, and the same Candidate Configuration safety flow is used to activate selected fields.

## Acceptance criteria

- [ ] A phone or Web path can upload a sensor manual to a cloud/manual parsing service without requiring board-side PDF parsing.
- [ ] The cloud service returns a Manual Profile containing protocol, default serial settings, register map, data types, scaling, and units.
- [ ] VelaGuard stores the Manual Profile metadata and makes it available for configuration generation.
- [ ] The UI lets the operator select desired fields from the Manual Profile.
- [ ] Natural-language instructions can reference the Manual Profile to generate a Candidate Configuration.
- [ ] The generated Candidate Configuration reuses the same validation, risk check, preview, test-read, and Local Confirmation flow as natural-language setup.
- [ ] Manual Profile parsing failure or missing fields is shown clearly and does not change active configuration.
- [ ] Full raw PDF/manual content is not retained indefinitely by default.

## Blocked by

- .scratch/velaguard-independent-edge-ai-gateway/issues/10-natural-language-candidate-configuration-flow.md
