/* Parent side of the IPC test: write a file, spawn the child, wait for it. */
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
static long svc6(long num, long a, long b, long c, long d, long e, long f)
{
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	register long x4 __asm__("x4") = e;
	register long x5 __asm__("x5") = f;
	register long x16 __asm__("x16") = num;
	__asm__ volatile("svc #0x80\n b.cc 1f\n neg x0, x0\n1:"
	    : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x16) : "memory");
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
static const char payload[] = "IPC_FROM_PARENT";
void main_user(void)
{
	static unsigned long mnt_data[16];
	static char *argv[2];
	int cpid = -1;
	long m, fd, w, r;
	int st = 0;

	con = svc3(5, (long)"/dev/console", 6, 0);
	if (con < 0) {
		con = 1;
	}
	m = svc4(167, (long)"hfs", (long)"/", 0x10000, (long)mnt_data);
	say("ipctest: remount rw rc=");
	say_num(m);
	say("\n");

	fd = svc4(5, (long)"/sagit-ipc", 0x202, 0644, 0);
	w = fd >= 0 ? svc3(4, fd, (long)payload, (long)sizeof(payload) - 1) : -1;
	if (fd >= 0) {
		svc3(6, fd, 0, 0);
	}
	say("ipctest: wrote=");
	say_num(w);
	say("\n");

	argv[0] = (char *)"/sbin/ipcchild";
	argv[1] = 0;
	r = svc6(244, (long)&cpid, (long)"/sbin/ipcchild", 0, 0, (long)argv, 0);
	if (r != 0) {
		say("ipctest: spawn failed errno=");
		say_num(-r);
		say("\n");
		svc3(1, 1, 0, 0);
	}
	w = svc4(7, cpid, (long)&st, 0, 0);
	say("ipctest: child pid=");
	say_num(cpid);
	say(" wait=");
	say_num(w);
	say(" exit=");
	say_num((st >> 8) & 0xff);
	say("\n");
	svc3(1, ((st >> 8) & 0xff) == 0 ? 0 : 3, 0, 0);
	for (;;) {
	}
}
