#!/usr/bin/env python3
"""Bind the device-local UFS driver's IOStorageFamily imports to the prelinked originals.

The native kernel is linked without the prelinked IOStorageFamily kext, so the
driver's IOBlockStorageDevice imports must resolve to the addresses that class
has in the final prelink image: original export address + profile slide.
The emulated `.globl/.set` bindings keep the native link honest and the prelink
then carries both the original kext code and our driver.
"""
import argparse
import json
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'kits' / 'XnuPrelink'))
import native_prelink as np

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--object', type=Path, required=True)
parser.add_argument('--kernelcache', type=Path, required=True)
parser.add_argument('--slide', type=lambda v: int(v, 0), required=True)
parser.add_argument('--archive', type=Path, required=True)
args = parser.parse_args()

cache = args.kernelcache.read_bytes()
exports = np.exports(cache, np.macho(cache))
by_name = {name: va for va, names in exports.items() for name in names}

# Undefined symbols of the driver object that no source kernel defines.
nm = subprocess.run(['xcrun', 'nm', '-u', str(args.object)], capture_output=True, text=True)
if nm.returncode:
    sys.exit(nm.stderr[-2000:])
undefined = [line.split()[-1] for line in nm.stdout.splitlines() if line.strip()]

# Only IOStorageFamily-provided classes need prelink bindings; everything else
# is a real kernel export and must stay a normal undefined reference.
pattern = re.compile(r'^__Z(TV|N|NK)20IO(BlockStorageDevice|Media|Storage)')
needed = [name for name in undefined if pattern.match(name)]
missing = [name for name in needed if name not in by_name]
if missing:
    sys.exit('no export in the kernelcache for: ' + ', '.join(missing))

bindings = {name: by_name[name] + args.slide for name in needed}
assembly = args.object.with_name('original-bindings.S')
assembly.write_text(''.join(f'.globl {name}\n.set {name}, {value:#x}\n' for name, value in sorted(bindings.items())))
subprocess.run(['xcrun', 'clang', '-target', 'arm64-apple-ios16.0', '-c', str(assembly),
                '-o', str(args.object.with_name('original-bindings.o'))], check=True)

args.archive.unlink(missing_ok=True)
subprocess.run(['xcrun', 'ar', 'rcs', str(args.archive), str(args.object),
                str(args.object.with_name('original-bindings.o'))], check=True)
report = args.archive.with_name('bindings.json')
report.write_text(json.dumps({'slide': args.slide, 'bindings': {k: hex(v) for k, v in bindings.items()}},
                            indent=2) + '\n')
print(f'NativeUfs: bound {len(bindings)} original storage imports into {args.archive.name}')
