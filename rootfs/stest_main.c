/* Signal test: start a long-running child, signal it, and decode wait status. */
typedef unsigned long u64;

/* There is no nanosleep syscall in Darwin; poll(0, 0, ms) blocks the calling
 * thread for the requested time without needing any file descriptors. */
static long sys_sleep(long sec, long nsec)
{
	long ms = sec * 1000 + nsec / 1000000;
	if (ms < 1 && (sec != 0 || nsec != 0)) {
		ms = 1;
	}
	{
		register long x0 __asm__("x0") = 0;
		register long x1 __asm__("x1") = 0;
		register long x2 __asm__("x2") = ms;
		register long x16 __asm__("x16") = 230; /* poll */
		__asm__ volatile("svc #0x80\n b.cc 1f\n neg x0, x0\n1:"
		    : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16) : "memory");
		return x0;
	}
}

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
static long spawn(const char *path)
{
	static char *argv[2];
	int cpid = -1;
	argv[0] = (char *)path;
	argv[1] = 0;
	{
		register long x0 __asm__("x0") = (long)&cpid;
		register long x1 __asm__("x1") = (long)path;
		register long x2 __asm__("x2") = 0;
		register long x3 __asm__("x3") = 0;
		register long x4 __asm__("x4") = (long)argv;
		register long x5 __asm__("x5") = 0;
		register long x16 __asm__("x16") = 244;
		__asm__ volatile("svc #0x80\n b.cc 1f\n neg x0, x0\n1:"
		    : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x16) : "memory");
		if (x0 != 0) {
			return x0;
		}
	}
	return (long)cpid;
}
static void test_signal(const char *name, long sig)
{
	long pid = spawn("/sbin/looper");
	int st = 0;
	long k, w;
	static long pause1[2] = { 1, 0 };
	if (pid < 0) {
		say("stest: spawn failed errno=");
		say_num(-pid);
		say("\n");
		return;
	}
	say("stest: spawned pid=");
	say_num(pid);
	say("\n");
	{
		/* Busy-wait instead of sleeping: the timer path is still being
		 * brought up, and a blocked sleep would stall the whole boot. */
		volatile unsigned long spin;
		for (spin = 0; spin < 50000000UL; spin++) {
			__asm__ volatile("" ::: "memory");
		}
	}
	k = svc3(37 /* kill */, pid, sig, 0);
	say("stest: ");
	say(name);
	say(" kill=");
	say_num(k);
	say("\n");
	w = svc4(7 /* wait4 */, pid, (long)&st, 0, 0);
	say("stest: ");
	say(name);
	say(" pid=");
	say_num(pid);
	say(" wait=");
	say_num(w);
	say(" wait=");
	say_num(w);
	say(" status=");
	say_num(st);
	say(" sig=");
	say_num(st & 0x7f);
	say("\n");
}

void main_user(void)
{
	con = svc3(5, (long)"/dev/console", 6, 0);
	if (con < 0) {
		con = 1;
	}
	test_signal("SIGTERM(15)", 15);
	test_signal("SIGKILL(9)", 9);
	say("stest: done\n");
	svc3(1, 0, 0, 0);
	for (;;) {
	}
}
