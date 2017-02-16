// Copyright (c) 2016-2017, Intel Corporation.

#ifdef BUILD_MODULE_TEST_PROMISE

#include "zjs_common.h"
#include "zjs_promise.h"
#include "zjs_util.h"

static uint8_t toggle = 0;
static uint32_t count = 0;

static jerry_value_t test_promise(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    jerry_value_t promise = jerry_create_object();
    ZJS_PRINT("Testing promise, object = %u, count = %u\n", promise, count);

    zjs_make_promise(promise, NULL, NULL);

    if (toggle) {
        ZJS_PRINT("Fulfilling\n");
        zjs_fulfill_promise(promise, NULL, 0);
    } else {
        ZJS_PRINT("Rejecting\n");
        zjs_reject_promise(promise, NULL, 0);
    }
    //toggle = (toggle) ? 0 : 1;
    count++;

    return promise;
}

jerry_value_t zjs_test_promise_init(void)
{
    jerry_value_t test = jerry_create_object();
    zjs_obj_add_function(test, test_promise, "test_promise");

    return test;
}

void zjs_test_promise_cleanup()
{
}

#endif  // BUILD_MODULE_CONSOLE
