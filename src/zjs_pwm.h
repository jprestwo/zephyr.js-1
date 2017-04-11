// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __zjs_pwm_h__
#define __zjs_pwm_h__

#include "jerryscript.h"

extern void (*zjs_pwm_convert_pin)(uint32_t num, int *dev, int *pin);

/*
 * Initialize the pwm module, or reinitialize after cleanup
 *
 * @return PWM API object
 */
jerry_value_t zjs_pwm_init();

/* Release resources held by the pwm module */
void zjs_pwm_cleanup();

#endif  // __zjs_pwm_h__
