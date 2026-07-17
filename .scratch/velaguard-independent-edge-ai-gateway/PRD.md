# VelaGuard Independent Industrial Edge AI Gateway PRD

Status: ready-for-agent

## Problem Statement

Industrial sensor gateways are often good at acquisition and forwarding, but weak at on-site configuration, explainable alarms, and operator-facing diagnostics. The user needs VelaGuard to be an independent STM32H750B-DK based industrial edge AI gateway that can keep the Local Safety Loop running without a PC, network, MQTT Broker, AI Bridge, MiMo, TTS, or ASR.

The project must also show clear openvela value for a competition: LVGL UI, networking, filesystem, task management, audio, Modbus acquisition, MQTT cloud integration, AI-assisted configuration, and robust degradation. It must not become a PC sidecar demo, a chat-only screen, or a cloud-dependent toy.

## Solution

Build VelaGuard as an independent H750B-DK gateway running openvela. The board reads at least one Modbus RTU sensor through RS485 using nanoMODBUS, evaluates local alarm rules, displays the operating state on LVGL, logs events locally, plays local alarm audio, and continues those core behaviors offline.

Cloud capability is added through a single MQTT/MQTTS path. VelaGuard connects to an MQTT Broker over RJ45 Ethernet first, with ESP-01 Wi-Fi as backup. A separate AI Bridge subscribes to MQTT requests, calls MiMo and related cloud services over HTTPS, and publishes structured responses. The board does not directly call MiMo HTTPS APIs. AI-generated Candidate Configurations must pass schema validation, risk checks, test read, and Local Confirmation before becoming active.

The same MQTT/TLS path also carries control traffic for AI diagnosis, TTS requests, voice upload chunks, status, alarms, retained status, and MQTT-only OTA. Firmware update packages are transferred through pull-based MQTT chunks, staged locally, verified by hash and signature, and only activated with rollback support.

## User Stories

