#!/usr/bin/env bash
# Assemble a flashable sagit boot image from the current kernel build.
#
#   tools/build-image.sh out/v10 [--base out/v9/boot-sagit.img] [--adt out/v8/kernel.macho.adt]
#
# Steps: strip the prelinked kernel, reuse or build the ADT, pack the LZMA FAT
# payload, then splice the new ramdisk into a base boot image that already
# carries the EDK2 firmware section (tools/build-sagit-boot.py).
set -euo pipefail
OUT=${1:?usage: build-image.sh <out/version> [--base IMG] [--adt ADT]}; shift || true
BASE=out/v6/boot-sagit.img
ADT=
while [ $# -gt 0 ]; do
  case "$1" in
    --base) BASE=$2; shift 2 ;;
    --adt)  ADT=$2;  shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
TREE=UniXNU/out/sagit/ios-16.7.16-20H392/sagit/tree
KERNEL=$TREE/build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/mach.development.bcm2837.unstripped.noctf
PRE=$TREE/../kernel/kernel.macho
export PATH=/usr/sbin:$PATH

mkdir -p "$OUT"
python3 unixnu-sagit/tools/strip-prelinked-kernel.py "$PRE" "$OUT/kernel.stripped.macho"

if [ -z "$ADT" ]; then
  ADT=$OUT/kernel.macho.adt
  UNIXNU_SRC=${UNIXNU_SRC:-$ROOT/UniXNU} python3 unixnu-sagit/tools/make-sagit-adt.py \
    --output "$ADT" --unique-chip-id 0x9f3c1d5e7a2b4c68 --keep-dtb "$OUT"
fi

python3 unixnu-sagit/tools/mksagit-payload.py \
  --kernel "$OUT/kernel.stripped.macho" --adt "$ADT" --output "$OUT/payload.img" \
  --size-mib 44 --args "-v -enable_kprintf_spam debug=0x14e" --lzma
python3 unixnu-sagit/tools/build-sagit-boot.py \
  --base "$BASE" --ramdisk "$OUT/payload.img.lzma" --output "$OUT/boot-sagit.img"

python3 - "$OUT" "$KERNEL" <<'PY'
import hashlib, json, sys
from pathlib import Path
out = Path(sys.argv[1]); kernel = Path(sys.argv[2])
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
manifest = {name: sha(out / name) for name in
            ('boot-sagit.img', 'payload.img.lzma', 'kernel.stripped.macho')
            if (out / name).is_file()}
for name in ('kernel.macho.adt', 'hardware.dtb'):
    if (out / name).is_file():
        manifest[name] = sha(out / name)
manifest['source_prelinked_kernel'] = sha(kernel)
(out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print('image:', out / 'boot-sagit.img')
for key, value in manifest.items():
    print(f'  {key}: {value}')
PY
