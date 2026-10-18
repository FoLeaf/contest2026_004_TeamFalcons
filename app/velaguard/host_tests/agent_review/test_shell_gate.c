/****************************************************************************
 * host_tests/agent_review/test_shell_gate.c
 *
 * The product command gate in tool_shell.c, compiled the way the board
 * builds it: AGENT_SHELL_SECURITY defaults to ALLOWLIST (agent_config.h) and
 * the velaguard-lvgl preset sets CONFIG_VG_HMI, so both the VelaGuard
 * per-command table (s_vg_readonly) and the metadata-only allowlist are
 * active.
 *
 * test_shell_output.c in the ai_agent tree asserts five blocked commands and
 * the small-buffer canaries.  What it does not pin down, and what this file
 * does, is the whole table in both directions: every subcommand the agent
 * must not reach, and every command it must still reach.  A gate that starts
 * over-blocking is just as much a defect as one that under-blocks, and the
 * positive rows are what make an over-block visible.
 *
 * Note on scope: the "allowed" rows only assert that the gate let the
 * command through, not that the command ran.  Several of them (heap_info,
 * net_status, router_status, vgstats, vgcfg, vgruntime, vgnet) are NSH
 * commands or VelaGuard apps that do not exist on the host, so they exit
 * non-zero here.  That is expected and is not what is being measured.
 ****************************************************************************/

#include <stdio.h>
#include <string.h>

#include "tools/tool_shell.c"

static int g_fail;

static void expect(int cond, const char *msg)
{
    if (!cond)
    {
        fprintf(stderr, "FAIL: %s\n", msg);
        g_fail++;
    }
}

/* Run one command through the tool and report whether the gate refused it. */
static int refused(const char *command, char *out, size_t cap)
{
    char json[512];
    snprintf(json, sizeof(json), "{\"command\":\"%s\"}", command);
    (void)tool_run_shell_execute(json, out, cap);
    return strstr(out, "is blocked for safety") != NULL;
}

struct row
{
    const char *command;
    int         must_be_refused;
    const char *why;
};

static const struct row g_rows[] = {
    /* ── VelaGuard state changes: refused outright ─────────────────── */
    { "vgpoint",                     1, "bare vgpoint" },
    { "vgpoint get",                 1, "vgpoint get" },
    { "vgpoint list",                1, "vgpoint list" },
    { "vgpoint add ups_load 0 100",  1, "vgpoint add" },
    { "vgpoint set 1 2 3",           1, "vgpoint set" },
    { "vgpoint del 1",               1, "vgpoint del" },
    { "vgpoint test",                1, "vgpoint test" },
    { "vgpoint apply --confirm",     1, "vgpoint apply" },
    { "/bin/vgpoint get",            1, "vgpoint under a path prefix" },
    { "vgdiscover",                  1, "bare vgdiscover" },
    { "vgdiscover scan",             1, "vgdiscover scan" },
    { "vgdiscover apply",            1, "vgdiscover apply" },
    { "vgmodbus",                    1, "bare vgmodbus" },
    { "vgmodbus -a 1 -r 0 -n 0",     1, "vgmodbus read" },

    /* ── per-command subcommand gate ───────────────────────────────── */
    { "vgcfg",                       1, "bare vgcfg" },
    { "vgcfg commit changed",        1, "vgcfg commit" },
    { "vgcfg apply",                 1, "vgcfg apply" },
    { "vgcfg dumpx",                 1, "a prefix of dump is not dump" },
    { "vgruntime",                   1, "bare vgruntime" },
    { "vgruntime report /tmp/x.md",  1, "vgruntime report writes a file" },
    { "vgruntime reset",             1, "vgruntime reset" },
    { "vgstats",                     1, "bare vgstats" },
    { "vgstats reset",               1, "vgstats reset" },
    { "vgstats inject 1 2 3",        1, "vgstats inject" },
    { "vgstats dumpsters",           1, "dumpsters is not dump" },
    { "vgstats dum",                 1, "dum is not dump" },
    { "vgnet",                       1, "bare vgnet" },
    { "vgnet inject eth0 1.2.3.4",   1, "vgnet inject" },
    { "vgnet wifi ssid pw",          1, "vgnet wifi" },

    /* ── no general-purpose file access in the product build ───────── */
    { "cat /data/velaguard/provision/llm_secrets.v1", 1, "cat of the credential blob" },
    { "cat /data/agent/config/config.json",           1, "cat of the agent config" },
    { "ls /data/velaguard/config",                    1, "ls of the config directory" },
    { "head -n 1 /data/velaguard/config/points.json", 1, "head of the point table" },
    { "grep -r key /data/velaguard",                  1, "grep over /data" },
    { "sed -i s/a/b/ /data/velaguard/config/points.json", 1, "sed in place" },
    { "jq . /data/velaguard/config/points.json",      1, "jq over the point table" },
    { "echo hello",                                   1, "echo" },
    { "sh -c id",                                     1, "a nested shell" },

    /* ── destructive commands ──────────────────────────────────────── */
    { "reboot",                                       1, "reboot" },
    { "rm -rf /data",                                 1, "rm" },
    { "mv a b",                                       1, "mv" },
    { "kill -9 1",                                    1, "kill" },
    { "dd if=/dev/zero of=/dev/mmcblk0",              1, "dd" },
    { "mount /dev/x /mnt",                            1, "mount" },
    { "chmod 777 /data",                              1, "chmod" },

    /* ── shell metacharacters are refused before anything else ─────── */
    { "vgstats dump; reboot",                         1, "command chaining" },
    { "vgstats dump | cat",                           1, "a pipe" },
    { "vgstats dump > /tmp/x",                        1, "a redirect" },
    { "vgstats dump `id`",                            1, "command substitution" },

    /* ── the read-only commands the agent must still reach ─────────── */
    { "vgstats dump",                                 0, "vgstats dump" },
    { "vgstats  dump",                                0, "extra spaces before dump" },
    { "vgcfg dump",                                   0, "vgcfg dump" },
    { "vgruntime dump",                               0, "vgruntime dump" },
    { "vgnet status",                                 0, "vgnet status" },
    { "free",                                         0, "free" },
    { "free -h",                                      0, "free with an argument" },
    { "ps",                                           0, "ps" },
    { "df",                                           0, "df" },
    { "uptime",                                       0, "uptime" },
    { "uname",                                        0, "uname" },
    { "heap_info",                                    0, "heap_info" },
    { "net_status",                                   0, "net_status" },
    { "router_status",                                0, "router_status" },
    { "date",                                         0, "date" },
    { "date -u",                                      0, "date UTC" },
    { "date +%s",                                     0, "date with a read-only format" },
    { "date -u +%s",                                  0, "date UTC with a read-only format" },

    /* Other metadata commands keep their historical option handling.  date
     * is different: the product gate must accept exactly one read-only
     * argument sequence.  The setter forms below are checked through
     * is_blocked() and are deliberately never executed. */
};