1. As an on-site operator, I want VelaGuard to boot without a connected PC, so that it can run as a real industrial gateway.
2. As an on-site operator, I want the Local Safety Loop to keep running without network access, so that alarms still work during outages.
3. As an on-site operator, I want the home screen to show network, acquisition, alarm, audio, and time state, so that I can understand the device at a glance.
4. As an on-site operator, I want the home screen to prioritize the highest-severity active alarm, so that urgent conditions are visible immediately.
5. As an on-site operator, I want an alarm details page to show all Active Alarms, so that multiple simultaneous issues are not hidden.
6. As an on-site operator, I want acknowledgement to be separate from resolution, so that acknowledging an alarm does not falsely imply the condition recovered.
7. As an on-site operator, I want local alarm sound to play when a serious alarm occurs, so that I notice problems without watching the screen.
8. As an on-site operator, I want to mute audio without disabling visual alarms, so that noisy environments can still be managed safely.
9. As an on-site operator, I want VelaGuard to show stale or offline sensor state distinctly, so that old values are not mistaken for current values.
10. As an on-site operator, I want Offline Modbus devices to be probed at low frequency, so that devices can recover automatically.
11. As an on-site operator, I want Recovering devices to require consecutive successful reads before returning online, so that brief noise does not hide an unstable link.
12. As an on-site operator, I want threshold alarms to trigger only after a configured duration, so that short spikes do not create noisy alarms.
13. As an on-site operator, I want threshold alarms to resolve only after the value stays normal for a configured duration, so that recovery is stable without using default hysteresis.
14. As an on-site operator, I want jump alarms based on windowed deltas, so that sudden abnormal changes are detected even when absolute thresholds are not crossed.
15. As an on-site operator, I want local logs to survive reboot, so that I can inspect what happened after a fault.
16. As an on-site operator, I want log levels to include debug, info, warn, and error, so that the device can be quiet normally and detailed during debugging.
17. As an on-site operator, I want UI log filtering by severity and category, so that I can quickly find relevant faults.
18. As an on-site operator, I want network status to show whether RJ45 or ESP-01 is active, so that I can diagnose connectivity.
19. As an on-site operator, I want VelaGuard to prefer RJ45 and use ESP-01 only as backup, so that the product-grade link remains primary.
20. As an on-site operator, I want network reconnect to use bounded exponential backoff, so that failures do not cause constant reconnect storms.
21. As an on-site operator, I want RJ45 failback to require a stability window, so that the active link does not flap.
22. As an on-site operator, I want ESP-01 failure to affect only cloud features, so that local acquisition and alarms continue.
23. As an on-site operator, I want Modbus readings to refresh every one to two seconds for the demonstration sensor, so that the UI feels live.
24. As an on-site operator, I want VelaGuard to read one to three registers from a Modbus RTU slave, so that the first industrial scenario stays focused and reliable.
25. As an on-site operator, I want Modbus test read before activating a new Sensor Configuration, so that wrong register settings do not enter the active loop.
26. As an on-site operator, I want failed test reads to save as disabled drafts only, so that work is not lost but unsafe configs do not run.
27. As an on-site operator, I want to add a sensor using natural language, so that I do not need to hand-author JSON.
28. As an on-site operator, I want VelaGuard to ask for missing fields instead of guessing, so that ambiguous configuration does not become active.
29. As an on-site operator, I want the board to preview AI-generated configuration details, so that I can verify protocol, slave address, registers, scaling, and rules.
30. As an on-site operator, I want Local Confirmation before activating a Candidate Configuration, so that AI cannot silently alter acquisition behavior.
31. As an on-site operator, I want remote Candidate Configurations to enter a waiting list, so that they do not interrupt alarm handling.
32. As an on-site operator, I want high-risk UI operations to require long-press confirmation or a confirmation code, so that accidental touches are less dangerous.
33. As an on-site operator, I want normal home-page usage to avoid dangerous actions, so that the main display remains safe during routine monitoring.
34. As an on-site operator, I want configuration changes to write structured events, so that later audits can identify source and time.
35. As an on-site operator, I want AI diagnosis for an active alarm, so that I receive possible causes and recommended checks.
36. As an on-site operator, I want AI diagnosis to return structured JSON, so that the UI can show summary, risk level, causes, actions, shutdown recommendation, and confidence.
37. As an on-site operator, I want AI diagnosis failures to degrade to a local fallback template, so that the diagnosis page remains useful.
38. As an on-site operator, I want TTS diagnosis playback to be optional, so that audio failure does not block visual diagnosis.
39. As an on-site operator, I want short voice capture on the H750B-DK to be uploadable through MQTT chunks, so that ASR can be added without phone audio upload.
40. As an on-site operator, I want phone voice input to become text on the phone before being sent, so that mature phone speech recognition is reused.
41. As an on-site operator, I want phone or Web manual upload to produce a Manual Profile, so that the board receives structured register information instead of parsing PDFs locally.
42. As an on-site operator, I want to use natural language with a Manual Profile, so that I can choose fields such as temperature and humidity from a manual.
43. As an on-site operator, I want VelaGuard to reject illegal AI outputs, so that malformed JSON cannot corrupt configuration.
44. As an on-site operator, I want VelaGuard to reject future configuration schema versions, so that older firmware does not misread newer config.
45. As an on-site operator, I want old configuration schemas to migrate when supported, so that firmware updates do not lose settings.
46. As an on-site operator, I want configuration corruption to fall back to a valid slot or factory defaults, so that the device can still boot.
47. As an on-site operator, I want logs to tolerate truncation and bad records, so that a corrupted log does not stop acquisition.
48. As an on-site operator, I want Pending Events to resend after reconnect, so that critical events can reach the cloud after an outage.
49. As a cloud operator, I want each device to publish under its own topic root, so that routing and ACLs are simple.
50. As a cloud operator, I want production Device IDs to derive from STM32 UID, so that each physical device has a stable identity.
51. As a developer, I want test firmware to allow compile-time Device ID override, so that multiple test identities can be exercised.
52. As a production operator, I want production firmware to expose no runtime Device ID edit interface, so that identity cannot be changed accidentally.
53. As a cloud operator, I want MQTT tokens to be derived using HMAC with a product secret, so that knowing a Device ID is not enough to authenticate.
54. As a cloud operator, I want token versions and denylist support, so that compromised credentials can be rotated or blocked.
55. As a cloud operator, I want MQTT over TLS in formal environments, so that credentials and payloads are protected.
56. As a cloud operator, I want Broker ACLs to restrict each device to its own topics, so that one device cannot read or publish another device's data.
57. As a cloud operator, I want the AI Bridge to have a separate account and limited ACLs, so that cloud service permissions remain bounded.
58. As a cloud operator, I want only current status topics to be retained, so that stale requests and events are not replayed.
59. As a cloud operator, I want telemetry and trend data to use QoS 0, so that high-frequency data does not congest the Broker.
60. As a cloud operator, I want alarms, AI requests, configuration candidates, TTS, voice chunks, OTA messages, and confirmations to use QoS 1, so that important control events are at least delivered once.
61. As a cloud operator, I want fixed client IDs and clean sessions for the first version, so that reconnect behavior is simple and predictable.
62. As a cloud operator, I want LWT status, so that disconnected devices can be marked offline.
63. As a cloud operator, I want AI Bridge requests to use req_id and payload_hash, so that repeated MQTT deliveries are idempotent.
64. As a cloud operator, I want repeated completed AI requests to resend the same response, so that VelaGuard can recover from lost responses.
65. As a cloud operator, I want AI Bridge to report processing status for in-flight duplicate requests, so that the board can show progress instead of starting duplicate work.
66. As a cloud operator, I want MiMo API keys to remain only on the cloud server, so that board firmware does not contain MiMo credentials.
67. As a cloud operator, I want request and response metadata to be audited without storing full prompts, PDFs, or audio forever, so that debugging and privacy are balanced.
68. As a cloud operator, I want the AI Bridge to use HTTPS to call MiMo and related cloud services, so that the board avoids direct HTTPS AI integration.
69. As a cloud operator, I want MQTT-only OTA, so that OTA uses the same authenticated MQTT/TLS path as other device traffic.
70. As a cloud operator, I want OTA to be pull-based by chunks, so that VelaGuard controls backpressure and memory use.
71. As an on-site operator, I want OTA offers to show version, size, and risk, so that updates are not surprising.
72. As an on-site operator, I want OTA to require local confirmation or a maintenance window, so that updates do not interrupt active use.
73. As an on-site operator, I want OTA to be blocked during high-severity Active Alarms, so that safety work is not disrupted.
74. As an on-site operator, I want OTA images to be hash-checked and signature-verified, so that corrupt or unauthorized firmware is rejected.
75. As an on-site operator, I want OTA to write to a staging image before switching, so that the running firmware is not destroyed mid-download.
76. As an on-site operator, I want OTA rollback after failed self-test, so that a bad firmware does not brick the gateway.
77. As an on-site operator, I want OTA progress and result events, so that I can see whether update is downloading, verifying, committed, failed, or rolled back.
78. As a developer, I want a clear test build mode and production build mode, so that debug conveniences do not leak into production behavior.
79. As a developer, I want MQTT plain-text allowed only for local test builds, so that early integration is possible without weakening production.
80. As a developer, I want production builds to require MQTTS and production signing material, so that deployments match the safety model.
81. As a developer, I want nanoMODBUS to be used through an openvela transport adapter, so that the known library remains isolated from platform details.
82. As a developer, I want UART resources to keep RS485, ESP-01, and debug separated, so that network and Modbus do not interfere.
83. As a developer, I want the ESP-01 driver to be a non-blocking AT state machine, so that Wi-Fi failures do not block acquisition.
84. As a developer, I want ESP-01 hardware reset support, so that stuck AT states can recover.
85. As a developer, I want structured events to include ts_ms, uptime_ms, and time_quality, so that events remain ordered even before time sync.
86. As a developer, I want the cloud to store received_ts_ms separately, so that historical device timestamps are not rewritten after reconnect.
87. As a developer, I want boot_id and persistent event sequence values, so that event IDs remain debuggable across reboots.
88. As a developer, I want one active network link at a time, so that duplicated cloud traffic and split-brain behavior are avoided.
89. As a developer, I want a bounded-size buffer strategy for JSON and MQTT payloads, so that SDRAM pressure does not create unpredictable failures.
90. As a developer, I want firmware features to be independently degraded, so that UI, audio, network, AI, and Modbus failures do not cascade.
91. As a competition judge, I want the demonstration to clearly show openvela UI, networking, filesystem, audio, and task behavior, so that the system is visibly built on openvela capabilities.
92. As a competition judge, I want the demonstration to show AI as configuration and diagnosis assistance, so that it is not just a chatbot.
93. As a competition judge, I want network failure to be demonstrated without losing local alarms, so that the independent gateway claim is credible.
94. As a competition judge, I want logs and reports to show AI Coding and system behavior, so that engineering effort is auditable.
95. As a competition judge, I want the H750B-DK hardware resources to be visible in the demo, so that LCD, touch, Ethernet, audio, and UART usage are clear.

