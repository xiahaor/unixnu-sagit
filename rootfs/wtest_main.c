/* HFS+ write test: create a file on the RAM-disk root, write to it, read it
 * back and print the result. Direct syscalls only (no libc).
 */
typedef unsigned long u64;
static long svc3(long num, long a, long b, long c)
{
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x16 __asm__("x16") = num;
	__asm__ volatile("svc #0x80\n b.cc 1f\n neg x0, x0\n1:" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16) : "memory");
	return x0;
}
static long svc4(long num, long a, long b, long c, long d)
{
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	register long x16 __asm__("x16") = num;
	__asm__ volatile("svc #0x80\n b.cc 1f\n neg x0, x0\n1:" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x16) : "memory");
	return x0;
}
static long con = 1;
static void say(const char *s)
{
	u64 n = 0;
	while (s[n]) {
		n++;
	}
	svc3(4, con, (long)s, (long)n);
}
static void say_num(long v)
{
	char b[24];
	int i = 0;
	if (v < 0) {
		svc3(4, con, (long)"-", 1);
		v = -v;
	}
	if (v == 0) {
		b[i++] = '0';
	}
	while (v > 0 && i < (int)sizeof(b)) {
		b[i++] = (char)('0' + (v % 10));
		v /= 10;
	}
	while (i > 0) {
		svc3(4, con, (long)&b[--i], 1);
	}
}

static const char payload[] = "SAGIT_HFS_WRITE_OK";
static char rbuf[64];

void main_user(void)
{
	long fd, w, r, rc;

	con = svc3(5, (long)"/dev/console", 6, 0);
	if (con < 0) {
		con = 1;
	}
	/* The kernel mounts the root read-only (MNT_RDONLY|MNT_ROOTFS); remount
	 * it read-write the same way a real init does with "mount -uw /". */
	{
		static unsigned long mnt_data[16]; /* hfs_mount_args is copied in by the kernel */
		long m = svc4(167 /* mount */, (long)"hfs", (long)"/", 0x10000 /* MNT_UPDATE */, (long)mnt_data);
		say("wtest: remount rw rc=");
		say_num(m);
		say("\n");
	}

	say("wtest: creating /sagit-write-test\n");

	/* O_RDWR=2 | O_CREAT=0x200 */
	fd = svc4(5, (long)"/sagit-write-test", 0x202, 0644, 0);
	say("wtest: create fd=");
	say_num(fd);
	say("\n");
	if (fd < 0) {
		svc3(1, 1, 0, 0);
	}

	w = svc3(4, fd, (long)payload, (long)sizeof(payload) - 1);
	say("wtest: write=");
	say_num(w);
	say("\n");
	svc3(6, fd, 0, 0);

	fd = svc3(5, (long)"/sagit-write-test", 0 /* O_RDONLY */, 0);
	r = fd >= 0 ? svc3(3, fd, (long)rbuf, (long)sizeof(rbuf) - 1) : -1;
	if (fd >= 0) {
		svc3(6, fd, 0, 0);
	}
	if (r > 0) {
		rbuf[r] = 0;
	}
	say("wtest: read=");
	say_num(r);
	say(" data=");
	say(r > 0 ? rbuf : "?");
	say("\n");

	/* Compare and exit 0 on success, 2 on mismatch, 3 on read failure. */
	rc = 3;
	if (r == (long)sizeof(payload) - 1) {
		long i, same = 1;
		for (i = 0; i < r; i++) {
			if (rbuf[i] != payload[i]) {
				same = 0;
			}
		}
		rc = same ? 0 : 2;
	}
	say("wtest: result=");
	say_num(rc);
	say("\n");
	svc3(1, rc, 0, 0);
	for (;;) {
	}
}
