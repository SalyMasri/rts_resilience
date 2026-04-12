#include "schedule.h"

/* One static descriptor per task */
static const frame_job_t JOB_A = { TSK_A, job_A, 10, 10 };
static const frame_job_t JOB_B = { TSK_B, job_B,  5,  5 };
static const frame_job_t JOB_C = { TSK_C, job_C, 25, 25 };
static const frame_job_t JOB_D = { TSK_D, job_D, 50, 50 };
static const frame_job_t JOB_E = { TSK_E, job_E, 50, 50 };
static const frame_job_t JOB_F = { TSK_F, job_F, 20, 20 };

/* Frame contents */
static const frame_job_t FR_B[]   = { JOB_B };
static const frame_job_t FR_BA[]  = { JOB_B, JOB_A };
static const frame_job_t FR_BC[]  = { JOB_B, JOB_C };
static const frame_job_t FR_BD[]  = { JOB_B, JOB_D };
static const frame_job_t FR_BE[]  = { JOB_B, JOB_E };
static const frame_job_t FR_BF[]  = { JOB_B, JOB_F };
static const frame_job_t FR_BAC[] = { JOB_B, JOB_A, JOB_C };
static const frame_job_t FR_BAF[] = { JOB_B, JOB_A, JOB_F };

/*
 * Clean phasing:
 * - B every frame
 * - A once every 2 frames
 * - F once every 4 frames
 * - C once every 5 frames
 * - D once every 10 frames
 * - E once every 10 frames
 *
 * Heavy B+C+D frames are removed.
 * Remaining critical frames are only B+E.
 */
const frame_t g_schedule[NUM_FRAMES] = {
    /* 00 */ { FR_BE,  2 },   /* B + E */
    /* 01 */ { FR_BA,  2 },   /* B + A */
    /* 02 */ { FR_BD,  2 },   /* B + D */
    /* 03 */ { FR_BAF, 3 },   /* B + A + F */
    /* 04 */ { FR_BC,  2 },   /* B + C */
    /* 05 */ { FR_BA,  2 },   /* B + A */
    /* 06 */ { FR_B,   1 },   /* B */
    /* 07 */ { FR_BAF, 3 },   /* B + A + F */
    /* 08 */ { FR_BC,  2 },   /* B + C */
    /* 09 */ { FR_BA,  2 },   /* B + A */

    /* 10 */ { FR_BE,  2 },   /* B + E */
    /* 11 */ { FR_BAF, 3 },   /* B + A + F */
    /* 12 */ { FR_BD,  2 },   /* B + D */
    /* 13 */ { FR_BA,  2 },   /* B + A */
    /* 14 */ { FR_BC,  2 },   /* B + C */
    /* 15 */ { FR_BAF, 3 },   /* B + A + F */
    /* 16 */ { FR_B,   1 },   /* B */
    /* 17 */ { FR_BA,  2 },   /* B + A */
    /* 18 */ { FR_BF,  2 },   /* B + F */
    /* 19 */ { FR_BAC, 3 }    /* B + A + C */
};