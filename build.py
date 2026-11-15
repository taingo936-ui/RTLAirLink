#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Added 2026-11-15: standalone build/release assembly for RTLAirLink.
"""Build a matched, signed experimental cohort. Never installs or edits EFI."""
import argparse
import datetime
import hashlib
import importlib
import json
import os
import platform
import plistlib
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CONFIG = json.loads((ROOT / 'BUILD_CONFIG.json').read_text())
CHECKS = ('ht', 'raw_tx', 'radio_timing', 'standalone', 'unicast', 'block_ack',
          'rx_filter', 'vif_restore', 'transport_wire', 'data_host', 'native_sync',
          'native_scan', 'native', 'service', 'control')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(folder):
    return {p.relative_to(folder).as_posix(): sha(p) for p in sorted(folder.rglob('*'))
            if p.is_file() and '__pycache__' not in p.parts and p.name != '.DS_Store'}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def executable(bundle):
    info = plistlib.loads((bundle / 'Contents/Info.plist').read_bytes())
    return bundle / 'Contents/MacOS' / info['CFBundleExecutable']


def uuid(path):
    text = subprocess.check_output(['xcrun', 'dwarfdump', '--uuid', str(path)], text=True)
    matches = re.findall(r'UUID: ([0-9A-Fa-f-]+) \(x86_64\)', text)
    require(len(matches) == 1, 'Expected one x86_64 UUID')
    return matches[0].upper()


def run(command, log, cwd=None, env=None):
    print('Running ' + log.name, flush=True)
    with log.open('w') as stream:
        process = subprocess.Popen(command, cwd=cwd, env=env, stdout=stream,
                                   stderr=subprocess.STDOUT)
        while True:
            try:
                result = process.wait(timeout=20)
                break
            except subprocess.TimeoutExpired:
                print('Still running ' + log.name, flush=True)
    if result:
        raise RuntimeError(log.name + ' failed:\n' + log.read_text(errors='replace')[-4000:])


def preflight(output):
    results = {}
    for name in CHECKS:
        print('Checking ' + name, flush=True)
        results[name] = importlib.import_module('check_' + name).run(
            ROOT / 'Source-Build', output / name)
    return results


def bridge_build(work, output, front_uuid):
    bridge = output / 'bridge-source'
    shutil.copytree(ROOT / 'Bridge-Source', bridge,
                    ignore=shutil.ignore_patterns('build', '__pycache__', '.DS_Store'))
    plugin = bridge / 'src/Plugin.cpp'
    raw = bytes.fromhex(front_uuid.replace('-', ''))
    replacement = 'constexpr uint8_t kFrontendUUID[16] = {\n    ' + ','.join(
        '0x%02x' % n for n in raw) + '\n};'
    text, count = re.subn(r'constexpr uint8_t kFrontendUUID\[16\] = \{[^}]+\};',
                         replacement, plugin.read_text())
    require(count == 1, 'Frontend UUID guard must bind exactly once')
    plugin.write_text(text)
    manifest = inventory(bridge)
    manifest.pop('SOURCE_SHA256.json', None)
    (bridge / 'SOURCE_SHA256.json').write_text(json.dumps(manifest, indent=2) + '\n')
    target = output / 'bridge-output'
    run(['bash', 'build_macos.sh', '--sdk', str(work / 'third_party/MacKernelSDK')],
        output / 'bridge-build.log', bridge,
        dict(os.environ, OWLY_BRIDGE_BUILD_OUTPUT=str(target)))
    bundle = target / 'UNQUALIFIED_DO_NOT_INSTALL/OwlyAirLinkBridge.kext'
    require(executable(bundle).read_bytes().count(raw) == 1, 'Bridge frontend guard differs')
    return bundle


