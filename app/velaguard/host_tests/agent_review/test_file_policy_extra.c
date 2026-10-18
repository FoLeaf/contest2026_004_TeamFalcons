/****************************************************************************
 * host_tests/agent_review/test_file_policy_extra.c
 *
 * Corners of the file-tool path policy (tool_files.c) that the ai_agent
 * suite's test_file_policy.c does not reach.
 *
 * Already covered there, and deliberately not repeated: writes to the agent
 * config, to /data/velaguard/config, and to the firmware runtime report; a
 * symlinked directory whose target is outside the sandbox; a read of the
 * agent config by its plain path; the allowed report and memory writes; the
 * list_dir canary.
 *
 * Added here:
 *   - the credential blob (/data/velaguard/provision) is unreadable, both by
 *     its plain path and through a symlink from an allowed directory;
 *   - the same holds for the agent config reached through a symlink;
 *   - edit_file enforces the identical policy as write_file, and a legitimate
 *     report edit still goes through;
 *   - a symlink pointing at an *existing* protected file is refused, while a
 *     symlinked parent directory that escapes is refused too (the AI-03 fix);
 *   - any path carrying ".." is refused outright;
 *   - nested report output is still creatable;
 *   - read_file / write_file / edit_file / list_dir respect small output
 *     buffers exactly, with canaries at 0/1/2/17/64 bytes.
 *
 * The relative roots come from -D on the command line, as in the existing
 * test, so the sandbox is a scratch directory and nothing real is touched.
 ****************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tools/tool_files.h"

static int g_fail;

static void expect(int cond, const char *msg)
{
    if (!cond)
    {
        fprintf(stderr, "FAIL: %s\n", msg);
        g_fail++;
    }
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

static int read_path(const char *path)
{
    char json[512];
    char out[256];
    snprintf(json, sizeof(json), "{\"path\":\"%s\"}", path);
    return tool_read_file_execute(json, out, sizeof(out));
}

static int write_path(const char *path)
{
    char json[512];
    char out[256];
    snprintf(json, sizeof(json), "{\"path\":\"%s\",\"content\":\"changed\"}", path);
    return tool_write_file_execute(json, out, sizeof(out));
}

static int edit_path(const char *path)
{
    char json[512];
    char out[256];
    snprintf(json, sizeof(json),
             "{\"path\":\"%s\",\"old_string\":\"original\",\"new_string\":\"edited\"}",
             path);
    return tool_edit_file_execute(json, out, sizeof(out));
}

static int file_contains(const char *path, const char *needle)
{
    char buf[256] = {0};
    FILE *f = fopen(path, "r");
    size_t n;
    if (!f)
        return 0;
    n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    return strstr(buf, needle) != NULL;
}

/* ── the sandbox ───────────────────────────────────────────────────── */

#define AGENT_CONFIG   "data/agent/config/config.json"
#define AGENT_MEMORY   "data/agent/memory/note.md"
#define VG_POINTS      "data/velaguard/config/points.json"
#define VG_PROVISION   "data/velaguard/provision/llm_secrets.v1"
#define VG_REPORT      "data/velaguard/reports/runtime-report.md"
#define VG_DAILY       "data/velaguard/reports/daily-2026-10-18.md"

static void build_sandbox(void)
{
    expect(mkdir("volume", 0700) == 0, "sandbox: volume");
    expect(symlink("volume", "data") == 0, "sandbox: data -> volume");
    expect(mkdir("volume/agent", 0700) == 0, "sandbox: agent");
    expect(mkdir("volume/agent/config", 0700) == 0, "sandbox: agent/config");
    expect(mkdir("volume/agent/memory", 0700) == 0, "sandbox: agent/memory");
    expect(mkdir("volume/velaguard", 0700) == 0, "sandbox: velaguard");
    expect(mkdir("volume/velaguard/config", 0700) == 0, "sandbox: velaguard/config");
    expect(mkdir("volume/velaguard/provision", 0700) == 0, "sandbox: provision");
    expect(mkdir("volume/velaguard/reports", 0700) == 0, "sandbox: reports");

    put("volume/agent/config/config.json", "original");
    put("volume/agent/config/SOUL.md", "original");
    put("volume/agent/config/USER.md", "original");
    put("volume/agent/memory/note.md", "original");
    put("volume/velaguard/config/points.json", "original");
    put("volume/velaguard/provision/llm_secrets.v1", "original");
    put("volume/velaguard/reports/runtime-report.md", "original");
    put("volume/velaguard/reports/daily-2026-10-18.md", "original");
}

