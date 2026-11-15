/* A service that always fails, used to exercise init's restart logic. */
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
static void write_str(long fd, const char *s)
{
	u64 n = 0;
	while (s[n]) {
		n++;
	}
	svc3(4, fd, (long)s, (long)n);
}
void main_user(void)
{
	long con = svc3(5, (long)"/dev/console", 6, 0);
	if (con < 0) {
		con = 1;
	}
	write_str(con, "crasher: failing on purpose (exit 3)\n");
	svc3(1, 3, 0, 0);
	for (;;) {
	}
}
