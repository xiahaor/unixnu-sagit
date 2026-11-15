/* Heartbeat service: prints one tick and exits 0. */
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
static void write_num(long fd, long v)
{
	char b[24];
	int i = 0;
	if (v == 0) {
		b[i++] = '0';
	}
	while (v > 0 && i < (int)sizeof(b)) {
		b[i++] = (char)('0' + (v % 10));
		v /= 10;
	}
	while (i > 0) {
		svc3(4, fd, (long)&b[--i], 1);
	}
}
void main_user(void)
{
	long con = svc3(5, (long)"/dev/console", 6, 0);
	if (con < 0) {
		con = 1;
	}
	write_str(con, "heartbeat: tick from pid=");
	write_num(con, svc3(20, 0, 0, 0));
	write_str(con, "\n");
	svc3(1, 0, 0, 0);
	for (;;) {
	}
}
