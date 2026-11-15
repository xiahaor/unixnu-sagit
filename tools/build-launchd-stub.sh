#!/usr/bin/env bash
# Build the freestanding PID 1 used as /sbin/launchd on the RAM-disk root.
# It is a static arm64 Mach-O with no libc: every interface is a direct XNU
# syscall (svc #0x80, number in x16). Used to prove userspace works end-to-end.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
TOOLCHAIN=${TOOLCHAIN:-$(cd "$HERE/../toolchains/unixnu-linux" && pwd)}
export PATH="$TOOLCHAIN/llvm/bin:$TOOLCHAIN/CommandLineTools/usr/bin:$PATH"
SRC=$HERE/rootfs
OUT=${1:-$HERE/out/rootfs}
mkdir -p "$OUT"
clang -target arm64-apple-ios16.0 -c "$SRC/launchd_entry.s" -o "$OUT/launchd_entry.o"
clang -target arm64-apple-ios16.0 -ffreestanding -fno-builtin -fno-stack-protector -O2 \
    -c "$SRC/launchd_main.c" -o "$OUT/launchd_main.o"
ld -arch arm64 -static -platform_version ios 16.0 16.0 -e _start \
    -o "$OUT/launchd" "$OUT/launchd_entry.o" "$OUT/launchd_main.o"
echo "built $OUT/launchd ($(stat -c%s "$OUT/launchd") bytes)"
# Second userspace program used to validate fork()+execve()+wait4().
clang -target arm64-apple-ios16.0 -ffreestanding -fno-builtin -fno-stack-protector -O2 \
    -c "$SRC/hello_main.c" -o "$OUT/hello_main.o"
ld -arch arm64 -static -platform_version ios 16.0 16.0 -e _start \
    -o "$OUT/hello" "$OUT/launchd_entry.o" "$OUT/hello_main.o"
echo "built $OUT/hello ($(stat -c%s "$OUT/hello") bytes)"

# Extra init services for the minimal-init experiment.
for prog in svc2 heartbeat wtest dtest; do
  case $prog in svc2) srcname=svc_main.c;; wtest) srcname=wtest_main.c;; dtest) srcname=dtest_main.c;; *) srcname=${prog}_main.c;; esac
  clang -target arm64-apple-ios16.0 -ffreestanding -fno-builtin -fno-stack-protector -O2 \
      -c "$SRC/$srcname" -o "$OUT/${prog}_main.o"
  ld -arch arm64 -static -platform_version ios 16.0 16.0 -e _start \
      -o "$OUT/$prog" "$OUT/launchd_entry.o" "$OUT/${prog}_main.o"
  echo "built $OUT/$prog ($(stat -c%s "$OUT/$prog") bytes)"
done
