/** @file
  Copy the previous XNU boot's console log out of the reserved DRAM page.

  The XNU console mirror (kernel patch 0033) leaves "SAGITLOG" + length + text
  at physical 0x85800000, a removed DRAM window.  This driver runs once UFS
  block devices exist, finds the boot partition by its ANDROID! magic and
  writes a record into that partition's unused tail (32 MiB in), where the
  recovery shell reads it back with dd.  Nothing on the boot path reads that
  tail: ABL only consumes the size the boot image header declares.

  The record always starts with "LOGDXE1" so a zero tail is distinguishable
  from "the driver never ran".  Byte 8 is 1 when the XNU page had a valid
  magic; bytes 16.. are the log text (or the raw page head when it did not).
**/
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/BlockIo.h>

#define SAGIT_LOG_PHYS    ((UINTN)0x85800000ULL)
#define SAGIT_LOG_MAX     0x8000U
#define SAGIT_PROBE_BYTES 64U
#define SAGIT_DUMP_OFFSET 0x2000000ULL

EFI_STATUS
EFIAPI
LogDumpDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  STATIC CONST UINT8 Magic[8] = { 'S', 'A', 'G', 'I', 'T', 'L', 'O', 'G' };
  CONST UINT8          *Page;
  BOOLEAN              HaveMagic;
  UINT32               Length;
  UINT8                *Record;
  UINTN                RecordSize;
  EFI_STATUS           Status;
  EFI_HANDLE           *Handles;
  UINTN                Count;
  UINTN                Index;
  EFI_BLOCK_IO_PROTOCOL *BlockIo;
  EFI_BLOCK_IO_MEDIA   *Media;
  UINTN                Blocks;
  UINT8                *Buffer;

  Page = (CONST UINT8 *)SAGIT_LOG_PHYS;
  HaveMagic = (CompareMem (Page, Magic, 8) == 0);
  Length = HaveMagic ? *(volatile UINT32 *)(SAGIT_LOG_PHYS + 8) : 0;
  if ((Length == 0) || (Length >= SAGIT_LOG_MAX)) {
    Length = 0;
  }

  RecordSize = 16 + (Length ? Length : SAGIT_PROBE_BYTES);
  Record = AllocateZeroPool (RecordSize);
  if (Record == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  CopyMem (Record, "LOGDXE1", 7);
  Record[8] = HaveMagic ? 1 : 0;
  Record[9] = (UINT8)(Length & 0xff);
  if (Length) {
    *(UINT32 *)(Record + 12) = Length;
    CopyMem (Record + 16, Page, Length);
  } else {
    CopyMem (Record + 16, Page, SAGIT_PROBE_BYTES);
  }

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &Count, &Handles);
  if (EFI_ERROR (Status)) {
    FreePool (Record);
    return Status;
  }

  for (Index = 0; Index < Count; Index++) {
    Status = gBS->HandleProtocol (Handles[Index], &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);
    if (EFI_ERROR (Status) || (BlockIo->Media == NULL)) {
      continue;
    }
    Media = BlockIo->Media;
    if ((Media->BlockSize == 0) || (Media->LastBlock == 0)) {
      continue;
    }
    /* The boot partition is the only 64 MiB block device on this board; never
     * read it here, because a read on a not-yet-clocked controller can spin
     * forever during DXE dispatch. */
    if ((Media->LastBlock + 1) * (UINT64)Media->BlockSize != 64ULL * 1024 * 1024) {
      continue;
    }
    break;
  }
  gBS->FreePool (Handles);
  if (Index == Count) {
    FreePool (Record);
    return EFI_NOT_FOUND;
  }

  if (SAGIT_DUMP_OFFSET + RecordSize > (Media->LastBlock + 1) * (UINT64)Media->BlockSize) {
    FreePool (Record);
    return EFI_BAD_BUFFER_SIZE;
  }
  Blocks = (RecordSize + Media->BlockSize - 1) / Media->BlockSize;
  Buffer = AllocateZeroPool (Blocks * Media->BlockSize);
  if (Buffer == NULL) {
    FreePool (Record);
    return EFI_OUT_OF_RESOURCES;
  }
  CopyMem (Buffer, Record, RecordSize);
  Status = BlockIo->WriteBlocks (BlockIo, Media->MediaId,
                                 SAGIT_DUMP_OFFSET / Media->BlockSize,
                                 Blocks * Media->BlockSize, Buffer);
  FreePool (Buffer);
  FreePool (Record);
  DEBUG ((DEBUG_INFO, "LogDump: %u bytes (magic=%u) -> boot+0x%lx: %r\n",
          RecordSize, HaveMagic, (UINT64)SAGIT_DUMP_OFFSET, Status));
  return Status;
}
