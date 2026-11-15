/* HFS+ directory operation test: mkdir, rename, rmdir, unlink. */
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
static void check(const char *what, long rc)
{
	say("dtest: ");
	say(what);
	say(" rc=");
	say_num(rc);
	say("\n");
}

void main_user(void)
{
	static unsigned long mnt_data[16];
	long m, rc, fd, fails = 0;

	con = svc3(5, (long)"/dev/console", 6, 0);
	if (con < 0) {
		con = 1;
	}
	m = svc4(167, (long)"hfs", (long)"/", 0x10000, (long)mnt_data);
	check("remount rw", m);

	rc = svc3(136 /* mkdir */, (long)"/sagitdir", 0755, 0);
	check("mkdir /sagitdir", rc);
	if (rc) {
		fails++;
	}

	rc = svc3(128 /* rename */, (long)"/sagitdir", (long)"/sagitdir2", 0);
	check("rename -> /sagitdir2", rc);
	if (rc) {
		fails++;
	}

	/* Create a file inside the renamed directory, then remove it. */
	fd = svc4(5, (long)"/sagitdir2/f1", 0x202 /* O_RDWR|O_CREAT */, 0644, 0);
	check("create /sagitdir2/f1", fd);
	if (fd >= 0) {
		svc3(6, fd, 0, 0);
	} else {
		fails++;
	}
	rc = svc3(10 /* unlink */, (long)"/sagitdir2/f1", 0, 0);
	check("unlink f1", rc);
	if (rc) {
		fails++;
	}

	rc = svc3(137 /* rmdir */, (long)"/sagitdir2", 0, 0);
	check("rmdir /sagitdir2", rc);
	if (rc) {
		fails++;
	}

	say("dtest: result=");
	say_num(fails);
	say("\n");
	svc3(1, fails, 0, 0);
	for (;;) {
	}
}
