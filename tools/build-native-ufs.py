#!/usr/bin/env python3
"""Build the device-local MSM8998 UFS driver against the source-kernel ABI.

The driver lives in this device repository (drivers/NativeUfs, ported from the
OnePlus 5 port's read-only warm-link adapter).  It is compiled with the same
recipe as the kernel's own IOPlatformExpert.cpp object so that the C++
ABI, the `-fapple-kext` dialect and the kalloc type checks match exactly.
"""
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]          # the assembled tree
DEVICE = Path(os.environ.get('UNIXNU_DEVICE') or ROOT / 'package' / 'device')
SRC = DEVICE / 'drivers' / 'NativeUfs' / 'NativeUfs.cpp'
KITS = Path(os.environ.get('UNIXNU_SRC', str(ROOT))).parent / 'UniXNU' / 'kits'

if not SRC.is_file():
    sys.exit(f'no driver source at {SRC} (set UNIXNU_DEVICE)')

recipe = ROOT / 'build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/iokit/DEVELOPMENT/IOPlatformExpert.cpo.json'
record = json.loads(recipe.read_text().strip().rstrip(','))
out = ROOT / 'build/native-ufs'
out.mkdir(parents=True, exist_ok=True)
destination = out / 'NativeUfs.o'

command = []
index = 0
arguments = record['arguments']
while index < len(arguments):
    value = arguments[index]
    if value == record['file']:
        command.append(str(SRC))
    elif value == '-o':
        command.extend(['-o', str(destination)])
        index += 1
    elif value == '-Xclang' and index + 1 < len(arguments) and arguments[index + 1].startswith('-W'):
        index += 1
    elif not value.startswith('-W'):
        command.append(value)
    index += 1

command += [
    '-I', str(ROOT / 'build/storage-abi/include'),
    '-I', str(SRC.parent),
    '-I', str(KITS / 'VerifiedRoot/include'),
    '-I', str(KITS / 'VerifiedRoot'),
    '-DNATIVE_VERIFIED_BLOCK=1', '-mgeneral-regs-only',
    '-Wall', '-Wextra', '-Wno-unused-parameter', '-Wno-c99-designator',
]
(out / 'compile-command.json').write_text(json.dumps(command, indent=2) + '\n')
result = subprocess.run(command, cwd=record['directory'], capture_output=True, text=True)
(out / 'compile.log').write_text((result.stdout or '') + (result.stderr or ''))
if result.returncode:
    sys.exit(((result.stdout or '') + (result.stderr or ''))[-3500:])

archive = out / 'libNativeUfs.a'
archive.unlink(missing_ok=True)
subprocess.run(['xcrun', 'ar', 'rcs', str(archive), str(destination)], check=True)
print(f'NativeUfs: {destination.stat().st_size} bytes compiled into {archive.name}')
