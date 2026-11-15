/* Anonymous mmap test: map a page, use it, unmap it. */
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

void main_user(void)
{
	long addr, rc, i, ok = 1;
	volatile unsigned char *p;
	static const unsigned char pattern[8] = { 'S','A','G','I','T','M','M','P' };

	con = svc3(5, (long)"/dev/console", 6, 0);
	if (con < 0) {
		con = 1;
	}

	/* mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0) */
	addr = svc6(197 /* mmap */, 0, 4096, 3 /* R|W */, 0x1002 /* PRIVATE|ANON */, -1, 0);
	say("mtest: mmap -> ");
	say_num(addr);
	say("\n");
	if (addr < 0) {
		svc3(1, 1, 0, 0);
	}

	p = (volatile unsigned char *)addr;
	for (i = 0; i < 8; i++) {
		p[i] = pattern[i];
	}
	for (i = 0; i < 8; i++) {
		if (p[i] != pattern[i]) {
			ok = 0;
		}
	}
	say("mtest: pattern ");
	say(ok ? "ok" : "MISMATCH");
	say(" data=");
	svc3(4, con, addr, 8);
	say("\n");

	rc = svc3(73 /* munmap */, addr, 4096, 0);
	say("mtest: munmap rc=");
	say_num(rc);
	say("\n");
	say("mtest: result=");
	say_num(ok ? 0 : 2);
	say("\n");
	svc3(1, ok ? 0 : 2, 0, 0);
	for (;;) {
	}
}
