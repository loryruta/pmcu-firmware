#ifndef TIMER_H_
#define TIMER_H_

#include <stdlib.h>
#include <stdint.h>

typedef struct {
    uint8_t id;

    uint32_t started_at;
    uint32_t delay;

    uint8_t satisfied;

} timer_Task;

void timer_init();

uint64_t timer_timestamp();

void timer_task_start(timer_Task *task, uint32_t delay);

void timer_task_cancel(timer_Task *task);

#endif
