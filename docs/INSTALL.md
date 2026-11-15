# Experimental installation

This build targets RTL8852BE on Intel/x86_64 macOS build `25G241`. It relies on the exact Legacy Wi-Fi ABI below. It is not a universal Realtek or stable AirDrop installer. Keep a bootable EFI backup before replacing kernel drivers.

## Dependencies

Provide your own compatible OpenCore installation and dependencies. The development setup used Lilu 1.7.3, the appropriate IOSkywalkFamily/IO80211FamilyLegacy arrangement, AMFIPass and working Realtek Bluetooth firmware/BlueToolFixup. Their required system configuration depends on the OS and is not supplied as a personal EFI here.

The bridge pins IO80211FamilyLegacy UUID `4C07538B-62EB-3D6D-AEF9-93BCDCF45FAB` (executable SHA256 `ac2dc15b77f8eaf97b71b34c5d0e1b59f862bfeac618396449306e1837e73894`) and validates instruction prefixes at runtime. Different images must not be treated as compatible merely because the filenames match. The bridge also pins the frontend UUID of its build cohort.

## OpenCore entries

Replace both top-level bundles together. The renamed folders contain the original executable filenames; use these paths in `Kernel -> Add`:

| BundlePath | ExecutablePath | PlistPath |
| --- | --- | --- |
| `RTLAirBridge.kext` | `Contents/MacOS/OwlyAirLinkBridge` | `Contents/Info.plist` |
| `RTLAirLink.kext` | `Contents/MacOS/AirPort_RTW89` | `Contents/Info.plist` |
| `RTLAirLink.kext/Contents/PlugIns/AirPortRTW89Front.kext` | `Contents/MacOS/AirPortRTW89Front` | `Contents/Info.plist` |

Lilu must load before its bridge plugin. The required Legacy/Networking dependencies must be available to the frontend. Enable the three project entries and restrict them to the tested OS range; consult the build report for the exact binaries. When migrating from the old names, replace the existing project entries rather than adding duplicates: the internal bundle identifiers are intentionally unchanged.

The runtime opt-in boot arguments retain their existing spelling:

```text
-owlyeb1 owlyebstage=4
```

Other development stages intentionally leave the full native client path inactive. Renaming the folders does not rename those arguments. Do not replace unrelated boot arguments or copy another machine's NVRAM/configuration.

Restart after installation and check the loaded bundle IDs/UUIDs against `BUILD_REPORT.json`. Signature verification and a successful reboot do not alone prove that the bridge activated or that AirDrop works.

## Display names

The new metadata advertises `RTLAirLink`, `RTLAirLinkFront` and `RTLAirBridge` as bundle display names. Wi-Fi driver-version reporting changes to `<detected chip> (RTLAirLink 0.5.0)`, while the controller model string changes to `RTLAirLink (<detected chip>)`. The vendor remains Realtek and the hardware-version field remains the actual chip model. The exact System Information presentation must be verified after installing and rebooting this build; macOS may cache labels or show hardware fields separately.

Low-level bundle IDs such as `com.rtw89.driver`, `com.rtw89.front` and `com.owly.airlink.eventbridge`, executable names and IOService class names are intentionally retained for compatibility. Seeing those identifiers in detailed diagnostics is expected.

## AirDrop validation

Keep Wi-Fi and Bluetooth enabled. For a controlled discovery test, leave Finder → AirDrop open and unlock the receiving iPhone. Send multiple small files in both directions immediately and after 30–60 seconds idle, recording discovery delay separately from upload time.

A cable can cause AirDrop to use a wired transport. Count a transfer as AWDL only when the runtime transport or a capture on `awdl0` confirms it. Prepare offline capture locally before disconnecting a router used for remote access. Intermittent Waiting/disappearance remains an open issue in this experimental line.
