#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"

#include "bsp.h"
#include "schedule.h"

/* ============================================================
 * Feature switches
 * Turn these on/off to demonstrate each RTS question.
 * ============================================================ */
#define ENABLE_MARGIN_JITTER_MONITOR   0
#define ENABLE_OVERLOAD_HANDLING       0
#define ENABLE_WATCHDOG_RECOVERY       0
#define ENABLE_TASK_STATS              1

/* ============================================================
 * Policy constants
 * ============================================================ */
#define FRAME_US                   (FRAME_MS * 1000ULL)
#define SAFE_SLACK_THRESHOLD_US    500      /* warn/degrade if slack below this */
#define DEGRADED_HYPERPERIODS      2        /* stay degraded for this many hyperperiods */
#define WATCHDOG_TIMEOUT_MS        200      /* watchdog timeout */
#define MAX_CONSEC_FAULTS          3        /* repeated severe faults before recovery */

typedef struct {
    uint64_t release_us;
} task_state_t;

typedef struct {
    uint64_t min_us;
    uint64_t max_us;
    uint64_t total_us;
    uint32_t count;
} exec_stats_t;

typedef struct {
    uint32_t frame_idx;
    uint64_t release_us;
    uint64_t start_us;
    uint64_t stop_us;
    uint64_t exec_us;
    int64_t  slack_us;
    uint64_t jitter_us;
    uint8_t  ran_jobs;
    uint8_t  skipped_jobs;
    bool     severe_fault;
    bool     low_slack;
} frame_result_t;

/* Shared with timer callback */
static volatile uint32_t g_next_frame_idx = 0;
static volatile bool g_frame_pending = false;
static volatile bool g_frame_overrun = false;
static volatile uint32_t g_pending_frame_idx = 0;
static volatile uint64_t g_pending_release_us = 0;

static repeating_timer_t g_tmr;

static task_state_t g_task_state[6] = {0};
static exec_stats_t g_task_stats[6];

static uint64_t g_min_slack_us = UINT64_MAX;
static uint64_t g_max_jitter_us = 0;
static uint64_t g_total_jitter_us = 0;
static uint32_t g_frame_count = 0;

static uint32_t g_fault_count_total = 0;
static uint32_t g_consecutive_faults = 0;
static uint32_t g_degraded_hyperperiods_left = 0;

static uint32_t g_severe_faults_this_hyperperiod = 0;
static uint32_t g_bad_hyperperiod_streak = 0;

static inline uint64_t now_us(void) {
    return time_us_64();
}

static const char* task_name(task_id_t id) {
    switch (id) {
        case TSK_A: return "A";
        case TSK_B: return "B";
        case TSK_C: return "C";
        case TSK_D: return "D";
        case TSK_E: return "E";
        case TSK_F: return "F";
        default:    return "?";
    }
}

static bool is_noncritical_task(task_id_t id) {
    /* Preserve only A and B under overload. Shed C, D, E, F. */
    return (id != TSK_A) && (id != TSK_B);
}

static void init_task_states(void) {
    uint64_t t0 = now_us();
    for (int i = 0; i < 6; ++i) {
        g_task_state[i].release_us = t0;
    }
}

static void init_task_stats(void) {
    for (int i = 0; i < 6; ++i) {
        g_task_stats[i].min_us = UINT64_MAX;
        g_task_stats[i].max_us = 0;
        g_task_stats[i].total_us = 0;
        g_task_stats[i].count = 0;
    }
}

static void update_task_stats(task_id_t id, uint64_t dur_us) {
#if ENABLE_TASK_STATS
    exec_stats_t *s = &g_task_stats[id];
    if (dur_us < s->min_us) s->min_us = dur_us;
    if (dur_us > s->max_us) s->max_us = dur_us;
    s->total_us += dur_us;
    s->count++;
#else
    (void)id;
    (void)dur_us;
#endif
}

static void print_task_stats_summary(void) {
#if ENABLE_TASK_STATS
    printf("\n=== TASK EXECUTION STATS SUMMARY ===\n");
    for (int i = 0; i < 6; ++i) {
        exec_stats_t *s = &g_task_stats[i];
        if (s->count == 0) {
            printf("Task %s: no samples\n", task_name((task_id_t)i));
        } else {
            uint64_t avg_us = s->total_us / s->count;
            printf("Task %s: count=%lu min=%" PRIu64 " us max=%" PRIu64 " us avg=%" PRIu64 " us\n",
                   task_name((task_id_t)i),
                   (unsigned long)s->count,
                   s->min_us,
                   s->max_us,
                   avg_us);
        }
    }
#endif
}