static void test_gate_table(void)
{
    char out[512];
    size_t i;

    for (i = 0; i < sizeof(g_rows) / sizeof(g_rows[0]); i++)
    {
        int got = refused(g_rows[i].command, out, sizeof(out));
        char msg[256];

        snprintf(msg, sizeof(msg), "%s: %s", g_rows[i].why,
                 g_rows[i].must_be_refused ? "must be refused" : "must be allowed");
        expect(got == g_rows[i].must_be_refused, msg);
    }
}

static void test_date_setter_gate(void)
{
    /* Use the gate directly.  A regression must not execute date -s on the
     * host or the board just to discover that it was allowed. */
    expect(is_blocked("date -s 'Oct 18 20:00:00 2026'"),
           "date -s is rejected by the gate");
    expect(is_blocked("date -sOct 18 20:00:00 2026"),
           "date -s without a separating space is rejected");
    expect(is_blocked("date -u -sOct 18 20:00:00 2026"),
           "date -s after -u is rejected");
    expect(is_blocked("date +%s -s 'Oct 18 20:00:00 2026'"),
           "date -s after a format is rejected");
    expect(is_blocked("date -u +%s -s 'Oct 18 20:00:00 2026'"),
           "date -s after UTC and a format is rejected");
    expect(is_blocked("date +%s\t-s 'Oct 18 20:00:00 2026'"),
           "a tab cannot hide date -s after a format");
    expect(is_blocked("date +%s extra"),
           "an operand after a format is rejected");
    expect(is_blocked("date -u +%s extra"),
           "an operand after UTC and a format is rejected");
    expect(is_blocked("date -u -u"), "duplicate date options are rejected");
    expect(is_blocked("date +"), "a bare format introducer is rejected");
    expect(!is_blocked("date"), "date without options remains allowed");
    expect(!is_blocked("date   "),
           "trailing whitespace after date remains allowed");
    expect(!is_blocked("date -u"), "date -u remains allowed");
    expect(!is_blocked("date -u   "),
           "trailing whitespace after date -u remains allowed");
    expect(!is_blocked("date +%s"), "date format output remains allowed");
    expect(!is_blocked("date +%s   "),
           "trailing whitespace after a date format remains allowed");
    expect(!is_blocked("date -u +%s"),
           "date UTC format output remains allowed");
}

static void test_argument_guards(void)
{
    char out[512];
    char json[512];
    char long_cmd[400];
    size_t i;

    /* No command at all. */
    expect(tool_run_shell_execute("{}", out, sizeof(out)) < 0, "missing command");
    expect(strstr(out, "Missing 'command'") != NULL, "missing command is explained");
    expect(tool_run_shell_execute("{}", out, sizeof(out)) < 0, "empty object");
    expect(tool_run_shell_execute("not json", out, sizeof(out)) < 0, "malformed JSON");

    /* Over the 256 byte command limit, refused before the gate. */
    for (i = 0; i < sizeof(long_cmd) - 1; i++)
        long_cmd[i] = 'a';
    long_cmd[sizeof(long_cmd) - 1] = '\0';
    snprintf(json, sizeof(json), "{\"command\":\"%s\"}", long_cmd);
    expect(tool_run_shell_execute(json, out, sizeof(out)) < 0, "over-long command");
    expect(strstr(out, "too long") != NULL, "over-long command is explained");

    /* A non-string command is not a command. */
    expect(tool_run_shell_execute("{\"command\":42}", out, sizeof(out)) < 0,
           "a numeric command is refused");

    /* A null output buffer must not be written. */
    expect(tool_run_shell_execute("{\"command\":\"df\"}", NULL, 0) < 0,
           "a null output buffer is refused");
}

int main(void)
{
    test_gate_table();
    test_date_setter_gate();
    test_argument_guards();

    if (g_fail != 0)
    {
        fprintf(stderr, "test_shell_gate: %d failure(s)\n", g_fail);
        return 1;
    }

    puts("test_shell_gate: OK (vgpoint/vgdiscover/vgmodbus refused, per-command "
         "subcommand gate, metadata-only allowlist, positive rows)");
    return 0;
}
