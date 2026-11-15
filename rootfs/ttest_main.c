/* Minimal userspace thread test: bsdthread_register + bsdthread_create. */
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
static long svc7(long num, long a, long b, long c, long d, long e, long f, long g)
{
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	register long x4 __asm__("x4") = e;
	register long x5 __asm__("x5") = f;
	register long x6 __asm__("x6") = g;
	register long x16 __asm__("x16") = num;
	__asm__ volatile("svc #0x80\n b.cc 1f\n neg x0, x0\n1:"
	    : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x6), "r"(x16) : "memory");
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

static void say_hex(unsigned long v)
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

struct timeval64 {
	long tv_sec;
	long tv_usec;
	int pad;
};

static void gettime(struct timeval64 *tv)
{
	tv->tv_sec = 0;
	tv->tv_usec = 0;
	tv->pad = 0;
	svc3(116 /* gettimeofday */, (long)tv, 0, 0);
}

static unsigned long fb_abstime(void)
{
	static const char rname[] = "kern.sagitfbreport";
	static unsigned char buf[1024];
	unsigned long len = sizeof(buf);
	long rc = svc6(274, (long)rname, (long)sizeof(rname) - 1,
	    (long)buf, (long)&len, 0, 0);
	if (rc != 0 || len < 8) {
		return 0;
	}
	/* mach_abs is the last 8 bytes of the report struct */
	return *(unsigned long *)(buf + len - 8);
}

extern void thread_tramp(void);
volatile unsigned long thread_done;
volatile unsigned long thread_count;

static void thread_fn(void *arg)
{
	thread_count++;
	thread_done = 1;
	say("ttest: hello from the second thread! arg=");
	say_num((long)arg);
	say("\n");
}

void main_user(void)
{
	long rc, sreg, spid;
	long stack, pthread_buf;
	static long pause_ts[2] = { 0, 20000000 }; /* 20 ms */
	int i;

	con = svc3(5, (long)"/dev/console", 6, 0);
	if (con < 0) {
		con = 1;
	}

	/* MAP_PRIVATE|MAP_ANON stack (256 KiB) and pthread area (4 KiB). */
	stack = svc6(197, 0, 0x40000, 3, 0x1002, -1, 0);
	pthread_buf = svc6(197, 0, 0x1000, 3, 0x1002, -1, 0);
	say("ttest: stack=");
	say_num(stack);
	say(" pthread=");
	say_num(pthread_buf);
	say("\n");
	if (stack < 0 || pthread_buf < 0) {
		svc3(1, 1, 0, 0);
	}

	/* Register this process with the pthread kext: (threadstart, wqthread,
	 * pthsize, pthread_init_data, data_size, dispatchqueue_offset, tsd). */
	sreg = svc7(366, (long)&thread_tramp, 0, 1024, 0, 0, 0, 0);
	say("ttest: bsdthread_register=");
	say_num(sreg);
	say(" (capability bits; non-negative = ok)\n");
	if (sreg < 0) {
		svc3(1, 2, 0, 0);
	}

	/* Ask the kernel to spawn a kernel thread (proves the scheduler path). */
	{
		struct fb_req {
			unsigned int x, y, w, h, color;
		} r;
		static const char fbname[] = "kern.sagitfb";
		long krc;
		r.x = 0x5A617469; /* magic */
		r.y = 0;
		r.w = 0;
		r.h = 0;
		r.color = 0x00A2B1E3; /* arm the return-to-user probe */
		krc = svc6(274, (long)fbname, (long)sizeof(fbname) - 1, 0, 0,
		    (long)&r, (long)sizeof(r));
		say("ttest: return-to-user probe arm rc=");
		say_num(krc);
		say("\n");
	}

	/* PTHREAD_START_CUSTOM = 0x01000000; stack top must be 16-aligned. */
	spid = svc6(360 /* bsdthread_create */, (long)&thread_fn, 0x1234,
	    (stack + 0x40000) & ~15L, pthread_buf, 0x01000000, 0);
	say("ttest: bsdthread_create=");
	say_num(spid);
	say("\n");
	if (spid < 0) {
		svc3(1, 3, 0, 0);
	}

	/* Wait up to ~2 s for the thread to run, and measure the real elapsed time. */
	{
		unsigned long raw0, raw1, spin;
		/* Is the kernel timebase advancing at all? */
		raw0 = fb_abstime();
		for (spin = 0; spin < 20000000UL; spin++) {
			__asm__ volatile("" ::: "memory");
		}
		raw1 = fb_abstime();
		say("ttest: timebase raw0=");
		say_num((long)raw0);
		say(" raw1=");
		say_num((long)raw1);
		say("\n");
	}
	{
		/* Verify that nanosleep really waits, using the kernel timebase. */
		unsigned long a0 = fb_abstime();
		long nrc = sys_sleep(pause_ts[0], pause_ts[1]);
		unsigned long a1 = fb_abstime();
		say("ttest: nanosleep rc=");
		say_num(nrc);
		say(" abs_delta=");
		say_num((long)(a1 - a0));
		say("\n");
		for (i = 0; i < 100 && !thread_done; i++) {
			sys_sleep(pause_ts[0], pause_ts[1]);
		}
	}
	/* Read back the return-to-user probe samples (normal context, safe). */
	{
		static const char rname[] = "kern.sagitfbreport";
		struct probe_sample {
			unsigned long pc, sp, cpsr, tid;
		};
		struct probe_report {
			unsigned int count, armed, bcount, reserved;
			unsigned long btids[8];
			struct probe_sample samples[16];
			unsigned long mach_abs;
		};
		static struct probe_report rep;
		unsigned long rlen = sizeof(rep);
		long rrc = svc6(274, (long)rname, (long)sizeof(rname) - 1,
		    (long)&rep, (long)&rlen, 0, 0);
		unsigned int k;
		say("ttest: return-probe rc=");
		say_num(rrc);
		say(" count=");
		say_num((long)rep.count);
		say(" bcount=");
		say_num((long)rep.bcount);
		say("\n");
		{
			unsigned int k;
			for (k = 0; k < rep.bcount && k < 8; k++) {
				say("ttest: bootstrap tid=");
				say_num((long)rep.btids[k]);
				say("\n");
			}
		}
		for (k = 0; k < rep.count && k < 16; k++) {
			say("ttest: sample pc=");
			say_hex(rep.samples[k].pc);
			say(" sp=");
			say_hex(rep.samples[k].sp);
			say(" cpsr=");
			say_hex(rep.samples[k].cpsr);
			say(" tid=");
			say_num((long)rep.samples[k].tid);
			say("\n");
		}
	}

	say("ttest: done flag=");
	say_num((long)thread_done);
	say(" count=");
	say_num((long)thread_count);
	say("\n");
	svc3(1, thread_done ? 0 : 4, 0, 0);
	for (;;) {
	}
}
