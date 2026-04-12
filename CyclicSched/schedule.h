#pragma once
#include <stdint.h>
#include "workload.h"

#define FRAME_MS    5
#define HYPER_MS    100
#define NUM_FRAMES  (HYPER_MS / FRAME_MS)

typedef enum { TSK_A, TSK_B, TSK_C, TSK_D, TSK_E, TSK_F } task_id_t;
typedef void (*job_fn_t)(jobReturn_t*);

typedef struct {
    task_id_t   id;
    job_fn_t    fn;
    uint32_t    period_ms;
    uint32_t    deadline_ms;
} frame_job_t;

typedef struct {
    const frame_job_t* jobs;
    uint8_t            count;
} frame_t;

extern const frame_t g_schedule[NUM_FRAMES];

static inline job_fn_t task_fn(task_id_t id) {
    switch (id) {
        case TSK_A: return job_A;
        case TSK_B: return job_B;
        case TSK_C: return job_C;
        case TSK_D: return job_D;
        case TSK_E: return job_E;
        case TSK_F: return job_F;
        default:    return job_A;
    }
}

static inline uint32_t task_period_ms(task_id_t id) {
    switch (id) {
        case TSK_A: return 10;
        case TSK_B: return 5;
        case TSK_C: return 25;
        case TSK_D: return 50;
        case TSK_E: return 50;
        case TSK_F: return 20;
        default:    return 1000;
    }
}