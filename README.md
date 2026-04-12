# Real-Time Embedded Scheduling and Resilience

This project implements and evaluates a small real-time embedded system focused on cyclic scheduling, timing behavior, overload handling, fault detection, and recovery strategies. The work was built as a practical real-time systems study, combining scheduler implementation with timing measurements and runtime fault-handling mechanisms.

The project is centered on a frame-based cyclic executive and extends it with monitoring and resilience features that are important in real-time embedded systems. The implementation was used to study how execution-time variation, scheduler overhead, overload, and recovery mechanisms affect deadline satisfaction in practice.

## Real-Time Systems Questions Addressed

The project was used to investigate the following real-time systems questions:

- How much timing margin does the cyclic schedule have, and how much jitter does the implementation introduce?
- How stable are the task execution times, and what execution-time variation is observed?
- How should a real-time system react under overload while preserving critical timing guarantees?
- How can a real-time embedded system recover from timing faults or hangs?

## Functionality Used

The implementation uses the following embedded-system functionality:

- frame-based cyclic scheduling
- periodic timer-driven dispatch
- execution-time measurement using timestamps
- deadline and slack monitoring
- release jitter monitoring
- overload handling through degraded-mode task shedding
- watchdog-based recovery
- runtime task execution statistics
- serial output for trace and timing logs
- board support package services for timing and peripheral initialization

## Implementation Overview

The system is implemented as a cyclic executive with a fixed minor frame and a repeating frame table over the full hyperperiod. Each frame releases a predefined set of jobs. At runtime, the scheduler records release time, start time, stop time, execution time, slack, and jitter.

The project was extended in stages:

1. **Baseline cyclic scheduler**  
   A frame-based dispatcher was implemented to execute the task set according to a static schedule.

2. **Timing instrumentation**  
   Task start and stop timestamps were added so execution time could be measured experimentally on the real hardware.

3. **Frame timing analysis**  
   The implementation was extended to measure frame execution time, frame slack, release jitter, and frame deadline misses.

4. **Overload handling**  
   A degraded mode was added so that non-critical jobs can be skipped under overload, allowing more critical timing behavior to be preserved.

5. **Watchdog recovery**  
   A watchdog mechanism was added to recover from repeated bad hyperperiods or severe timing faults.

6. **Execution-time statistics**  
   Per-task minimum, maximum, and average execution times were collected to study timing stability and variation.

## Project Structure

```text
.
├── CMakeLists.txt
├── README.md
├── CyclicSched/
│   ├── main.c
│   ├── schedule.c
│   └── schedule.h
└── common/
    ├── workload.c
    └── workload.h

## Build and Run

1. Make sure the Pico SDK environment is configured.
2. Open the project in VS Code.
3. Configure the project with CMake.
4. Build the project.
5. Copy the generated `.uf2` file to the board in BOOTSEL mode.
6. Open the serial monitor to view timing summaries and runtime logs.