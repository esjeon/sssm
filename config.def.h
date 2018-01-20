
static char fmt[1024] = "@t";
static int mode_xsetroot = 0;
static int mode_loop = 0;
static int interval = 2000; /* millisecond */

static Method methods[] = {
    { 't', curtime },
    { 'x', runcmd },
};
