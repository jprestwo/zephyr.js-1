// Copyright (c) 2016-2017, Intel Corporation.

#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include <zephyr.h>
#endif

#include <string.h>
#include <stdlib.h>

#ifndef ZJS_LINUX_BUILD
#include "zjs_zephyr_port.h"
#else
#include "zjs_linux_port.h"
#endif

// ZJS includes
#if defined(CONFIG_BOARD_ARDUINO_101) || defined(ZJS_LINUX_BUILD)
#include "zjs_a101_pins.h"
#endif
#ifdef BUILD_MODULE_BUFFER
#include "zjs_buffer.h"
#endif
#ifdef BUILD_MODULE_CONSOLE
#include "zjs_console.h"
#endif
#include "zjs_dgram.h"
#include "zjs_net.h"
#include "zjs_web_sockets.h"
#include "zjs_event.h"
#include "zjs_gpio.h"
#include "zjs_modules.h"
#include "zjs_performance.h"
#include "zjs_callbacks.h"
#ifdef BUILD_MODULE_SENSOR
#include "zjs_sensor.h"
#endif
#include "zjs_timers.h"
#include "zjs_util.h"
#ifdef BUILD_MODULE_OCF
#include "zjs_ocf_common.h"
#endif
#ifdef BUILD_MODULE_TEST_PROMISE
#include "zjs_test_promise.h"
#endif
#ifdef BUILD_MODULE_TEST_CALLBACKS
#include "zjs_test_callbacks.h"
#endif

#ifndef ZJS_LINUX_BUILD
#include "zjs_aio.h"
#include "zjs_ble.h"
#include "zjs_grove_lcd.h"
#include "zjs_i2c.h"
#include "zjs_pwm.h"
#include "zjs_uart.h"
#include "zjs_fs.h"
#ifdef CONFIG_BOARD_ARDUINO_101
#include "zjs_ipm.h"
#endif
#ifdef CONFIG_BOARD_FRDM_K64F
#include "zjs_k64f_pins.h"
#endif
#else
#include "zjs_script.h"
#endif // ZJS_LINUX_BUILD

typedef jerry_value_t (*initcb_t)();
typedef void (*cleanupcb_t)();

typedef struct module {
    const char *name;
    initcb_t init;
    cleanupcb_t cleanup;
    jerry_value_t instance;
} module_t;

// init function is required, cleanup is optional in these entries
module_t zjs_modules_array[] = {
#ifndef ZJS_LINUX_BUILD
#ifndef QEMU_BUILD
#ifndef CONFIG_BOARD_FRDM_K64F
#ifdef BUILD_MODULE_AIO
    { "aio", zjs_aio_init, zjs_aio_cleanup },
#endif
#endif
#ifdef BUILD_MODULE_BLE
    { "ble", zjs_ble_init, zjs_ble_cleanup },
#endif
#ifdef BUILD_MODULE_GROVE_LCD
    { "grove_lcd", zjs_grove_lcd_init, zjs_grove_lcd_cleanup },
#endif
#ifdef BUILD_MODULE_PWM
    { "pwm", zjs_pwm_init, zjs_pwm_cleanup },
#endif
#ifdef BUILD_MODULE_I2C
    { "i2c", zjs_i2c_init },
#endif
#ifdef BUILD_MODULE_FS
    { "fs", zjs_fs_init, zjs_fs_cleanup },
#endif
#ifdef CONFIG_BOARD_FRDM_K64F
    { "k64f_pins", zjs_k64f_init },
#endif
#endif // QEMU_BUILD
#ifdef BUILD_MODULE_UART
    { "uart", zjs_uart_init, zjs_uart_cleanup },
#endif
#endif // ZJS_LINUX_BUILD
#ifdef BUILD_MODULE_A101
    { "arduino101_pins", zjs_a101_init },
#endif
#ifdef BUILD_MODULE_GPIO
    { "gpio", zjs_gpio_init, zjs_gpio_cleanup },
#endif
#ifdef BUILD_MODULE_DGRAM
    { "dgram", zjs_dgram_init, zjs_dgram_cleanup },
#endif
#ifdef BUILD_MODULE_NET
    { "net", zjs_net_init, zjs_net_cleanup },
#endif
#ifdef BUILD_MODULE_WS
    { "ws", zjs_ws_init, zjs_ws_cleanup },
#endif
#ifdef BUILD_MODULE_EVENTS
    { "events", zjs_event_init, zjs_event_cleanup },
#endif
#ifdef BUILD_MODULE_PERFORMANCE
    { "performance", zjs_performance_init },
#endif
#ifdef BUILD_MODULE_OCF
    { "ocf", zjs_ocf_init, zjs_ocf_cleanup },
#endif
#ifdef BUILD_MODULE_TEST_PROMISE
    { "test_promise", zjs_test_promise_init },
#endif
#ifdef BUILD_MODULE_TEST_CALLBACKS
    { "test_callbacks", zjs_test_callbacks_init }
#endif
};

