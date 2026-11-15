/* Minimal init for the XNU RAM-disk root bring-up.
 *
 * Freestanding arm64 Mach-O, no libc: every interface is a direct XNU syscall
 * (svc #0x80, number in x16). Behaviour:
 *   - mount devfs, open /dev/console
 *   - prove VFS works by reading our own Mach-O header
 *   - start the configured services in order with posix_spawn() and record
 *     their exit status with wait4()
 *   - run a heartbeat service on a fixed cadence
 *   - stay alive as pid 1, reaping any stray children
 */
typedef unsigned long u64;

static long svc3(long num, long a, long b, long c)
{
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x16 __asm__("x16") = num;
	__asm__ volatile("svc #0x80\n b.cc 1f\n neg x0, x0\n1:"
	    : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16) : "memory");
	return x0;
}

static long svc4(long num, long a, long b, long c, long d)
{
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	register long x16 __asm__("x16") = num;
	__asm__ volatile("svc #0x80\n b.cc 1f\n neg x0, x0\n1:"
	    : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x16) : "memory");
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
	if (v < 0) {
		svc3(4, fd, (long)"-", 1);
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
		svc3(4, fd, (long)&b[--i], 1);
	}
}

static long con_fd = 1;

static void say(const char *s) { write_str(con_fd, s); }
static void say_num(const char *s, long v) { write_str(con_fd, s); write_num(con_fd, v); }

/* posix_spawn(path) -> child pid, or negative errno. */
static long spawn_service(const char *path)
{
	static char *argv[2];
	int cpid = -1;
	argv[0] = (char *)path;
	argv[1] = 0;
	long r = svc6(244 /* posix_spawn */, (long)&cpid, (long)path, 0, 0, (long)argv, 0);
	if (r != 0) {
		return r;
	}
	return (long)cpid;
}

static void run_service(const char *name, const char *path)
{
	long pid = spawn_service(path);
	if (pid < 0) {
		say("init: spawn ");
		say(name);
		say(" failed errno=");
		write_num(con_fd, -pid);
		say("\n");
		return;
	}
	int st = 0;
	long w = svc4(7 /* wait4 */, pid, (long)&st, 0, 0);
	say("init: service ");
	say(name);
	say(" pid=");
	write_num(con_fd, pid);
	say(" wait=");
	write_num(con_fd, w);
	say(" status=");
	write_num(con_fd, st);
	say(" exit=");
	write_num(con_fd, (st >> 8) & 0xff);
	say("\n");
}

/* Start a service and restart it when it exits abnormally. */
static void supervise_service(const char *name, const char *path, int max_restarts)
{
	int attempt, restarts = 0;
	for (attempt = 1; attempt <= max_restarts + 1; attempt++) {
		long pid = spawn_service(path);
		int st = 0;
		long w;
		if (pid < 0) {
			say("init: supervise ");
			say(name);
			say(" spawn failed errno=");
			write_num(con_fd, -pid);
			say("\n");
			return;
		}
		w = svc4(7, pid, (long)&st, 0, 0);
		say("init: supervise ");
		say(name);
		say(" attempt=");
		write_num(con_fd, attempt);
		say(" pid=");
		write_num(con_fd, pid);
		say(" wait=");
		write_num(con_fd, w);
		say(" exit=");
		write_num(con_fd, (st >> 8) & 0xff);
		say("\n");
		if (((st >> 8) & 0xff) == 0) {
			say("init: service ");
			say(name);
			say(" exited cleanly; supervision done\n");
			return;
		}
		restarts++;
		if (restarts > max_restarts) {
			say("init: service ");
			say(name);
			say(" keeps failing; giving up after restarts=");
			write_num(con_fd, max_restarts);
			say("\n");
			return;
		}
		say("init: restarting ");
		say(name);
		say(" (");
		write_num(con_fd, restarts);
		say("/");
		write_num(con_fd, max_restarts);
		say(")\n");
	}
}

void main_user(void)
{
	long con;
	int i;
	static const char *services[] = { "/sbin/hello", "/sbin/svc2", "/sbin/wtest", "/sbin/dtest", "/sbin/ltest", "/sbin/stest" };

	svc3(167 /* mount */, (long)"devfs", (long)"/dev", 0);
	con = svc3(5 /* open */, (long)"/dev/console", 6 /* O_RDWR|O_NONBLOCK */, 0);
	if (con >= 0) {
		con_fd = con;
	}

	say("hello from userspace on Xiaomi Mi 6 (XNU ramdisk root)\n");
	say_num("init: pid=", svc3(20, 0, 0, 0));
	say("\n");

	/* VFS check: read our own Mach-O header through the HFS+ root. */
	{
		long self = svc3(5, (long)"/sbin/launchd", 0, 0);
		unsigned char hdr[4] = { 0 };
		long n = -1;
		if (self >= 0) {
			n = svc3(3, self, (long)hdr, 4);
			svc3(6, self, 0, 0);
		}
		say_num("init: vfs self-open=", self);
		say_num(" n=", n);
		say(" hdr=");
		for (i = 0; i < 4; i++) {
			static const char hx[] = "0123456789abcdef";
			char b[2] = { hx[(hdr[i] >> 4) & 15], hx[hdr[i] & 15] };
			svc3(4, con_fd, (long)b, 2);
		}
		say("\n");
	}

	/* Start the service list in order and record every exit status. */
	for (i = 0; i < (int)(sizeof(services) / sizeof(services[0])); i++) {
		run_service(services[i], services[i]);
	}

	/* Crash supervision: restart a failing service up to three times. */
	supervise_service("/sbin/crasher", "/sbin/crasher", 3);

	/* Heartbeat cadence: three ticks, two seconds apart. */
	for (i = 0; i < 3; i++) {
		static long ts[2] = { 2, 0 };
		svc3(240 /* nanosleep */, (long)&ts[0], 0, 0);
		{
			long pid = spawn_service("/sbin/heartbeat");
			int st = 0;
			long w;
			if (pid < 0) {
				say_num("init: heartbeat spawn failed errno=", -pid);
				say("\n");
				continue;
			}
			w = svc4(7, pid, (long)&st, 0, 0);
			say("init: heartbeat pid=");
			write_num(con_fd, pid);
			say(" wait=");
			write_num(con_fd, w);
			say(" exit=");
			write_num(con_fd, (st >> 8) & 0xff);
			say("\n");
		}
	}

	say("init: all services started; staying alive as pid 1\n");

	/* Supervisor idle loop: reap strays if any, otherwise sleep. */
	for (;;) {
		static long idle[2] = { 5, 0 };
		int st = 0;
		long w = svc4(7 /* wait4 */, -1, (long)&st, 1 /* WNOHANG */, 0);
		if (w > 0) {
			say("init: reaped stray pid=");
			write_num(con_fd, w);
			say(" exit=");
			write_num(con_fd, (st >> 8) & 0xff);
			say("\n");
		}
		svc3(240, (long)&idle[0], 0, 0);
	}
}
