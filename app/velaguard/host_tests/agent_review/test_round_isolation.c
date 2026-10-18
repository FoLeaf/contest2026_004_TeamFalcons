/****************************************************************************
 * host_tests/agent_review/test_round_isolation.c
 *
 * Request correlation and result ownership for vg_agent_round.c.
 *
 * host_tests/test_agent_round.c already covers one timeout, one late
 * callback, the strict "consume before the next round" queue and recovery.
 * This file adds the parts of the contract that are still unproven, all of
 * them deterministic on a mocked clock:
 *
 *   - a request that never reaches the agent bus is released by the queue
 *     watchdog instead of holding the channel for the whole round budget;
 *   - a reply that arrives while the request is still QUEUED is kept, not
 *     dropped on the floor;
 *   - a submit failure isolates that round: the failed round's callback can
 *     neither mark the failed round as answered nor the next one;
 *   - reclaim() frees somebody else's finished-but-unread round only after
 *     the grace period, never the caller's own, never a running one;
 *   - the generation counter only advances on a reply, so a consumer can
 *     tell a fresh result from one it already handled;
 *   - the documented argument guards reject.
 ****************************************************************************/

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <velaclaw/client.h>

#include "vg_agent_round.h"

/* The request buffer is private to vg_agent_round.c.  These two lengths pin
 * its boundary from the outside: 1535 fits, 1536 must be refused. */
#define REQ_MAX 1536

/* ── mocked clock and agent client ─────────────────────────────────── */

static uint32_t now = 1000;

int __wrap_clock_gettime(clockid_t clock, struct timespec *ts)
{
    (void)clock;
    ts->tv_sec = now / 1000;
    ts->tv_nsec = (now % 1000) * 1000000;
    return 0;
}

static void (*cb_slot)(int, const char *, void *);
static void *cookie_slot;
static int ask_rc;
static int ask_calls;
static int open_ok = 1;
static int sync_reply;              /* answer inside the ask, as a synchronous
                                     * dispatch build would */
static const char *sync_text = "sync-answer";

velaclaw_client_t *velaclaw_client_open(const char *name)
{
    (void)name;
    return open_ok ? (velaclaw_client_t *)(uintptr_t)1 : NULL;
}

int velaclaw_ask_async(velaclaw_client_t *c, const velaclaw_ask_req_t *req,
                       void (*cb)(int, const char *, void *), void *ck)
{
    (void)c;
    (void)req;
    ask_calls++;
    if (ask_rc != 0)
        return ask_rc;
    cb_slot = cb;
    cookie_slot = ck;
    if (sync_reply)
        cb(0, sync_text, ck);
    return 0;
}

/* ── helpers ───────────────────────────────────────────────────────── */

#define STATE_IS(s) assert(vg_agent_round_state() == (s))

/* Queue, submit, and answer the round with `text`. */
static void complete_round(const char *text, enum vg_agent_round_owner owner)
{
    assert(vg_agent_round_queue_owned(text, owner) == 0);
    vg_agent_round_tick();                 /* QUEUED -> RUNNING */
    STATE_IS(VG_AGENT_ROUND_RUNNING);
    cb_slot(0, text, cookie_slot);
    vg_agent_round_tick();                 /* reply seen -> DONE */
    STATE_IS(VG_AGENT_ROUND_DONE);
}

static void expect_reply(const char *expected)
{
    char reply[VG_AGENT_ROUND_REPLY_MAX];

    assert(vg_agent_round_copy_last_reply(reply, sizeof(reply)) == 0);
    assert(!strcmp(reply, expected));
}

/* ── tests ─────────────────────────────────────────────────────────── */

/* The bus never comes up: the round must be released by the queue watchdog,
 * and it must retry the open on every tick in the meantime. */
static void test_queue_watchdog_when_bus_is_down(void)
{
    open_ok = 0;

    assert(vg_agent_round_queue("boot") == 0);
    STATE_IS(VG_AGENT_ROUND_QUEUED);
    assert(vg_agent_round_busy());

    vg_agent_round_tick();
    STATE_IS(VG_AGENT_ROUND_QUEUED);        /* still waiting for the bus */
    assert(ask_calls == 0);

    now += VG_AGENT_ROUND_QUEUE_TIMEOUT_MS / 2;
    vg_agent_round_tick();
    STATE_IS(VG_AGENT_ROUND_QUEUED);        /* grace not spent yet */

    now += VG_AGENT_ROUND_QUEUE_TIMEOUT_MS;
    vg_agent_round_tick();
    STATE_IS(VG_AGENT_ROUND_ERROR);         /* released, not left hanging */
    assert(!vg_agent_round_busy());
    assert(ask_calls == 0);

    vg_agent_round_clear();
    STATE_IS(VG_AGENT_ROUND_IDLE);

    open_ok = 1;
    ask_rc = 0;
}