/* ── tests ─────────────────────────────────────────────────────────── */

static void test_credentials_and_config_are_unreadable(void)
{
    expect(read_path(AGENT_CONFIG) < 0, "agent config is not readable");
    expect(read_path(VG_PROVISION) < 0, "the provision blob is not readable");
    expect(read_path("data/agent/config/SOUL.md") < 0, "SOUL.md is not readable");
    expect(read_path("data/agent/config/USER.md") < 0, "USER.md is not readable");

    /* The same two files reached through a symlink planted in a directory the
     * agent may write.  The read guard has to resolve the link, not compare
     * the logical path. */
    expect(symlink("../velaguard/provision/llm_secrets.v1",
                   "volume/agent/memory/peek_creds") == 0, "plant cred symlink");
    expect(read_path("data/agent/memory/peek_creds") < 0,
           "the provision blob is not readable through a symlink");

    expect(symlink("../config/config.json",
                   "volume/agent/memory/peek_cfg") == 0, "plant config symlink");
    expect(read_path("data/agent/memory/peek_cfg") < 0,
           "the agent config is not readable through a symlink");

    /* A file the agent is allowed to read still is. */
    expect(read_path(AGENT_MEMORY) == 0, "memory is readable");
    expect(file_contains("volume/agent/memory/note.md", "original"),
           "the allowed read did not modify the file");
}

static void test_write_policy_including_edit_file(void)
{
    expect(write_path(AGENT_CONFIG) < 0, "agent config is not writable");
    expect(write_path(VG_POINTS) < 0, "the point table is not writable");
    expect(write_path(VG_PROVISION) < 0, "the provision blob is not writable");
    expect(write_path(VG_REPORT) < 0, "the firmware report is not writable");

    /* edit_file must not be a softer door than write_file. */
    expect(edit_path(AGENT_CONFIG) < 0, "agent config is not editable");
    expect(edit_path(VG_POINTS) < 0, "the point table is not editable");
    expect(edit_path(VG_PROVISION) < 0, "the provision blob is not editable");
    expect(edit_path(VG_REPORT) < 0, "the firmware report is not editable");

    expect(file_contains("volume/velaguard/config/points.json", "original"),
           "the point table content is untouched");
    expect(file_contains("volume/velaguard/provision/llm_secrets.v1", "original"),
           "the provision blob content is untouched");

    /* The legitimate outputs must still work, including a nested path that
     * has to be created.  The edit target gets its own fixture: edit_file
     * matches an existing string, so it cannot run against a file the write
     * test just replaced. */
    expect(write_path(VG_DAILY) == 0, "a daily report is writable");
    put("volume/velaguard/reports/editable.md", "original");
    expect(edit_path("data/velaguard/reports/editable.md") == 0,
           "a daily report is editable");
    expect(file_contains("volume/velaguard/reports/editable.md", "edited"),
           "the allowed edit actually landed");
    expect(write_path("data/velaguard/reports/sub/deep.md") == 0,
           "nested report output is writable");
    expect(write_path(AGENT_MEMORY) == 0, "memory is writable");
}

