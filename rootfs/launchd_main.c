/* Minimal freestanding PID 1 for the XNU RAM-disk root bring-up.
 * No libc: every interface is a direct XNU syscall (svc #0x80, number in x16).
 */
typedef unsigned long u64;
typedef long s64;

static long svc3(long num, long a, long b, long c)
{
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x16 __asm__("x16") = num;
	__asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16) : "memory");
	return x0;
}

static long sys_write(long fd, const void *buf, u64 n) { return svc3(4, fd, (long)buf, (long)n); }
static long sys_read(long fd, void *buf, u64 n) { return svc3(3, fd, (long)buf, (long)n); }
static long sys_open(const char *p, long flags, long mode) { return svc3(5, (long)p, flags, mode); }
static long sys_close(long fd) { return svc3(6, fd, 0, 0); }
static long sys_mount(const char *type, const char *dir, long flags, void *data) { return svc3(167, (long)type, (long)dir, flags); }
static long sys_getpid(void) { return svc3(20, 0, 0, 0); }

static void write_str(long fd, const char *s)
{
	u64 n = 0;
	while (s[n]) {
		n++;
	}
	sys_write(fd, s, n);
}

static void write_num(long fd, long v)
{
	char buf[24];
	int i = 0;
	if (v < 0) {
		sys_write(fd, "-", 1);
		v = -v;
	}
	if (v == 0) {
		buf[i++] = '0';
	} else {
		while (v > 0 && i < (int)sizeof(buf)) {
			buf[i++] = (char)('0' + (v % 10));
			v /= 10;
		}
	}
	while (i > 0) {
		sys_write(fd, &buf[--i], 1);
	}
}

void main_user(void)
{
	char c = 0;
	long con, r;

	sys_mount("devfs", "/dev", 0, (void *)0);
	/* O_RDWR | O_NONBLOCK: the phone has no console input device, so a blocking
	 * read would park pid 1 forever; we only want to exercise the syscall. */
	con = sys_open("/dev/console", 6, 0);
	if (con < 0) {
		con = 1;
	}

	write_str(con, "hello from userspace on Xiaomi Mi 6 (XNU ramdisk root)\n");
	write_str(con, "launchd stub: pid=");
	write_num(con, sys_getpid());
	write_str(con, " console fd=");
	write_num(con, con);
	write_str(con, "\n");

	/* Prove the HFS+ root is readable from userspace: open our own binary. */
	{
		long self = sys_open("/sbin/launchd", 0 /* O_RDONLY */, 0);
		unsigned char hdr[8] = { 0 };
		long n = -1;
		if (self >= 0) {
			n = sys_read(self, hdr, sizeof(hdr));
			sys_close(self);
		}
		write_str(con, "launchd stub: self-open=");
		write_num(con, self);
		write_str(con, " n=");
		write_num(con, n);
		write_str(con, " hdr=");
		for (int i = 0; i < 4; i++) {
			static const char hexd[] = "0123456789abcdef";
			char b[2] = { hexd[(hdr[i] >> 4) & 15], hexd[hdr[i] & 15] };
			sys_write(con, b, 2);
		}
		write_str(con, "\n");
	}

	r = sys_read(con, &c, 1);
	write_str(con, "launchd stub: read(/dev/console) -> ");
	write_num(con, r);
	write_str(con, "\n");

	write_str(con, "launchd stub: staying alive as pid 1\n");
	sys_close(con);

	/* PID 1 must never exit: sleep in a loop. */
	for (;;) {
		static long ts[2] = { 1, 0 };
		svc3(240, (long)&ts[0], 0, 0);
	}
}
