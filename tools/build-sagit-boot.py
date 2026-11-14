#!/usr/bin/env python3
"""Assemble an Android boot image for sagit: EDK2 firmware + UniXNU payload.

The firmware section (gzip of the EDK2 FV that ABL loads) is taken from a base
image and never changes while iterating on the XNU payload. The ramdisk section
is the LZMA-wrapped FAT payload produced by mksagit-payload.py --lzma.
"""
import argparse
import struct
from pathlib import Path

BASE_FIELDS = '<8sIIIIIIII'  # magic, kernel, kernel_addr, ramdisk, ramdisk_addr, second, second_addr, tags, page


def sections(image: bytes):
    magic, ksize, kaddr, rsize, raddr, ssize, saddr, tags, page = struct.unpack_from(BASE_FIELDS, image, 0)
    if magic != b'ANDROID!':
        raise SystemExit('base image is not an Android boot image')
    kstart = page
    rstart = kstart + ((ksize + page - 1) // page) * page
    return {
        'page': page, 'kaddr': kaddr, 'raddr': raddr, 'tags': tags,
        'kernel': image[kstart:kstart + ksize],
        'name': image[36:52], 'cmdline': image[64:64 + 512], 'id': image[576:608],
        'extra': image[608:608 + 1024],
    }


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--base', type=Path, required=True, help='earlier boot image with the firmware section')
parser.add_argument('--firmware-gz', type=Path, help='new firmware gzip (FV); the Android DTB is appended to it')
parser.add_argument('--dtb', type=Path, help='Android-sagit.dtb to append after the firmware gzip')
parser.add_argument('--ramdisk', type=Path, required=True, help='payload .img.lzma')
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()

base = sections(args.base.read_bytes())
firmware = base['kernel']
if args.firmware_gz:
    if not args.dtb:
        raise SystemExit('--firmware-gz also needs --dtb (the Android DTB follows the gzip)')
    firmware = args.firmware_gz.read_bytes() + args.dtb.read_bytes()
    base['kernel'] = firmware
ramdisk = args.ramdisk.read_bytes()
header = struct.pack(BASE_FIELDS, b'ANDROID!', len(base['kernel']), base['kaddr'], len(ramdisk),
                     base['raddr'], 0, 0, base['tags'], base['page'])
header += base['name'] + base['cmdline'] + base['id'] + base['extra']
page = base['page']


def padded(blob):
    return blob + b'\0' * ((-len(blob)) % page)


args.output.write_bytes(padded(header) + padded(base['kernel']) + padded(ramdisk))
print(f"{args.output}: {args.output.stat().st_size} bytes "
      f"(firmware {len(base['kernel'])}, payload {len(ramdisk)}, page {page})")