def assemble(work, bridge, output, report):
    """Validate and package the compiled cohort; used by the complete builder."""
    driver = work / 'build/out/AirPort_RTW89.kext'
    front = work / 'build/out/AirPortRTW89Front.kext'
    host = (work / 'build/log/hosttest_fakechip.log').read_text()
    for label, expected in [('software AWDL integration', 12), ('AWDL SCC lifecycle', 28)]:
        rows = re.findall(r'== ' + label + r': (\d+) check\(s\), (\d+) failure\(s\)', host)
        require(len(rows) == 4 and all(int(n) == expected and int(f) == 0 for n, f in rows),
                'Host lifecycle results differ: ' + label)
    report['driver_host_tests'] = {'runs': 4, 'awdl_checks_per_run': 12,
                                   'lifecycle_checks_per_run': 28, 'failures': 0}
    assembly = output / 'assembly'
    assembly.mkdir()
    renamed = assembly / CONFIG['bundle_names']['backend']
    shutil.copytree(driver, renamed)
    shutil.copytree(front, renamed / 'Contents/PlugIns' / front.name)
    shutil.copytree(ROOT / 'Resources', renamed / 'Contents/Resources', dirs_exist_ok=True)
    renamed_bridge = assembly / CONFIG['bundle_names']['bridge']
    shutil.copytree(bridge, renamed_bridge)
    run(['codesign', '--force', '--sign', '-', '--timestamp=none', str(renamed)],
        output / 'driver-sign.log')
    cohort = {}
    for label, bundle, key in [
        ('backend', renamed, 'native_backend_allowed_imports'),
        ('frontend', renamed / 'Contents/PlugIns' / front.name, 'native_frontend_allowed_imports'),
        ('bridge', renamed_bridge, 'bridge_imports')]:
        run(['codesign', '--verify', '--deep', '--strict', str(bundle)],
            output / (label + '-verify.log'))
        binary = executable(bundle)
        imports = set(subprocess.check_output(['nm', '-u', '-j', str(binary)], text=True).split())
        require(not imports - set(CONFIG[key]), label + ' has unreviewed imports')
        if label == 'frontend':
            require(set(CONFIG['native_required_frontend_imports']) <= imports,
                    'Required frontend imports are missing')
        cohort[label] = {'uuid': uuid(binary), 'sha256': sha(binary),
                         'bundle_identifier': plistlib.loads((bundle / 'Contents/Info.plist').read_bytes())['CFBundleIdentifier']}
    report.update(status='BUILD_VERIFIED_RUNTIME_UNPROVEN', cohort=cohort)
    (assembly / 'BUILD_REPORT.json').write_text(json.dumps(report, indent=2) + '\n')
    shutil.copy(ROOT / 'LICENSE', assembly / 'LICENSE')
    shutil.copytree(ROOT / 'LICENSES', assembly / 'LICENSES')
    for name in ['CREDITS.md', 'docs/INSTALL.md']:
        shutil.copy(ROOT / name, assembly / Path(name).name)
    verified = output / 'VERIFIED_KEXTS'
    assembly.rename(verified)
    with zipfile.ZipFile(output / 'RTLAirLink-Fix46-x86_64.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(verified.rglob('*')):
            if path.is_file():
                archive.write(path, 'RTLAirLink-Fix46/' + path.relative_to(verified).as_posix())
    with zipfile.ZipFile(output / 'RTLAirLink-Fix46-x86_64.zip') as archive:
        require(archive.testzip() is None, 'Release ZIP CRC check failed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checks-only', action='store_true')
    parser.add_argument('--legacy-image', type=Path,
                        help='Exact Legacy executable for build-only checking when not loaded')
    args = parser.parse_args()
    require(platform.system() == 'Darwin' and platform.machine() == CONFIG['architecture'],
            'The complete build requires macOS x86_64 with Xcode')
    version = subprocess.check_output(['/usr/bin/sw_vers', '-buildVersion'], text=True).strip()
    require(version == CONFIG['macos_build'], 'Build is pinned to macOS ' + CONFIG['macos_build'])
    output = ROOT / 'build' / datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    output.mkdir(parents=True)
    report = {'status': 'INCOMPLETE', 'profile': 'Fix46', 'project': 'RTLAirLink',
              'macos_build': version, 'architecture': platform.machine(),
              'front_back_abi': 7, 'installed': False, 'efi_modified': False,
              'runtime_airdrop_verified': False,
              'limits': ['RTL8852BE experimental', 'Exact Legacy4C ABI only',
                         'Single-channel radio; AP/AWDL coexistence remains limited',
                         'Intermittent discovery/Waiting remains under investigation']}
    try:
        report['checks'] = preflight(output / 'checks')
        if args.checks_only:
            report['status'] = 'SOURCE_CHECKS_PASSED'
        else:
            work = output / 'source'
            shutil.copytree(ROOT / 'Source-Build', work,
                            ignore=shutil.ignore_patterns('build', 'docs', '__pycache__', '.DS_Store'))
            (work / 'docs').mkdir(exist_ok=True)
            source_before = inventory(work)
            flags = [k + '=' + str(v) for k, v in CONFIG['flags'].items()]
            command = ['make', '-j2', 'HOST_CPUS=2', 'OTHER_CHIP_IDS=', *flags]
            run([*command, 'kext'], output / 'backend-build.log', work)
            run([*command, 'front'], output / 'frontend-build.log', work)
            driver = work / 'build/out/AirPort_RTW89.kext'
            front = work / 'build/out/AirPortRTW89Front.kext'
            run(['python3', 'tools/check_kpi.py', str(driver)], output / 'backend-kpi.log', work)
            legacy_check = ['python3', 'tools/check_kpi_legacy.py', str(front),
                            '--legacy-catalog', str(work / 'tools/owly-legacy4c-exports.json')]
            if args.legacy_image:
                legacy_check += ['--legacy-image', str(args.legacy_image.resolve()),
                                 '--allow-unloaded-legacy']
            run(legacy_check, output / 'frontend-kpi.log', work)
            import check_p2p_abi
            report['p2p_abi'] = check_p2p_abi.run(executable(front), output / 'p2p-abi.json')
            run(['codesign', '--force', '--sign', '-', '--timestamp=none', str(front)],
                output / 'frontend-sign.log')
            bridge = bridge_build(work, output, uuid(executable(front)))
            require(all(sha(work / name) == value for name, value in source_before.items()),
                    'Source changed during build')
            assemble(work, bridge, output, report)
    except Exception as error:
        report.update(status='BUILD_FAILED', error=str(error))
        for folder in ('assembly', 'VERIFIED_KEXTS'):
            shutil.rmtree(output / folder, ignore_errors=True)
        (output / 'RTLAirLink-Fix46-x86_64.zip').unlink(missing_ok=True)
        raise
    finally:
        (output / 'BUILD_REPORT.json').write_text(json.dumps(report, indent=2) + '\n')
        print('Report: ' + str(output / 'BUILD_REPORT.json'), flush=True)
    print(report['status'], flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