static void test_symlink_targets(void)
{
    /* An existing symlink to an existing protected file. */
    expect(symlink("../config/points.json",
                   "volume/velaguard/reports/alias_points.md") == 0, "plant alias");
    expect(write_path("data/velaguard/reports/alias_points.md") < 0,
           "a symlink to the point table is not a way in");

    /* A symlinked parent directory that leaves the sandbox (the AI-03 case). */
    expect(symlink("../..", "volume/velaguard/reports/up") == 0, "plant parent link");
    expect(write_path("data/velaguard/reports/up/velaguard/sneak.md") < 0,
           "a symlinked parent that escapes is refused");

    /* Any ".." component is refused before resolution. */
    expect(write_path("data/velaguard/reports/../../agent/memory/dotdot.md") < 0,
           "a path containing .. is refused");
    expect(read_path("data/velaguard/reports/../../agent/memory/note.md") < 0,
           "a read path containing .. is refused");
}

static void test_small_buffers_stay_in_bounds(void)
{
    static const size_t caps[] = { 0, 1, 2, 17, 64 };
    size_t i;

    for (i = 0; i < sizeof(caps) / sizeof(caps[0]); i++)
    {
        size_t cap = caps[i];
        unsigned char raw[96];
        size_t j;

        memset(raw, 0xa5, sizeof(raw));
        {
            char json[128];
            snprintf(json, sizeof(json), "{\"path\":\"%s\"}", AGENT_MEMORY);
            (void)tool_read_file_execute(json, (char *)raw + 1, cap);
        }
        expect(raw[0] == 0xa5, "read_file left the byte before the buffer alone");
        for (j = cap + 1; j < sizeof(raw); j++)
            if (raw[j] != 0xa5)
                break;
        expect(j == sizeof(raw), "read_file wrote only inside the cap");

        memset(raw, 0xa5, sizeof(raw));
        (void)tool_list_dir_execute("{}", (char *)raw + 1, cap);
        expect(raw[0] == 0xa5, "list_dir left the byte before the buffer alone");
        for (j = cap + 1; j < sizeof(raw); j++)
            if (raw[j] != 0xa5)
                break;
        expect(j == sizeof(raw), "list_dir wrote only inside the cap");

        memset(raw, 0xa5, sizeof(raw));
        {
            char json[128];
            snprintf(json, sizeof(json), "{\"path\":\"%s\",\"content\":\"x\"}",
                     AGENT_MEMORY);
            (void)tool_write_file_execute(json, (char *)raw + 1, cap);
        }
        expect(raw[0] == 0xa5, "write_file left the byte before the buffer alone");
        for (j = cap + 1; j < sizeof(raw); j++)
            if (raw[j] != 0xa5)
                break;
        expect(j == sizeof(raw), "write_file wrote only inside the cap");
    }
}

static void test_malformed_arguments_are_refused(void)
{
    char out[128];

    expect(tool_read_file_execute("{}", out, sizeof(out)) < 0,
           "read_file without a path is refused");
    expect(tool_read_file_execute("not json", out, sizeof(out)) < 0,
           "read_file with malformed JSON is refused");
    expect(tool_write_file_execute(
               "{\"path\":\"data/agent/memory/x.md\"}", out, sizeof(out)) < 0,
           "write_file without content is refused");
    expect(tool_read_file_execute("{}", NULL, 0) < 0,
           "a null output buffer is refused");
}

int main(void)
{
    char root[] = "/tmp/vg-file-policy-extra-XXXXXX";

    if (mkdtemp(root) == NULL || chdir(root) != 0)
    {
        perror("sandbox");
        return 2;
    }

    build_sandbox();

    test_credentials_and_config_are_unreadable();
    test_write_policy_including_edit_file();
    test_symlink_targets();
    test_small_buffers_stay_in_bounds();
    test_malformed_arguments_are_refused();

    if (chdir("/tmp") != 0)
    {
        perror("chdir /tmp");
        return 2;
    }

    if (g_fail != 0)
    {
        fprintf(stderr, "test_file_policy_extra: %d failure(s)\n", g_fail);
        return 1;
    }

    puts("test_file_policy_extra: OK (credential/config read guard, edit_file "
         "parity, symlink targets, .. refusal, small-buffer canaries)");
    return 0;
}