static void print_frame_summary(const frame_result_t *r) {
#if ENABLE_MARGIN_JITTER_MONITOR
    printf("[FRAME %02lu SUMMARY] release=%" PRIu64
           " start=%" PRIu64
           " stop=%" PRIu64
           " jitter=%" PRIu64 " us"
           " exec=%" PRIu64 " us"
           " slack=%" PRId64 " us"
           " ran=%u skipped=%u mode=%s\n",
           (unsigned long)r->frame_idx,
           r->release_us,
           r->start_us,
           r->stop_us,
           r->jitter_us,
           r->exec_us,
           r->slack_us,
           r->ran_jobs,
           r->skipped_jobs,
           (g_degraded_hyperperiods_left > 0) ? "DEGRADED" : "NORMAL");
#else
    (void)r;
#endif
}

static void enter_degraded_mode(const char *reason) {
#if ENABLE_OVERLOAD_HANDLING
    if (g_degraded_hyperperiods_left == 0) {
        printf(">>> ENTERING DEGRADED MODE: %s\n", reason);
    }
    g_degraded_hyperperiods_left = DEGRADED_HYPERPERIODS;
#else
    (void)reason;
#endif
}

static void maybe_recover_after_hyperperiod(void) {
#if ENABLE_WATCHDOG_RECOVERY
    if (g_severe_faults_this_hyperperiod > 0) {
        g_bad_hyperperiod_streak++;
    } else {
        g_bad_hyperperiod_streak = 0;
    }

    if (g_bad_hyperperiod_streak >= 2) {
        printf("!!! WATCHDOG RECOVERY: bad hyperperiod streak = %lu\n",
               (unsigned long)g_bad_hyperperiod_streak);
        printf("!!! System will reboot now.\n");
        fflush(stdout);
        sleep_ms(50);
        watchdog_reboot(0, 0, 0);
        while (true) {
            tight_loop_contents();
        }
    }

    g_severe_faults_this_hyperperiod = 0;
#endif
}

static frame_result_t run_frame(uint32_t idx, uint64_t release_us) {
    const frame_t* fr = &g_schedule[idx];
    frame_result_t res = {0};

    res.frame_idx = idx;
    res.release_us = release_us;
    res.start_us = now_us();
    res.jitter_us = res.start_us - res.release_us;

#if ENABLE_MARGIN_JITTER_MONITOR
    if (res.jitter_us > g_max_jitter_us) g_max_jitter_us = res.jitter_us;
    g_total_jitter_us += res.jitter_us;
#endif

    printf("\n[FRAME %02lu] release=%" PRIu64 " start=%" PRIu64 " jitter=%" PRIu64 " us\n",
           (unsigned long)idx, res.release_us, res.start_us, res.jitter_us);

    for (uint8_t i = 0; i < fr->count; ++i) {
        const frame_job_t* j = &fr->jobs[i];

#if ENABLE_OVERLOAD_HANDLING
        if ((g_degraded_hyperperiods_left > 0) && is_noncritical_task(j->id)) {
            printf("[SKIP] Task %s skipped in degraded mode\n", task_name(j->id));
            res.skipped_jobs++;
            continue;
        }
#endif

        jobReturn_t r = {0};
        j->fn(&r);

        uint64_t dur_us = r.stop - r.start;
        update_task_stats(j->id, dur_us);

        g_task_state[j->id].release_us += (uint64_t)j->period_ms * 1000ULL;
        res.ran_jobs++;
    }

    res.stop_us = now_us();
    res.exec_us = res.stop_us - res.start_us;
    res.slack_us = (int64_t)(res.release_us + FRAME_US) - (int64_t)res.stop_us;

#if ENABLE_MARGIN_JITTER_MONITOR
    if ((uint64_t)res.slack_us < g_min_slack_us && res.slack_us >= 0) {
        g_min_slack_us = (uint64_t)res.slack_us;
    }
#endif

    if (res.slack_us < 0) {
        res.severe_fault = true;
        g_fault_count_total++;
        g_consecutive_faults++;
        g_severe_faults_this_hyperperiod++;
        BSP_ToggleLED(LED_RED);
        printf("!!! FRAME DEADLINE MISS: frame=%02lu slack=%" PRId64 " us\n",
            (unsigned long)idx, res.slack_us);
        enter_degraded_mode("negative slack");
    } else {
        if (res.slack_us < SAFE_SLACK_THRESHOLD_US) {
            res.low_slack = true;
            printf("!!! LOW SLACK WARNING: frame=%02lu slack=%" PRId64 " us\n",
                   (unsigned long)idx, res.slack_us);
            enter_degraded_mode("low slack");
        }

        if (!g_frame_overrun) {
            g_consecutive_faults = 0;
        }
    }

    print_frame_summary(&res);
    g_frame_count++;

    return res;
}

