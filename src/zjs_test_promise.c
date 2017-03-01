// Copyright (c) 2016-2017, Intel Corporation.

#ifdef BUILD_MODULE_TEST_PROMISE

#include "zjs_common.h"
#include "zjs_callbacks.h"
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
        zjs_fulfill_promise(promise, NULL, 0);
    } else {
        zjs_reject_promise(promise, NULL, 0);
    }
    toggle = (toggle) ? 0 : 1;
    count++;

    return promise;
}

static uint32_t async_count = 0;
static jerry_value_t async_promise;
static uint8_t in_progress = 0;


static jerry_value_t test_async_promise(const jerry_value_t function_obj,
                                        const jerry_value_t this,
                                        const jerry_value_t argv[],
                                        const jerry_length_t argc)
{
    if (!in_progress) {
        async_promise = jerry_create_object();
        ZJS_PRINT("Testing Async promise, object = %u, count = %u\n", async_promise, async_count);

        zjs_make_promise(async_promise, NULL, NULL);

        return async_promise;
    } else {
        return ZJS_UNDEFINED;
    }
}

static jerry_value_t test_fulfill(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    zjs_fulfill_promise(argv[0], NULL, 0);
    return ZJS_UNDEFINED;
}

static jerry_value_t test_reject(const jerry_value_t function_obj,
                                 const jerry_value_t this,
                                 const jerry_value_t argv[],
                                 const jerry_length_t argc)
{
    zjs_reject_promise(argv[0], NULL, 0);
    return ZJS_UNDEFINED;
}


jerry_value_t zjs_test_promise_init(void)
{
    jerry_value_t test = jerry_create_object();
    zjs_obj_add_function(test, test_promise, "test_promise");
    zjs_obj_add_function(test, test_async_promise, "test_async_promise");
    zjs_obj_add_function(test, test_fulfill, "fulfill");
    zjs_obj_add_function(test, test_reject, "reject");

    return test;
}

void zjs_test_promise_cleanup()
{
}

#endif  // BUILD_MODULE_CONSOLE
