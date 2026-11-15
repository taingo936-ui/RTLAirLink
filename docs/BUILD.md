# Source build

Use `bash build.sh` from the repository root. Intel macOS build `25G241`, Xcode/macOS SDK, Python 3 and the matching Legacy dependency are required for this complete experimental build. No packages from pip are needed.

`BUILD_CONFIG.json` records the research build's feature flags, import allowlists and output bundle names. `Source-Build` includes the driver/frontend, shims, firmware, pinned MacKernelSDK and dependency sources. `Bridge-Source` includes the production bridge source, Lilu headers/library snapshot, tests and vendor provenance.

The public builder runs the production-source checks with normal and sanitized execution, compiles the driver/frontend and executes four fake-chip AWDL/lifecycle test groups. It checks KPI imports and the compiled native P2P vtable, then rebinds and rebuilds the bridge for the actual frontend UUID. Finally it assembles the two renamed outer bundles and verifies signatures and imports.

The compiler still uses the inherited executable and class names. The wrapper renames the final bundle directories. Source metadata and model/driver display strings also use RTLAirLink branding; chip detection, identifiers, classes and executable names are retained. Source changes or a different compiler may change the frontend UUID; never reuse a bridge from another build. Bit-for-bit reproducibility across Xcode versions is not asserted.

Output is under `build/<timestamp>/`. Only a successful complete build creates `VERIFIED_KEXTS/` and its paired ZIP. `BUILD_REPORT.json` records checks, UUIDs and hashes; local build logs may contain local paths and are ignored by Git. No EFI/NVRAM writes, loads or reboot are performed.

`bash build.sh --checks-only` runs source checks without creating kexts. For an unloaded Legacy dependency, pass `--legacy-image` with the exact executable as described in README.md. A mismatched loaded Legacy image remains an error.

GPL-covered source and build scripts for each release are published in the same tagged revision and source archive. Firmware/vendor terms remain separate; retain their notices. `UPSTREAM_SNAPSHOT_SHA256.json` records the pre-publication research-source snapshot; documentation lists the subsequent publication changes.