static void test_argument_guards(void)
{
    static char big[REQ_MAX + 64];

    assert(vg_agent_round_queue(NULL) == -EINVAL);
    assert(vg_agent_round_queue("") == -EINVAL);
    STATE_IS(VG_AGENT_ROUND_IDLE);

    memset(big, 'a', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    assert(vg_agent_round_queue(big) == -E2BIG);
    STATE_IS(VG_AGENT_ROUND_IDLE);          /* a refused request changes nothing */
    assert(vg_agent_round_copy_last_reply(NULL, 16) == -EINVAL);
    {
        char reply[VG_AGENT_ROUND_REPLY_MAX];
        assert(vg_agent_round_copy_last_reply(reply, 0) == -EINVAL);
    }

    big[REQ_MAX - 1] = '\0';                /* exactly REQ_MAX - 1 bytes */
    assert(strlen(big) == REQ_MAX - 1);
    assert(vg_agent_round_queue(big) == 0);
    vg_agent_round_tick();
    STATE_IS(VG_AGENT_ROUND_RUNNING);

    /* Abandon it; the late reply for this round is used by the next test. */
    now += VG_AGENT_ROUND_TIMEOUT_MS;
    vg_agent_round_tick();
    STATE_IS(VG_AGENT_ROUND_ERROR);
    vg_agent_round_clear();
    STATE_IS(VG_AGENT_ROUND_IDLE);
}

/* A stale callback from an abandoned round must not touch the new one. */
static void test_late_callback_cannot_cross_rounds(void)
{
    void (*old_cb)(int, const char *, void *) = cb_slot;
    void *old_cookie = cookie_slot;
    uint32_t gen_before;

    complete_round("first", VG_AGENT_ROUND_OWNER_ADVICE);
    gen_before = vg_agent_round_generation();
    assert(vg_agent_round_owner() == VG_AGENT_ROUND_OWNER_ADVICE);
    expect_reply("first");
    {
        char truncated[4];

        assert(vg_agent_round_copy_last_reply(truncated,
                                              sizeof(truncated)) == 0);
        assert(!strcmp(truncated, "fir"));
    }

    /* A terminal result is not free until its owner reads it. */
    assert(vg_agent_round_queue("blocked") == -EBUSY);
    STATE_IS(VG_AGENT_ROUND_DONE);

    vg_agent_round_clear();
    STATE_IS(VG_AGENT_ROUND_IDLE);
    assert(vg_agent_round_owner() == VG_AGENT_ROUND_OWNER_NONE);
    assert(vg_agent_round_generation() == gen_before);

    assert(vg_agent_round_queue_owned("second", VG_AGENT_ROUND_OWNER_DAILY) == 0);
    vg_agent_round_tick();
    STATE_IS(VG_AGENT_ROUND_RUNNING);

    /* The reply of the previous round arrives now. */
    old_cb(0, "stale", old_cookie);
    vg_agent_round_tick();
    STATE_IS(VG_AGENT_ROUND_RUNNING);       /* not mistaken for an answer */
    assert(vg_agent_round_generation() == gen_before);

    cb_slot(0, "fresh", cookie_slot);
    vg_agent_round_tick();
    STATE_IS(VG_AGENT_ROUND_DONE);
    assert(vg_agent_round_generation() == gen_before + 1);
    expect_reply("fresh");

    /* A duplicate of the current reply is ignored as well. */
    cb_slot(0, "again", cookie_slot);
    vg_agent_round_tick();
    STATE_IS(VG_AGENT_ROUND_DONE);
    assert(vg_agent_round_generation() == gen_before + 1);
    expect_reply("fresh");

    vg_agent_round_clear();
}

/* A reply that arrives while the round is still QUEUED must be kept.
 *
 * round_submit() calls the client before it moves the state to RUNNING, so a
 * client that dispatches its reply synchronously (or a second thread that
 * wins the race) delivers a callback while the round still reads QUEUED.
 * round_reply() accepts QUEUED for exactly this reason; dropping it would
 * lose the answer and leave the round to expire on the watchdog. */
static void test_reply_while_queued_is_kept(void)
{
    uint32_t gen_before = vg_agent_round_generation();

    assert(vg_agent_round_queue("early") == 0);
    STATE_IS(VG_AGENT_ROUND_QUEUED);

    sync_reply = 1;
    vg_agent_round_tick();                  /* submit; reply lands mid-submit */
    sync_reply = 0;
    STATE_IS(VG_AGENT_ROUND_RUNNING);

    vg_agent_round_tick();                  /* the kept reply is consumed */
    STATE_IS(VG_AGENT_ROUND_DONE);
    assert(vg_agent_round_generation() == gen_before + 1);
    expect_reply(sync_text);

    vg_agent_round_clear();
}

/* A rejected submit is a terminal error for that round only. */
static void test_submit_failure_is_isolated(void)
{
    void (*previous_cb)(int, const char *, void *) = cb_slot;
    void *previous_cookie = cookie_slot;
    uint32_t gen_before = vg_agent_round_generation();
    int calls_before = ask_calls;

    ask_rc = -EBUSY;                        /* the client still thinks it is busy */
    assert(vg_agent_round_queue_owned("doomed", VG_AGENT_ROUND_OWNER_DAILY) == 0);
    vg_agent_round_tick();
    STATE_IS(VG_AGENT_ROUND_ERROR);
    assert(ask_calls == calls_before + 1);
    assert(vg_agent_round_generation() == gen_before);   /* no reply, no bump */

    /* The old round's callback must not turn the failure into a success. */
    previous_cb(0, "ghost", previous_cookie);
    vg_agent_round_tick();
    STATE_IS(VG_AGENT_ROUND_ERROR);
    assert(vg_agent_round_generation() == gen_before);

    vg_agent_round_clear();
    ask_rc = 0;

    /* Recovery: the channel is usable again and the ghost cannot leak into
     * the round that follows either. */
    assert(vg_agent_round_queue_owned("after", VG_AGENT_ROUND_OWNER_ADVICE) == 0);
    vg_agent_round_tick();
    STATE_IS(VG_AGENT_ROUND_RUNNING);
    previous_cb(0, "ghost again", previous_cookie);
    vg_agent_round_tick();
    STATE_IS(VG_AGENT_ROUND_RUNNING);

    cb_slot(0, "after-answer", cookie_slot);
    vg_agent_round_tick();
    STATE_IS(VG_AGENT_ROUND_DONE);
    expect_reply("after-answer");
    vg_agent_round_clear();
}

static void test_reclaim_semantics(void)
{
    uint32_t gen_before = vg_agent_round_generation();

    complete_round("daily", VG_AGENT_ROUND_OWNER_DAILY);
    assert(vg_agent_round_generation() == gen_before + 1);

    /* The owner's own result is never reclaimed, at any age. */
    assert(!vg_agent_round_reclaim(VG_AGENT_ROUND_OWNER_DAILY, 30000));
    now += 600000;
    assert(!vg_agent_round_reclaim(VG_AGENT_ROUND_OWNER_DAILY, 30000));
    STATE_IS(VG_AGENT_ROUND_DONE);

    /* Somebody else's result is only freed once the grace period is spent.
     * `now` is 600000 ms past the finish here, so a grace of 600001 must not
     * release it and a grace of 600000 must. */
    assert(!vg_agent_round_reclaim(VG_AGENT_ROUND_OWNER_ADVICE, 600001));
    STATE_IS(VG_AGENT_ROUND_DONE);
    assert(vg_agent_round_reclaim(VG_AGENT_ROUND_OWNER_ADVICE, 600000));
    STATE_IS(VG_AGENT_ROUND_IDLE);
    assert(vg_agent_round_owner() == VG_AGENT_ROUND_OWNER_NONE);
    assert(vg_agent_round_generation() == gen_before + 1);   /* result kept */

    /* Nothing to reclaim while idle. */
    assert(!vg_agent_round_reclaim(VG_AGENT_ROUND_OWNER_ADVICE, 0));

    /* A running round is never reclaimed, however long the caller waits. */
    assert(vg_agent_round_queue_owned("running", VG_AGENT_ROUND_OWNER_DAILY) == 0);
    vg_agent_round_tick();
    STATE_IS(VG_AGENT_ROUND_RUNNING);
    now += 600000;
    assert(!vg_agent_round_reclaim(VG_AGENT_ROUND_OWNER_ADVICE, 0));
    STATE_IS(VG_AGENT_ROUND_RUNNING);

    cb_slot(0, "running-answer", cookie_slot);
    vg_agent_round_tick();
    STATE_IS(VG_AGENT_ROUND_DONE);
    assert(vg_agent_round_reclaim(VG_AGENT_ROUND_OWNER_ADVICE, 0));
    STATE_IS(VG_AGENT_ROUND_IDLE);
}

int main(void)
{
    test_queue_watchdog_when_bus_is_down();
    test_argument_guards();
    test_late_callback_cannot_cross_rounds();
    test_reply_while_queued_is_kept();
    test_submit_failure_is_isolated();
    test_reclaim_semantics();

    puts("test_round_isolation: OK (queue watchdog, stale/duplicate replies, "
         "early reply, submit failure isolation, reclaim)");
    return 0;
}
