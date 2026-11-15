# RTLAirLink Fix46 — experimental

First paired RTLAirLink distribution of the Owly Fix46 development line, derived from AirPort_RTW89.

Included bundles:

- `RTLAirLink.kext`, including `AirPortRTW89Front.kext`.
- `RTLAirBridge.kext`, matched to that frontend UUID.

This release renames the outer bundles and display metadata. Wi-Fi model/driver labels use RTLAirLink while preserving the detected Realtek chip/vendor. Internal identifiers, executable/class names, boot arguments and the nested frontend directory retain their original values. Update OpenCore BundlePath entries according to INSTALL.md; do not load old and new names together.

Fix46 retains Fix45 HT capability and corrects dynamic BlockAck CAM fallback and confirmed-slot reply scheduling. Intermittent Waiting/discovery and single-channel coexistence limitations remain under investigation. This is not a stable or universal AirDrop release. The renamed/rebuilt cohort has not yet been validated by repeated real-world AirDrop tests or by observing the new names in System Information after boot.

The paired binary ZIP includes installation instructions, the public build report and license notices. The source archive and tagged repository revision provide the corresponding GPL-covered source and build scripts. Dependency and firmware licenses remain separate. No personal diagnostics, EFI configuration or Apple system kexts are included.

Target: RTL8852BE, Intel/x86_64 macOS build 25G241 and the exact Legacy4C ABI documented in the repository. Consult the release BUILD_REPORT.json for compile checks and binary hashes; those checks do not establish runtime compatibility on another machine.
