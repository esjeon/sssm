
static char fmt[1024] = "@d";
static int mode_xsetroot = 0;
static int mode_loop = 0;
static int interval = 2000; /* millisecond */

static Method methods[] = {
    { 'd', method_date },
    { 'x', method_exec },
};
