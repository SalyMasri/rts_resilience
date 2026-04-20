
#include "workload.h"

void job_A(jobReturn_t* retval) {
    retval->start = time_us_64();
    BSP_WaitClkCycles(EXECUTION_TIME_A);
    retval->stop = time_us_64();
}
/*-----------------------------------------------------------*/

void job_B(jobReturn_t* retval) {
    retval->start = time_us_64();
    BSP_WaitClkCycles(EXECUTION_TIME_B);
    retval->stop = time_us_64();
}
/*-----------------------------------------------------------*/

void job_C(jobReturn_t* retval) {
    retval->start = time_us_64();
    BSP_WaitClkCycles(EXECUTION_TIME_C);
    retval->stop = time_us_64();
}
/*-----------------------------------------------------------*/

void job_D(jobReturn_t* retval) {
    retval->start = time_us_64();
    BSP_WaitClkCycles(EXECUTION_TIME_D);
    retval->stop = time_us_64();
}
/*-----------------------------------------------------------*/

void job_E(jobReturn_t* retval) {
    retval->start = time_us_64();
    BSP_WaitClkCycles(EXECUTION_TIME_E);
    retval->stop = time_us_64();
}
/*-----------------------------------------------------------*/

void job_F(jobReturn_t* retval) {
    retval->start = time_us_64();
    BSP_WaitClkCycles(EXECUTION_TIME_F);
    retval->stop = time_us_64();
}
/*-----------------------------------------------------------*/