## Implementation Decisions

- VelaGuard is the board-side product boundary. It owns the Local Safety Loop: Modbus acquisition, rule evaluation, LVGL alarm UI, local logging, and local alarm audio.
- The Local Safety Loop must start before cloud features and continue during network, MQTT, AI Bridge, MiMo, TTS, ASR, and audio enhancement failures.
- The primary board is STM32H750B-DK running openvela.
- RJ45 Ethernet is the primary network path. ESP-01 Wi-Fi is the backup path.
- Only one active network link is used at a time. RJ45 failback requires a stability window.
- ESP-01 must have independent 3.3V power sized for peak current, an exclusive UART, non-blocking AT state machine behavior, bounded exponential backoff, and hardware reset support.
- USB CDC and UART debug are development and rescue channels only. They are not production runtime dependencies.
- Modbus RTU is implemented as a master/client through nanoMODBUS plus an openvela/NuttX serial transport adapter.
- The first Modbus scope is intentionally narrow: one slave, one to three registers, function codes 03 and 04, read-only, one to two second polling.
- The Modbus state model includes online, degraded, offline, and recovering.
- Offline keeps previous alarm state and performs low-frequency probes. Recovering resumes full reads but requires consecutive successes before online.
- The rule engine supports threshold-high, threshold-low, offline, sudden jump, trigger duration, and restore duration.
- Threshold alarms do not default to hysteresis. They trigger when the condition persists for trigger_duration_ms and resolve when the safe condition persists for restore_duration_ms.
- Multiple Active Alarms may exist internally. The home page shows the primary highest-priority state and the details page lists all.
- Acknowledgement is separate from Resolution.
- Repeated detections for the same unresolved condition update the same alarm_id.
- Alarm timestamps use first_seen_ts, last_seen_ts, ack_ts, and resolved_ts as Unix millisecond timestamps.
- Event payloads include ts_ms, uptime_ms, and time_quality. time_quality may be unknown, rtc, ntp, or cloud.
- Historical event timestamps are not rewritten after network recovery. The cloud records received_ts_ms separately.
- req_id identifies request/response pairs. event_id identifies persisted events for deduplication. alarm_id identifies one alarm instance until Resolution.
- Device IDs are stable. Production Device ID is derived from STM32 UID. Test firmware can override Device ID through code or compile-time configuration only.
- Production firmware exposes no runtime Device ID change path through UI, MQTT, serial CLI, HTTP, or any other interface.
- Display Name remains editable because it does not participate in identity, authentication, ACLs, or routing.
- MQTT token derivation uses an HMAC over Device ID and a product secret, with versioned derivation strings.
- Production supports token version migration and per-device denylist.
- Formal environments use MQTT over TLS, per-device tokens, and Broker ACLs.
- Test environments may use plain MQTT only for local development and only in test build mode.
- VelaGuard connects only to the MQTT Broker for cloud interaction. It does not directly call MiMo HTTPS APIs.
- AI Bridge is a separate cloud service. It subscribes to MQTT requests, calls MiMo, TTS, ASR, and manual parsing services over HTTPS, and publishes MQTT responses.
- MQTT topic roots use vg/{device_id}/... without environment prefixes.
- Retained messages are limited to Retained Status. Requests, responses, telemetry, trend data, events, and alarms are not retained.
- QoS 0 is used for telemetry, trend, and status. QoS 1 is used for alarms, AI requests and responses, Candidate Configurations, TTS, voice chunks, OTA, and confirmations.
- The first MQTT session strategy uses fixed client_id, clean_session=true, resubscribe after reconnect, LWT, and local Pending Events for critical resend.
- AI Bridge idempotency is based on req_id plus payload_hash.
- AI Bridge duplicate handling resends completed responses, returns processing for in-flight work, and only retries transient network or server failures.
- AI-generated Sensor Configurations are Candidate Configurations until device-side schema validation, risk checks, test read, and Local Confirmation succeed.
- Candidate Configuration states include draft, disabled, active, and error.
- If test read fails, the configuration may be saved as a disabled draft but must not become active.
- AI cannot directly write registers, modify active configuration, control actuators, close alarms, or override local safety rules.
- Manual upload is handled primarily by phone or Web to cloud, producing a Manual Profile. The board should not parse full PDFs locally.
- Natural language configuration without manual upload is supported when the prompt includes explicit register parameters.
- H750B-DK voice upload, when implemented, uses MQTT chunks. Phone voice upload is out of scope because phone ASR should convert speech to text locally.
- MQTT large payloads are chunked. Audio and OTA chunks should be pulled or paced so the board controls memory pressure.
- OTA is MQTT-only and pull-based. OTA offers, acceptance, chunk requests, chunk data, progress, results, and confirmation all travel over MQTT/TLS.
- OTA images are written as Staging Images, verified by sha256 and digital signature, switched only after confirmation or maintenance policy, and rolled back after failed self-test.
- OTA should be blocked during high-severity Active Alarms, unstable power or storage, or other unsafe local conditions.
- Logs follow a Log4j2-inspired, Minecraft-like rolling style without embedding real Log4j2.
- Logging outputs include latest.log, optional debug.log, archive logs, and structured events.jsonl.
- Log events include timestamp, logger/category, level, message, and key-value fields.
- Log levels include debug, info, warn, and error. Different appenders may have different minimum levels and category filters.
- Cloud upload defaults to structured events and key warn/error summaries, not full latest.log.
- Configuration storage uses dual slots with schema_version, seq, checksum, and committed flag.
- Startup chooses the newest valid committed configuration slot. If both slots are invalid, factory/default configuration is used.
- All configurations have schema_version. Firmware supports explicit min_supported_schema and current_schema behavior.
- Old configurations migrate through versioned functions. Future configuration versions are rejected.
- Missing non-critical fields get defaults and warn logs. Invalid critical fields disable the module rather than the whole device.
- UI home page shows status and alarms, not dangerous operations.
- Low-risk operations use ordinary confirmation. Medium-risk operations use second confirmation. High-risk operations use long-press confirmation or a confirmation code.
- Remote Candidate Configurations enter a waiting list and do not interrupt alarm UI.
- High-severity alarms take UI priority over configuration confirmation.
- Configuration changes write structured events with a source such as local_ui, remote_candidate, or factory_default.
- The recommended build boundary is test versus production. Test builds allow Device ID override, local plain MQTT, and verbose debug. Production builds remove runtime identity edits, require secure MQTT behavior, and hide secrets.