static void end_of_hyperperiod_housekeeping(void) {
    print_task_stats_summary();

#if ENABLE_MARGIN_JITTER_MONITOR
    uint64_t avg_jitter = (g_frame_count > 0) ? (g_total_jitter_us / g_frame_count) : 0;
    printf("\n=== FRAME TIMING SUMMARY ===\n");
    printf("frames=%lu min_slack=%" PRIu64 " us max_jitter=%" PRIu64 " us avg_jitter=%" PRIu64 " us faults=%lu\n",
           (unsigned long)g_frame_count,
           (g_min_slack_us == UINT64_MAX) ? 0 : g_min_slack_us,
           g_max_jitter_us,
           avg_jitter,
           (unsigned long)g_fault_count_total);
#endif

#if ENABLE_OVERLOAD_HANDLING
    if (g_degraded_hyperperiods_left > 0) {
        g_degraded_hyperperiods_left--;
        if (g_degraded_hyperperiods_left == 0) {
            printf(">>> EXITING DEGRADED MODE\n");
        } else {
            printf(">>> DEGRADED MODE remains for %lu more hyperperiod(s)\n",
                   (unsigned long)g_degraded_hyperperiods_left);
        }
    }
#endif

    maybe_recover_after_hyperperiod();
}

static bool on_timer(repeating_timer_t *tmr) {
    (void)tmr;

    uint32_t irq_state = save_and_disable_interrupts();

    if (g_frame_pending) {
        g_frame_overrun = true;
    } else {
        g_pending_frame_idx = g_next_frame_idx;
        g_pending_release_us = now_us();
        g_next_frame_idx = (g_next_frame_idx + 1) % NUM_FRAMES;
        g_frame_pending = true;
    }

    restore_interrupts(irq_state);
    return true;
}

int main(void) {
    stdio_init_all();
    BSP_Init();

    sleep_ms(2000);

#if ENABLE_WATCHDOG_RECOVERY
    if (watchdog_caused_reboot()) {
        printf(">>> NOTICE: system restarted after watchdog event\n");
    }
    watchdog_enable(WATCHDOG_TIMEOUT_MS, 1);
#endif

    init_task_states();
    init_task_stats();

    printf("Cyclic scheduler started. frame_ms=%u hyper_ms=%u\n", FRAME_MS, HYPER_MS);
    printf("Features: margin/jitter=%d overload=%d watchdog=%d task_stats=%d\n",
           ENABLE_MARGIN_JITTER_MONITOR,
           ENABLE_OVERLOAD_HANDLING,
           ENABLE_WATCHDOG_RECOVERY,
           ENABLE_TASK_STATS);

    add_repeating_timer_ms(FRAME_MS, on_timer, NULL, &g_tmr);

    while (true) {
#if ENABLE_WATCHDOG_RECOVERY
        watchdog_update();
#endif

        bool have_frame = false;
        bool overrun = false;
        uint32_t frame_idx = 0;
        uint64_t release_us = 0;

        uint32_t irq_state = save_and_disable_interrupts();
        if (g_frame_pending) {
            have_frame = true;
            frame_idx = g_pending_frame_idx;
            release_us = g_pending_release_us;
            g_frame_pending = false;
        }
        if (g_frame_overrun) {
            overrun = true;
            g_frame_overrun = false;
        }
        restore_interrupts(irq_state);

        if (overrun) {
            printf("!!! FRAME OVERRUN: previous frame was still pending at next release\n");
            BSP_ToggleLED(LED_RED);
            g_fault_count_total++;
            g_consecutive_faults++;
            enter_degraded_mode("frame overrun");
        }

        if (have_frame) {
            frame_result_t r = run_frame(frame_idx, release_us);

            if (r.severe_fault) {
            }

            if (frame_idx == (NUM_FRAMES - 1)) {
                end_of_hyperperiod_housekeeping();
            }
        } else {
            tight_loop_contents();
        }
    }
}