/****************************************************************************
 * host_tests/agent_review/test_runner_budget.c
 *
 * Fault injection for the bounded shell runner (tool_shell_runner.c).
 *
 * The ai_agent tree already ships tests/test_shell_runner.c, which covers the
 * happy path, a silent hang, a child that closes stdout, a bounded overflow,
 * recovery and the direct-child reap.  This file adds only what that one
 * cannot show:
 *
 *   1. a child that never stops producing output still hits the deadline
 *      (the drain loop must not be starved by the read side);
 *   2. the deadline is honoured to within a bounded error, so a stuck
 *      command cannot hold the tool thread for an unbounded time;
 *   3. a background descendant of the command is reclaimed, not left
 *      running behind a returned timeout  (host: process-group kill);
 *   4. repeated timeouts leak neither file descriptors nor children;
 *   5. a child killed by a signal is a failure, not a success;
 *   6. the output buffer is written only inside [0, cap-1], for caps of
 *      1, 2, 3, 5, 64 and 257 bytes;
 *   7. the documented argument guards actually reject.
 *
 * Every command is bounded: the longest single run is about 1.2 s.
 ****************************************************************************/

#include "tools/tool_shell_runner.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static uint64_t mono_ms(void)
{
    struct timespec ts;
    assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

/* Count of open descriptors, used only for a before/after comparison. */
static int open_fds(void)
{
    DIR *d = opendir("/proc/self/fd");
    struct dirent *e;
    int n = 0;
    assert(d != NULL);
    while ((e = readdir(d)) != NULL)
        if (e->d_name[0] != '.')
            n++;
    closedir(d);
    return n;
}

/* 1 + 2: a command that never stops writing must still end at the deadline. */
static void test_endless_output_hits_deadline(void)
{
    char out[64];
    uint64_t t0 = mono_ms();
    int rc = tool_shell_run("while true; do printf x; done", out, sizeof(out), 200);
    uint64_t elapsed = mono_ms() - t0;

    assert(rc == -ETIMEDOUT);
    assert(elapsed >= 180);    /* must not give up before the budget */
    assert(elapsed <= 1200);   /* and must not run past it */
    assert(strlen(out) == sizeof(out) - 1);   /* capped and terminated */
}

/* 2: the deadline for a silent hang is the requested one, not "eventually". */
static void test_deadline_is_accurate(void)
{
    char out[64];
    uint64_t t0 = mono_ms();
    int rc = tool_shell_run("sleep 5", out, sizeof(out), 300);
    uint64_t elapsed = mono_ms() - t0;

    assert(rc == -ETIMEDOUT);
    assert(elapsed >= 280);
    assert(elapsed <= 1200);
}

/* 3: what the command left behind in the background must be gone.
 *
 * The child is started as its own process-group leader, so on the host the
 * kill reaches everything it forked.  NuttX only signals the task itself
 * (kill(pid, SIGKILL), no group), which is why this property is a host
 * result and not a board result - see the review document.
 */
static void test_background_descendant_is_reclaimed(void)
{
    char dir[] = "/tmp/vg-runner-reclaim-XXXXXX";
    char marker[256];
    char cmd[512];
    char out[64];

    assert(mkdtemp(dir) != NULL);
    snprintf(marker, sizeof(marker), "%s/marker", dir);
    snprintf(cmd, sizeof(cmd),
             "(sleep 0.5; printf late > %s) & sleep 5", marker);

    assert(tool_shell_run(cmd, out, sizeof(out), 50) == -ETIMEDOUT);

    /* Wait well past the point where the background writer would have run. */
    usleep(900000);
    assert(access(marker, F_OK) != 0);

    unlink(marker);
    rmdir(dir);
}

/* 4: the stop path must give the descriptors back and reap the child. */
static void test_repeated_timeouts_do_not_leak(void)
{
    char out[64];
    int before = open_fds();
    int i;

    for (i = 0; i < 30; i++)
        assert(tool_shell_run("sleep 5", out, sizeof(out), 20) == -ETIMEDOUT);

    assert(open_fds() <= before + 1);

    /* No child of ours may be left unreaped. */
    assert(waitpid(-1, NULL, WNOHANG) < 0 && errno == ECHILD);

    /* And the next normal request still works. */
    assert(tool_shell_run("printf recovered", out, sizeof(out), 500) == 0);
    assert(!strcmp(out, "recovered"));
}

/* 5: a signalled child is a failure, not an empty success. */
static void test_signalled_child_is_a_failure(void)
{
    char out[64];
    assert(tool_shell_run("kill -9 $$", out, sizeof(out), 500) == -EIO);
    assert(tool_shell_run("", out, sizeof(out), 200) == 0);
    assert(out[0] == '\0');
}

/* 6: exact canaries for tiny and awkward caps. */
static void test_output_stays_inside_the_cap(void)
{
    static const size_t caps[] = { 1, 2, 3, 5, 64, 257 };
    unsigned char raw[300];
    size_t i;

    for (i = 0; i < sizeof(caps) / sizeof(caps[0]); i++)
    {
        size_t cap = caps[i];
        size_t j;

        memset(raw, 0xa5, sizeof(raw));
        (void)tool_shell_run("printf abcdefghij", (char *)raw + 1, cap, 500);
        assert(raw[0] == 0xa5);
        for (j = cap + 1; j < sizeof(raw); j++)
            assert(raw[j] == 0xa5);
    }
}

/* 7: the argument guards documented in the header. */
static void test_argument_guards(void)
{
    char out[32];
    assert(tool_shell_run(NULL, out, sizeof(out), 100) == -EINVAL);
    assert(tool_shell_run("x", NULL, sizeof(out), 100) == -EINVAL);
    assert(tool_shell_run("x", out, 0, 100) == -EINVAL);
    assert(tool_shell_run("x", out, sizeof(out), 0) == -EINVAL);
}

int main(void)
{
    test_argument_guards();
    test_endless_output_hits_deadline();
    test_deadline_is_accurate();
    test_background_descendant_is_reclaimed();
    test_repeated_timeouts_do_not_leak();
    test_signalled_child_is_a_failure();
    test_output_stays_inside_the_cap();

    puts("test_runner_budget: OK (endless output, deadline accuracy, "
         "descendant reclaim, fd/zombie leaks, signalled child, cap canaries)");
    return 0;
}
