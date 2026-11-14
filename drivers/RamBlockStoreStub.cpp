// SPDX-License-Identifier: BSD-2-Clause-Patent
// LOCAL: no persistent store on this board yet. RamBlock declares these as
// weak imports; providing "no store" answers lets it publish the packed RAM
// image as a block device without a UFS backend.
extern "C" {
unsigned native_ufs_store_mode (void) { return 0; }
int native_ufs_store_open (unsigned long long, unsigned long long *, const char **why)
{
  if (why) *why = "no persistent store on sagit (RAM-only bring-up)";
  return -1;
}
int native_ufs_store_read (unsigned long long, unsigned, void *) { return -1; }
int native_ufs_store_write (unsigned long long, unsigned, const void *) { return -1; }
int native_ufs_store_write_cached (unsigned long long, unsigned, const void *) { return -1; }
int native_ufs_store_sync (void) { return -1; }
unsigned native_ufs_store_gpt_check (void) { return 0; }
/* RamBlock only calls this when boot-payload-release=1; the source kernel does
 * not export it, so answer "cannot release" and keep the seed resident. */
int ml_static_mfree_prepare (void *address, unsigned length) { (void)address; (void)length; return 0; }
}