struct routine_map {
    zjs_service_routine func;
    void *handle;
};

static uint8_t num_routines = 0;
struct routine_map svc_routine_map[NUM_SERVICE_ROUTINES];

static ZJS_DECL_FUNC(native_require_handler)
{
    // args: module name
    ZJS_VALIDATE_ARGS(Z_STRING);

    jerry_size_t size = MAX_MODULE_STR_LEN;
    char module[size];
    zjs_copy_jstring(argv[0], module, &size);
    if (!size) {
        return RANGE_ERROR("native_require_handler: argument too long");
    }

    int modcount = sizeof(zjs_modules_array) / sizeof(module_t);
    for (int i = 0; i < modcount; i++) {
        module_t *mod = &zjs_modules_array[i];
        if (!strcmp(mod->name, module)) {
            // We only want one instance of each module at a time
            if (mod->instance == 0) {
                mod->instance = mod->init();
            }
            return jerry_acquire_value(mod->instance);
        }
    }
    DBG_PRINT("Native module not found, searching for JavaScript module %s\n",
              module);
#ifdef ZJS_LINUX_BUILD
    // Linux can pass in the script at runtime, so we have to read in/parse any
    // JS modules now rather than at compile time
    char full_path[size + 9];
    char *str;
    uint32_t len;
    sprintf(full_path, "modules/%s", module);
    full_path[size + 8] = '\0';

    if (zjs_read_script(full_path, &str, &len)) {
        ERR_PRINT("could not read module %s\n", full_path);
        return NOTSUPPORTED_ERROR("native_require_handler: could not read module script");
    }
    ZVAL code_eval = jerry_parse((jerry_char_t *)str, len, false);
    if (jerry_value_has_error_flag(code_eval)) {
        return SYSTEM_ERROR("native_require_handler: could not parse javascript");
    }
    ZVAL result = jerry_run(code_eval);
    if (jerry_value_has_error_flag(result)) {
        return SYSTEM_ERROR("native_require_handler: could not run javascript");
    }

    zjs_free_script(str);
#endif

    ZVAL global_obj = jerry_get_global_object();
    ZVAL modules_obj = zjs_get_property(global_obj, "module");

    if (!jerry_value_is_object(modules_obj)) {
        return SYSTEM_ERROR("native_require_handler: modules object not found");
    }

    ZVAL exports_obj = zjs_get_property(modules_obj, "exports");
    if (!jerry_value_is_object(exports_obj)) {
        return SYSTEM_ERROR("native_require_handler: exports object not found");
    }

    for (int i = 0; i < 4; i++) {
        // Strip the ".js"
        module[size-i] = '\0';
    }

    ZVAL found_obj = zjs_get_property(exports_obj, module);
    if (!jerry_value_is_object(found_obj)) {
        return NOTSUPPORTED_ERROR("native_require_handler: module not found");
    }

    DBG_PRINT("JavaScript module %s loaded\n", module);
    return jerry_acquire_value(found_obj);
}

// native eval handler
static ZJS_DECL_FUNC(native_eval_handler)
{
    return zjs_error("eval not supported");
}

