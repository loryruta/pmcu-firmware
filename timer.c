#include "timer.h"

#include <msp430.h>

#define TIMER_TASKS_RLENGTH 16
#define TIMER_TASKS_LENGTH (TIMER_TASKS_RLENGTH - 1)

timer_Task *timer_tasks[TIMER_TASKS_RLENGTH];
uint32_t timer_counter;

void timer_init() {
    uint8_t i;
    for (i = 0; i < TIMER_TASKS_RLENGTH; i++) {
        timer_tasks[i] = 0;
    }

    timer_counter = 0;

    // aclk = 32768hz
    // 1/32768hz * 8 * x = 1s - values to have every second timer
    // x = 4096
    TA1CCR0 = 4096;

    TA1CTL = TASSEL__ACLK | MC__UP | ID__8;
    TA1CCTL0 &= ~CCIFG;
    TA1CCTL0 |= CCIE;
}

uint64_t timer_timestamp() {
    return timer_counter;
}

void timer_task_start(timer_Task *task, uint32_t delay) {
    uint8_t next_id;

    for (next_id = 1; next_id < TIMER_TASKS_RLENGTH; next_id++) {
        if (!timer_tasks[next_id]) {
            break;
        }
    }
    if (next_id == TIMER_TASKS_LENGTH) {
        return; // no position available (we will never create TIMER_TASKS_LENGTH tasks)
    }
    task->id = next_id;
    task->delay = delay;
    task->started_at = timer_counter;
    task->satisfied = 0;
    timer_tasks[task->id] = task;
}

void timer_task_cancel(timer_Task *task) {
    timer_tasks[task->id] = 0;
    task->id = 0;
}

#pragma vector=TIMER1_A0_VECTOR // interrupt for TA1CCR0
__interrupt void timer_on_tick() {
    uint8_t i;

    timer_counter++;

    for (i = 1; i < TIMER_TASKS_RLENGTH; i++) {
        if (timer_tasks[i] && (timer_counter - timer_tasks[i]->started_at) >= timer_tasks[i]->delay) {
            timer_tasks[i]->satisfied = 1;
            timer_tasks[i] = 0;
        }
    }

    TA1CCTL0 &= ~CCIFG;
}
