# Validation status

## Research baseline

Fix45 completed file transfers in both directions over real AWDL in development testing. Waiting and disappearing discovery targets recurred; that result is not a stability qualification.

Fix46's late-slot reply and dynamic BlockAck CAM regressions failed against unchanged Fix45 source and passed after the fixes. Normal and ASan/UBSan execution passed. The research Python suite reported 205 passes and one optional historic-fixture skip out of 206 tests.

The complete research build passed four runs of 12 software-AWDL checks and four runs of 28 SCC-lifecycle checks, plus import, compiled-vtable and signature validation. Its backend UUID was observed loaded after reboot. Full bridge activation and repeated real-world two-way tests for Fix46 remain pending.

## Publication build

The release build report records the checks and binaries produced by the public builder. Newly rebuilt binaries require runtime validation even when the code logic matches the research candidate. Directory renaming and compilation checks do not establish discovery reliability, RF delivery or AirDrop stability.

Known limitations include a single actual radio channel, incomplete AP/off-channel coexistence, approximate radio clock mapping, and intermittent discovery/endpoint resolution. Shared Apple ID/contact avatar/name is a separate display behavior; the driver does not replace that identity with a device label.
