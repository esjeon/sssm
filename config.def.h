
static char fmt[1024] = "Memory - @x[grep Avail /proc/meminfo] !";
static int mode_xsetroot = 0;
static int mode_loop = 0;
static int interval = 2000; /* millisecond */

static Method methods[] = {
    { 'x', runcmd },
};