## Testing Decisions

- Tests should validate externally visible behavior at the highest practical seams instead of implementation details.
- The main board-side seam is the VelaGuard application boundary: feed network, Modbus, config, and MQTT events into the application and assert UI state, alarm state, logs, and outbound MQTT messages.
- The main cloud seam is the MQTT contract between VelaGuard, Broker, and AI Bridge: publish request messages and assert idempotent responses, status transitions, and ACL-safe topic usage.
- The main safety seam is the Local Safety Loop: simulate no network, MQTT disconnect, AI Bridge timeout, TTS failure, and filesystem corruption while asserting Modbus acquisition and local alarms continue.
- The main configuration seam is Candidate Configuration processing: natural language output or Manual Profile input becomes schema validation, risk checks, test read, Local Confirmation, and only then active configuration.
- The main alarm seam is the alarm state model: verify multiple Active Alarms, acknowledgement versus Resolution, repeated alarm updates, trigger duration, restore duration, jump detection, offline, and recovering behavior.
- The main network seam is network_manager behavior: RJ45 preference, ESP-01 fallback, single active link, bounded exponential backoff, LWT publication, and stable-window failback.
- The main logging seam is observable log output and structured event output: verify levels, categories, rolling behavior, event fields, and that secrets are redacted.
- The main storage seam is startup recovery: corrupt one config slot, corrupt both slots, truncate logs, corrupt Pending Events, and verify the device still boots into a safe usable state.
- The main identity and security seam is topic/auth behavior: verify Device ID derivation, test override only in test build mode, production runtime immutability, token derivation versioning, and ACL topic restrictions.
- The main OTA seam is the MQTT-only OTA state machine: offer, accept, chunk request, chunk data, progress, verification, staging, switch, confirm, failed verification, interruption, resume, and rollback.
- A minimal hardware-in-the-loop test must read at least one Modbus RTU sensor or simulator over RS485 and show local UI alarm behavior.
- A network-failure test must disconnect RJ45 and ESP-01/cloud access while the Local Safety Loop keeps running.
- An AI Bridge timeout test must show that AI diagnosis fails or degrades without blocking acquisition and alarms.
- A reconnect test must verify Pending Events resend and historical timestamps are not rewritten.
- A UI safety test must verify remote configuration cannot become active without Local Confirmation.
- A production-build test must verify no runtime Device ID modification path is available.
- A payload-pressure test must verify voice or OTA chunk transfer does not starve Modbus acquisition or UI responsiveness.
- A repeated demo test should run the minimal demonstration loop multiple times without reboot-only recovery.

