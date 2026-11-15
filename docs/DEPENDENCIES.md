# Three required upstream dependencies

The [AirPort_RTW89 installation guide](https://github.com/FranciumOS-org/AirPort_RTW89#requirements) requires the following three kexts in addition to Lilu. RTLAirLink retains that requirement and adds its matched RTLAirBridge plugin.

| Dependency | Pinned upstream download |
| --- | --- |
| `IOSkywalkFamily.kext` | [IOSkywalkFamily-v1.2.0.zip](https://raw.githubusercontent.com/dortania/OpenCore-Legacy-Patcher/d9604c36a432eaf243ea18659ff4d208187452d7/payloads/Kexts/Wifi/IOSkywalkFamily-v1.2.0.zip) |
| `IO80211FamilyLegacy.kext` | [IO80211FamilyLegacy-v1.0.0.zip](https://raw.githubusercontent.com/dortania/OpenCore-Legacy-Patcher/d9604c36a432eaf243ea18659ff4d208187452d7/payloads/Kexts/Wifi/IO80211FamilyLegacy-v1.0.0.zip) |
| `AMFIPass.kext` | [AMFIPass-v1.4.1-RELEASE.zip](https://raw.githubusercontent.com/dortania/OpenCore-Legacy-Patcher/d9604c36a432eaf243ea18659ff4d208187452d7/payloads/Kexts/Acidanthera/AMFIPass-v1.4.1-RELEASE.zip) |

These are the exact provider URLs referenced by upstream, pinned to OpenCore Legacy Patcher commit `d9604c36a432eaf243ea18659ff4d208187452d7`. The archive labels differ from some internal CFBundleVersion values. The downloaded Legacy executable was checked against this project's pinned UUID `4C07538B-62EB-3D6D-AEF9-93BCDCF45FAB` and SHA256; Skywalk also matches the development UUID. Download hashes and identities are recorded in DEPENDENCIES.json.

As in upstream, these external kext binaries are not repackaged in this project's ZIP. They retain their own ownership/licenses. Obtain [Lilu](https://github.com/acidanthera/Lilu/releases) separately; the development setup used 1.7.3. Working Bluetooth support is also necessary for AirDrop discovery; it is separate from these three Wi-Fi-stack dependencies.

## Download helper

On macOS, open `Download-Dependencies.command` in the release root, or run:

```bash
python3 tools/download_dependencies.py
```

Python 3 is required. The helper downloads the three original ZIPs into `~/Downloads/RTLAirLink-Dependencies`, verifies SHA256, size and ZIP CRC, and leaves them unextracted. It does not modify EFI, install kexts, change NVRAM/security settings or reboot. Existing files are reused only when their hashes match; a mismatch is an error rather than a silent overwrite.

## Install following the upstream manual procedure

1. Keep a bootable copy of your current OpenCore EFI on USB.
2. Unzip the three downloaded dependencies. In the **downloaded copy** of `IO80211FamilyLegacy.kext`, remove `Contents/PlugIns/AirPortBrcmNIC.kext` as upstream instructs for non-Broadcom cards. Do not edit the copy currently running from your backup EFI.
3. Copy the three dependencies and the paired `RTLAirLink.kext`/`RTLAirBridge.kext` into `EFI/OC/Kexts`.
4. Snapshot with ProperTree and check `Kernel -> Add`: `Lilu`, `RTLAirBridge`, `IOSkywalkFamily`, `IO80211FamilyLegacy`, `AMFIPass`, `RTLAirLink`, then `RTLAirLink.kext/Contents/PlugIns/AirPortRTW89Front.kext`. The bridge is placed after Lilu so it can register its callbacks; the Wi-Fi stack/driver ordering follows upstream. Remove stale duplicate entries under the previous project names.
5. Add the upstream system-Skywalk exclusion under `Kernel -> Block`: `Identifier=com.apple.iokit.IOSkywalkFamily`, `Strategy=Exclude`, `Arch=x86_64`, `MinKernel=23.0.0`, `Enabled=true`.
6. Set `Misc -> Security -> SecureBootModel` to `Disabled` as required by upstream.
7. On macOS Tahoe, retain upstream's `-amfipassbeta` boot argument. For RTLAirBridge's full experimental native path also use `-owlyeb1 owlyebstage=4`. Preserve unrelated existing boot arguments.
8. For an EFI shared with Ventura or older, upstream restricts the legacy stack/driver to `MinKernel=23.0.0`. This RTLAirLink snapshot is checked only on Darwin 25/macOS build 25G241, so restrict this project's entries to that tested range and do not treat the upstream minimum as broader support for this fork.
9. Restart and verify the loaded identities and bridge status, then test Wi-Fi and both AirDrop directions. See INSTALL.md for renamed executable/BundlePath values and runtime limitations.

The original Kext Installer was designed for the original bundle names. This release supplies manual instructions and a dependency downloader; it does not claim that the unmodified upstream installer understands RTLAirLink/RTLAirBridge.
