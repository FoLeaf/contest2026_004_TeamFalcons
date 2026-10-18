/****************************************************************************
 * host_tests/agent_review/repro_symlink_write_escape.c
 *
 * Minimal reproduction of an OPEN defect in tool_files.c.  This program is
 * expected to FAIL until the defect is fixed; it is kept out of the `test`
 * target so the regression set stays a usable signal.
 *
 *   make -C app/velaguard/host_tests/agent_review repro
 *
 * What it shows
 * -------------
 * write_file follows a symlink that sits in the LAST path component when
 * that symlink is dangling (its target does not exist yet).  The policy
 * check inspects the link's own path, the write itself goes to the link's
 * target, so a write the policy refuses by path is performed anyway:
 *
 *   case A: create a file completely outside the sandbox
 *   case B: create a file at a path the policy explicitly protects
 *
 * How the two paths diverge
 * -------------------------
 * validate_path() / is_write_protected() call resolve_target(), which
 * resolves the full path with realpath() and, when that fails, falls back to
 * resolving the deepest EXISTING ancestor and appending the remaining
 * components verbatim:
 *
 *     realpath("data/velaguard/reports/a.md")      -> fails (dangling)
 *     realpath("data/velaguard/reports")           -> ok
 *     resolved = "<abs>/data/velaguard/reports/a.md"
 *
 * The dangling name is therefore never resolved, so the policy sees a path
 * that is inside the one writable subtree.  tool_write_file_execute() then
 * calls fopen(path, "w") on the LOGICAL path, which does follow the link and
 * creates the target.
 *
 * Scope, measured rather than assumed
 * ----------------------------------
 *   - write_file is affected.  edit_file is not: it opens the file for
 *     reading first, and a dangling link has nothing to read.
 *   - an EXISTING symlink to an existing file is refused correctly, because
 *     realpath() resolves the full path in that case.
 *   - a symlinked parent directory that escapes is refused correctly (the
 *     AI-03 fix).
 *   - reads are unaffected: an existing link is resolved and protected, a
 *     dangling one cannot be opened at all.
 *   - NuttX /data is a FAT volume, which has no symlinks, so this is not
 *     reachable on the current board.  It becomes live if /data ever moves
 *     to a symlink-capable filesystem, or if a future tool can create one.
 *
 * Suggested fix (not applied here; the product source is under review, not
 * being changed by this task): have resolve_target() resolve the final
 * component too, or refuse the write when lstat() reports the target is a
 * symlink.
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tools/tool_files.h"

static int g_fail;

static void check(int cond, const char *what)
{
    printf("  %-58s %s\n", what, cond ? "PASS" : "FAIL");
    if (!cond)
        g_fail++;
}

static int write_path(const char *path, const char *content)
{
    char json[512];
    char out[256];
    snprintf(json, sizeof(json), "{\"path\":\"%s\",\"content\":\"%s\"}",
             path, content);
    return tool_write_file_execute(json, out, sizeof(out));
}

static void put(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f)
    {
        perror(path);
        exit(2);
    }
    fputs(text, f);
    fclose(f);
}

int main(void)
{
    char root[] = "/tmp/vg-symlink-escape-XXXXXX";

    if (mkdtemp(root) == NULL || chdir(root) != 0)
    {
        perror("sandbox");
        return 2;
    }

    mkdir("volume", 0700);
    symlink("volume", "data");
    mkdir("volume/agent", 0700);
    mkdir("volume/agent/config", 0700);
    mkdir("volume/velaguard", 0700);
    mkdir("volume/velaguard/config", 0700);
    mkdir("volume/velaguard/reports", 0700);
    mkdir("outside", 0700);
    put("volume/velaguard/config/points.json", "REAL");

    puts("control: an existing symlink to an existing protected file");
    symlink("../config/points.json", "volume/velaguard/reports/c.md");
    check(write_path("data/velaguard/reports/c.md", "PWNED") < 0,
          "write through an existing link is refused");
    check(access("volume/velaguard/config/points.json", F_OK) == 0,
          "and the protected file still exists");

    puts("case A: dangling link, target outside the sandbox");
    symlink("../../../outside/pwned.md", "volume/velaguard/reports/a.md");
    check(write_path("data/velaguard/reports/a.md", "PWNED") < 0,
          "write through the dangling link is refused");
    check(access("outside/pwned.md", F_OK) != 0,
          "no file was created outside the sandbox");

    puts("case B: dangling link, target at a protected path");
    symlink("../config/planted.json", "volume/velaguard/reports/b.md");
    check(write_path("data/velaguard/reports/b.md", "PWNED") < 0,
          "write through the dangling link is refused");
    check(access("volume/velaguard/config/planted.json", F_OK) != 0,
          "no file was created at the protected path");

    if (chdir("/tmp") != 0)
    {
        perror("chdir /tmp");
        return 2;
    }

    if (g_fail != 0)
    {
        printf("repro_symlink_write_escape: %d check(s) FAILED - defect present\n",
               g_fail);
        return 1;
    }

    puts("repro_symlink_write_escape: all checks passed - defect fixed");
    return 0;
}
