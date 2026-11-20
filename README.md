# RTLAirLink

Native macOS AWDL and AirDrop implementation for Realtek RTL8852BE, built on top of the core Wi-Fi driver from [AirPort_RTW89](https://github.com/FranciumOS-org/AirPort_RTW89) and the [Owly] (https://github.com/t4t5/owly) development work.

[![Support on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/taingo)

## Find me

[Discord]([https://discord.com/856029147750858793](https://discord.com/users/856029147750858793))

The project ships two top-level kext bundles together:

| Bundle | Role |
| --- | --- |
| `RTLAirLink.kext` | Wi-Fi backend and nested `AirPortRTW89Front.kext` frontend |
| `RTLAirBridge.kext` | Lilu plugin bridging the pinned Legacy Wi-Fi event/client interfaces |

The output directories and display metadata use the new branding. CFBundleName/CFBundleDisplayName are RTLAirLink, RTLAirLinkFront and RTLAirBridge; Wi-Fi model/driver labels include RTLAirLink while retaining the detected Realtek chip. Executable names, bundle identifiers, classes, boot arguments and the nested frontend directory retain their existing values. The bridge is matched to the frontend UUID; install both bundles from the same build.

## Status

**RTLAirLink v0.1.0 — experimental; not a stable general-purpose AirDrop release.**

- Development target: RTL8852BE, Intel/x86_64 macOS build `25G241` and the exact Legacy Wi-Fi ABI described in [installation notes](docs/INSTALL.md).
- Earlier development testing completed real file transfers in both directions over AWDL. Intermittent discovery, missing endpoints and long Waiting remained.
- This release corrects dynamic BlockAck CAM fallback and the scheduling gate for BlockAck replies. Source tests, sanitizer checks, native compilation, import checks and signatures passed in the research build. Its backend was observed loaded after reboot; full bridge activation and repeated two-way stability tests remain pending.
- Single-channel AP/AWDL coexistence and approximate radio timing remain limitations. Compilation or signing does not establish working AirDrop on another machine.

Other PCI IDs present in inherited source are not a claim of tested support. Apple Silicon is not supported by this build.

## Downloads and installation

Use the paired ZIP from [GitHub Releases](https://github.com/taingo936-ui/RTLAirLink/releases). Read [INSTALL.md](docs/INSTALL.md) before changing OpenCore. Apple Wi-Fi system kexts, Bluetooth drivers, Lilu, AMFIPass and a personal EFI/config are not included in the release.

The same release files are also distributed through [GitHub Packages](https://github.com/users/taingo936-ui/packages/container/package/rtlairlink) as `ghcr.io/taingo936-ui/rtlairlink:v0.1.0`. See [download and publishing instructions](docs/PACKAGES.md).

As in the [upstream installer guide](https://github.com/FranciumOS-org/AirPort_RTW89#requirements), install **IOSkywalkFamily, IO80211FamilyLegacy and AMFIPass**, plus Lilu. The release includes `Download-Dependencies.command`, pinned download hashes and the [complete dependency/setup guide](docs/DEPENDENCIES.md), including the system-Skywalk block and Tahoe boot argument.

## Build from source

Requirements: Intel macOS `25G241`, Xcode with its macOS SDK, Python 3 and the matching Legacy Wi-Fi dependency available for import validation. MacKernelSDK, driver dependencies and firmware are included with their notices.

```bash
git clone https://github.com/taingo936-ui/RTLAirLink.git
cd RTLAirLink
bash build.sh
```

If the exact Legacy dependency is not loaded, supply its executable from your own copy for build-only checking:

```bash
bash build.sh --legacy-image /path/to/IO80211FamilyLegacy.kext/Contents/MacOS/IO80211FamilyLegacy
```

The supplied image must match UUID `4C07538B-62EB-3D6D-AEF9-93BCDCF45FAB` and the SHA256 checked by `Source-Build/tools/check_kpi_legacy.py`. A different loaded Legacy image is rejected.

`build.py` runs the production-source host checks, compiles the driver/frontend, rebinds the bridge to the rebuilt frontend UUID, validates imports/vtable/signatures and writes `build/<timestamp>/VERIFIED_KEXTS` plus a ZIP. An unsuccessful build does not produce that verified directory. Build-only platform checking does not remove the production bridge's UUID, instruction-prefix, entitlement or stage guards. No build script installs kexts, edits EFI/NVRAM or restarts the computer.

For source checks only:

```bash
bash build.sh --checks-only
```

See [BUILD.md](docs/BUILD.md), [STATUS.md](docs/STATUS.md), [MODIFICATIONS.md](MODIFICATIONS.md), and the [Vietnamese overview](README_VI.md).

## License and attribution

The driver/bridge work is distributed under GPLv2; retain original copyright and license notices. [CREDITS.md](CREDITS.md) identifies the inherited projects. Dependencies retain their licenses in their source trees and [LICENSES](LICENSES/). Realtek firmware remains under its own terms, including its redistribution and patent conditions; it is not relicensed under GPLv2. RTLAirLink is an independent derivative project and does not claim endorsement by Realtek, Apple or upstream authors.
