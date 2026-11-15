/* List the HFS+ root directory from userspace with getdirentries(). */
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
static int name_is(const char *name, int len, const char *want)
{
	int i = 0;
	while (want[i]) {
		i++;
	}
	if (i != len) {
		return 0;
	}
	for (i = 0; i < len; i++) {
		if (name[i] != want[i]) {
			return 0;
		}
	}
	return 1;
}

void main_user(void)
{
	static char buf[1024];
	static long base;
	long fd, n, count = 0, saw_sbin = 0, saw_write = 0;
	int round;

	con = svc3(5, (long)"/dev/console", 6, 0);
	if (con < 0) {
		con = 1;
	}

	fd = svc3(5, (long)"/", 0, 0);
	say("ltest: open / -> ");
	say_num(fd);
	say("\n");
	if (fd < 0) {
		svc3(1, 1, 0, 0);
	}

	base = 0;
	for (round = 0; round < 4; round++) {
		n = svc4(344 /* getdirentries64 */, fd, (long)buf, (long)sizeof(buf), (long)&base);
		if (n < 0) {
			break;
		}
		say("ltest: getdirentries64 -> ");
		say_num(n);
		say("\n");
		if (n == 0) {
			break;
		}
		{
			long off = 0;
			while (off + 22 <= n) {
				unsigned reclen, namlen;
				const char *name;
				/* 64-bit dirent: ino(8) seekoff(8) reclen(2) namlen(2) type(1) name */
				__builtin_memcpy(&reclen, buf + off + 16, 2);
				__builtin_memcpy(&namlen, buf + off + 18, 2);
				reclen &= 0xffff;
				namlen &= 0xffff;
				if (reclen < 22 || off + (long)reclen > n || namlen > 255) {
					break;
				}
				name = buf + off + 21;
				if (name_is(name, (int)namlen, "sbin")) {
					saw_sbin = 1;
				}
				if (name_is(name, (int)namlen, "sagit-write-test")) {
					saw_write = 1;
				}
				if (count < 8) {
					say("ltest: entry ");
					svc3(4, con, (long)name, namlen < 24 ? namlen : 24);
					say("\n");
				}
				count++;
				off += reclen;
			}
		}
	}
	svc3(6, fd, 0, 0);

	say("ltest: entries=");
	say_num(count);
	say(" sbin=");
	say_num(saw_sbin);
	say(" writefile=");
	say_num(saw_write);
	say("\n");
	svc3(1, (saw_sbin && saw_write) ? 0 : 2, 0, 0);
	for (;;) {
	}
}
