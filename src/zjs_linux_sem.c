// Copyright (c) 2017, Intel Corporation.

#include "zjs_linux_port.h"

int zjs_port_sem_take(zjs_port_sem *sem, int32_t wait)
{
    zjs_port_timer_t timer;
    zjs_port_timer_start(&timer, wait);
    int ret = pthread_mutex_trylock(sem);
    while (ret != 0) {
        if (zjs_port_timer_test(&timer)) {
            return -EAGAIN;
        }
        ret = pthread_mutex_trylock(sem);
    }
    return 0;
}
