/* Userspace framebuffer test: fill coloured rectangles through kern.sagitfb. */
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

struct fb_req {
	unsigned int x, y, w, h, color;
};

static long fb_fill(unsigned x, unsigned y, unsigned w, unsigned h, unsigned color)
{
	static struct fb_req r;
	r.x = x;
	r.y = y;
	r.w = w;
	r.h = h;
	r.color = color;
	static const char name[] = "kern.sagitfb";
	return svc6(274 /* sysctlbyname */, (long)name, (long)sizeof(name) - 1,
	    0, 0, (long)&r, (long)sizeof(r));
}

void main_user(void)
{
	long r1, r2, r3;

	con = svc3(5, (long)"/dev/console", 6, 0);
	if (con < 0) {
		con = 1;
	}

	/* Three bands low on the 1080x1920 panel: red, green, blue. */
	r1 = fb_fill(80, 1050, 920, 180, 0x00FF0000);
	r2 = fb_fill(80, 1280, 920, 180, 0x0000FF00);
	r3 = fb_fill(80, 1510, 920, 180, 0x000000FF);
	say("fbtest: red rc=");
	say_num(r1);
	say(" green rc=");
	say_num(r2);
	say(" blue rc=");
	say_num(r3);
	say("\n");
	say("fbtest: done\n");
	svc3(1, (r1 == 0 && r2 == 0 && r3 == 0) ? 0 : 1, 0, 0);
	for (;;) {
	}
}
