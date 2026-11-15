/* Child side of the IPC test: read the file the parent wrote and verify it. */
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
static const char expect[] = "IPC_FROM_PARENT";
void main_user(void)
{
	static char buf[64];
	long fd, n, i, ok = 1;

	con = svc3(5, (long)"/dev/console", 6, 0);
	if (con < 0) {
		con = 1;
	}
	fd = svc3(5, (long)"/sagit-ipc", 0, 0);
	n = fd >= 0 ? svc3(3, fd, (long)buf, (long)sizeof(buf) - 1) : -1;
	if (fd >= 0) {
		svc3(6, fd, 0, 0);
	}
	if (n > 0) {
		buf[n] = 0;
	}
	say("ipcchild: fd=");
	say_num(fd);
	say(" read=");
	say_num(n);
	say(" data=");
	say(n > 0 ? buf : "?");
	say("\n");
	if (n != (long)sizeof(expect) - 1) {
		ok = 0;
	} else {
		for (i = 0; i < n; i++) {
			if (buf[i] != expect[i]) {
				ok = 0;
			}
		}
	}
	say("ipcchild: verify ");
	say(ok ? "ok" : "MISMATCH");
	say("\n");
	svc3(1, ok ? 0 : 2, 0, 0);
	for (;;) {
	}
}