## Out of Scope

- AI direct control of actuators, register writes, coil writes, or any control command that bypasses Local Confirmation.
- Long-term PC sidecar dependence for normal operation.
- Board-side direct MiMo HTTPS integration.
- Board-side HTTPS OTA download.
- Board-side full PDF/manual parsing.
- Phone audio upload to the cloud as the primary voice path.
- Multi-protocol industrial platform generalization beyond the initial Modbus RTU scope.
- Multiple active network links sending duplicate cloud traffic.
- Persistent MQTT sessions as the primary critical-event durability mechanism in the first version.
- Large desktop-class frontend frameworks on the H750B-DK.
- Complex local ASR on the H750B-DK.
- Multi-device fleet management beyond the per-device Broker, AI Bridge, token, ACL, and OTA primitives needed for VelaGuard.

## Further Notes

The first demonstration should preserve the minimum loop: H750B-DK boots, LVGL home page appears, Modbus reads a sensor or simulator, a threshold alarm is injected, UI and local audio alarm activate, AI diagnosis is requested over MQTT, the AI Bridge returns structured diagnosis, logs are saved, and network failure does not stop local acquisition or alarms.

The strongest technical argument for VelaGuard is not that MQTT is always faster than HTTPS. It is that the board uses one secure long-lived MQTTS path for status, events, AI requests, TTS control, audio chunks, OTA chunks, and confirmations, while heavyweight HTTPS integrations stay on the cloud side.

The PRD respects the accepted ADRs: independent gateway with Local Safety Loop, MQTT Broker plus AI Bridge instead of board-direct MiMo, stable production Device ID, device-side confirmation for AI-generated configuration, and MQTT-only pull-based OTA.
