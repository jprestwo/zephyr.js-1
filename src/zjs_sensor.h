// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __zjs_sensor_h__
#define __zjs_sensor_h__

#include "jerryscript.h"

/* Initialize the sensor module, or reinitialize after cleanup */
void zjs_sensor_init();

/* Release resources held by the sensor module */
void zjs_sensor_cleanup();

#endif  // __zjs_sensor_h__
