/* Query the framebuffer geometry through the read-only kern.sagitfbinfo sysctl. */
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
static void say_hex(u64 v)
{
	char b[20];
	int i = 0;
	static const char hx[] = "0123456789abcdef";
	if (v == 0) {
		say("0x0");
		return;
	}
	while (v > 0 && i < (int)sizeof(b)) {
		b[i++] = hx[v & 15];
		v >>= 4;
	}
	say("0x");
	while (i > 0) {
		svc3(4, con, (long)&b[--i], 1);
	}
}

struct fb_info {
	unsigned int width;
	unsigned int height;
	unsigned int depth;
	unsigned int rowbytes;
	unsigned long physaddr;
};

void main_user(void)
{
	static const char name[] = "kern.sagitfbinfo";
	static struct fb_info info;
	u64 len = sizeof(info);
	long rc;

	con = svc3(5, (long)"/dev/console", 6, 0);
	if (con < 0) {
		con = 1;
	}
	rc = svc6(274 /* sysctlbyname */, (long)name, (long)sizeof(name) - 1,
	    (long)&info, (long)&len, 0, 0);
	say("fbinfo: rc=");
	say_num(rc);
	if (rc == 0) {
		say(" size=");
		say_num((long)len);
		say(" ");
		say_num(info.width);
		say("x");
		say_num(info.height);
		say(" depth=");
		say_num(info.depth);
		say(" rowbytes=");
		say_num(info.rowbytes);
		say(" phys=");
		say_hex(info.physaddr);
	}
	say("\n");
	svc3(1, rc == 0 ? 0 : 1, 0, 0);
	for (;;) {
	}
}