// native print handler
static ZJS_DECL_FUNC(native_print_handler)
{
    if (argc < 1 || !jerry_value_is_string(argv[0]))
        return zjs_error("print: missing string argument");

    jerry_size_t size = 0;
    char *str = zjs_alloc_from_jstring(argv[0], &size);
    if (!str)
        return zjs_error("print: out of memory");

    ZJS_PRINT("%s\n", str);
    zjs_free(str);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(stop_js_handler)
{
#ifdef CONFIG_BOARD_ARDUINO_101
#ifdef CONFIG_IPM
    zjs_ipm_free_callbacks();
#endif
#endif
    zjs_modules_cleanup();
    jerry_cleanup();
    return ZJS_UNDEFINED;
}

void zjs_modules_init()
{
    // Add module.exports to global namespace
    ZVAL global_obj = jerry_get_global_object();
    ZVAL modules_obj = jerry_create_object();
    ZVAL exports_obj = jerry_create_object();

    zjs_set_property(modules_obj, "exports", exports_obj);
    zjs_set_property(global_obj, "module", modules_obj);

    // Todo: find a better solution to disable eval() in JerryScript.
    // For now, just inject our eval() function in the global space
    zjs_obj_add_function(global_obj, native_eval_handler, "eval");
    zjs_obj_add_function(global_obj, native_print_handler, "print");
    zjs_obj_add_function(global_obj, stop_js_handler, "stopJS");

    // create the C handler for require JS call
    zjs_obj_add_function(global_obj, native_require_handler, "require");

    // auto-load the events module without waiting for require(); needed so its
    //   init function will run before it's used by UART, etc.
    int modcount = sizeof(zjs_modules_array) / sizeof(module_t);
    for (int i = 0; i < modcount; i++) {
        module_t *mod = &zjs_modules_array[i];

        // DEV: if you add another module name here, remove the break below
        if (!strcmp(mod->name, "events")) {
            mod->instance = jerry_acquire_value(mod->init());
            break;
        }
    }
    zjs_init_callbacks();
    // initialize fixed modules
    zjs_error_init();
    zjs_timers_init();
#ifdef BUILD_MODULE_CONSOLE
    zjs_console_init();
#endif
#ifdef BUILD_MODULE_BUFFER
    zjs_buffer_init();
#endif
#ifdef BUILD_MODULE_SENSOR
    zjs_sensor_init();
#endif
}

void zjs_modules_cleanup()
{
    // stop timers first to prevent further calls
    zjs_timers_cleanup();

    int modcount = sizeof(zjs_modules_array) / sizeof(module_t);
    for (int i = 0; i < modcount; i++) {
        module_t *mod = &zjs_modules_array[i];
        if (mod->instance) {
            if (mod->cleanup) {
                mod->cleanup();
            }
            jerry_release_value(mod->instance);
            mod->instance = 0;
        }
    }

    // clean up fixed modules
    zjs_error_cleanup();
#ifdef BUILD_MODULE_CONSOLE
    zjs_console_cleanup();
#endif
#ifdef BUILD_MODULE_BUFFER
    zjs_buffer_cleanup();
#endif
#ifdef BUILD_MODULE_SENSOR
    zjs_sensor_cleanup();
#endif
}

void zjs_register_service_routine(void *handle, zjs_service_routine func)
{
    if (num_routines >= NUM_SERVICE_ROUTINES) {
        DBG_PRINT(("not enough space, increase NUM_SERVICE_ROUTINES\n"));
        return;
    }
    svc_routine_map[num_routines].handle = handle;
    svc_routine_map[num_routines].func = func;
    num_routines++;
    return;
}

int32_t zjs_service_routines(void)
{
    int32_t wait = ZJS_TICKS_FOREVER;
    int i;
    for (i = 0; i < num_routines; ++i) {
        int32_t ret = svc_routine_map[i].func(svc_routine_map[i].handle);
        wait = (wait < ret) ? wait : ret;
    }
#ifdef ZJS_LINUX_BUILD
    if (wait == ZJS_TICKS_FOREVER) {
        return 0;
    }
#endif
    return wait;
}
