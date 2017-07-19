// Copyright (c) 2016-2017, Intel Corporation.

// C includes
#include <string.h>

// ZJS includes
#include "zjs_callbacks.h"
#include "zjs_event.h"
#include "zjs_util.h"

#define ZJS_MAX_EVENT_NAME_SIZE 24
#define DEFAULT_MAX_LISTENERS   10

static jerry_value_t zjs_event_emitter_prototype;

typedef struct listener {
    jerry_value_t func;
    struct listener *next;
} listener_t;

// allocate sizeof(event_t) + len(name)
typedef struct event {
    struct event *next;
    listener_t *listeners;
    int namelen;
    char name[1];
} event_t;

typedef struct emitter {
    int max_listeners;
    event_t *events;
} emitter_t;

static void zjs_emitter_free_cb(void *native)
{
    emitter_t *handle = (emitter_t *)native;
    event_t *event = handle->events;
    while (event) {
        ZJS_LIST_FREE(listener_t, event->listeners, zjs_free);
        event = event->next;
    }
    ZJS_LIST_FREE(event_t, handle->events, zjs_free);
    zjs_free(handle);
}

static const jerry_object_native_info_t emitter_type_info = {
    .free_cb = zjs_emitter_free_cb
};

typedef struct event_trigger {
    void *handle;
    zjs_post_event post;
} event_trigger_t;

typedef struct event_names {
    jerry_value_t name_array;
    int idx;
} event_names_t;

// TODO: should be able to remove this and call post directly
void post_event(void *h, jerry_value_t ret_val)
{
    event_trigger_t *trigger = (event_trigger_t *)h;
    if (trigger) {
        if (trigger->post) {
            trigger->post(trigger->handle);
        }
        zjs_free(trigger);
    }
}

static u32_t get_num_events(jerry_value_t emitter)
{
    ZVAL val = zjs_get_property(emitter, "numEvents");
    if (!jerry_value_is_number(val)) {
        ERR_PRINT("emitter had no numEvents property\n");
        return 0;
    }
    u32_t num = jerry_get_number_value(val);
    return num;
}

static u32_t get_max_event_listeners(jerry_value_t emitter)
{
    ZVAL val = zjs_get_property(emitter, "maxListeners");
    if (!jerry_value_is_number(val)) {
        ERR_PRINT("emitter had no maxListeners property\n");
        return 0;
    }
    u32_t num = jerry_get_number_value(val);
    return num;
}

static s32_t get_callback_id(jerry_value_t event_obj)
{
    s32_t callback_id = -1;
    ZVAL id_prop = zjs_get_property(event_obj, "callback_id");
    if (jerry_value_is_number(id_prop)) {
        // If there already is an event object, get the callback ID
        zjs_obj_get_int32(event_obj, "callback_id", &callback_id);
    }
    return callback_id;
}

static int compare_name(event_t *event, const char *name)
{
    return strncmp(event->name, name, event->namelen);
}

jerry_value_t zjs_add_event_listener(jerry_value_t obj, const char *event_name,
                                     jerry_value_t func)
{
    // requires: event is an event name; func is a valid JS function
    //  returns: an error, or 0 value on success
    ZJS_PRINT("ADD EVENT LISTENER\n");
    ZJS_GET_HANDLE_ALT(obj, emitter_t, handle, emitter_type_info);

    bool added = false;
    event_t *event = ZJS_LIST_FIND_CMP(event_t, handle->events, compare_name,
                                       event_name);
    if (!event) {
        int len = strlen(event_name);
        event = (event_t *)zjs_malloc(sizeof(event_t) + len);
        if (event) {
            event->next = NULL;
            event->listeners = NULL;
            event->namelen = len;
            strcpy(event->name, event_name);
        }
        added = true;
    }

    listener_t *listener = zjs_malloc(sizeof(listener_t));
    if (listener) {
        listener->func = func;
        listener->next = NULL;
    }

    if (!event || !listener) {
        if (added) {
            zjs_free(event);
        }
        zjs_free(listener);
        return zjs_error_context("out of memory", 0, 0);
    }

    ZJS_LIST_APPEND(event_t, handle->events, event);
    ZJS_LIST_APPEND(listener_t, event->listeners, listener);

    if (ZJS_LIST_LENGTH(listener_t, event->listeners) > handle->max_listeners) {
        // warn of possible leak as per Node docs
        ZJS_PRINT("possible memory leak on event %s", event_name);
    }

#ifdef ZJS_FIND_FUNC_NAME
    char name[strlen(event) + sizeof("event: ")];
    sprintf(name, "event: %s", event);
    zjs_obj_add_string(func, name, ZJS_HIDDEN_PROP("function_name"));
#endif

    return 0;
}

static ZJS_DECL_FUNC(add_listener)
{
    // args: event name, callback
    ZJS_PRINT("ADD LISTENER\n");

    ZJS_VALIDATE_ARGS(Z_STRING, Z_FUNCTION);

    char *name = zjs_alloc_from_jstring(argv[0], NULL);
    if (!name) {
        return zjs_error("out of memory");
    }

    jerry_value_t rval = zjs_add_event_listener(this, name, argv[1]);
    zjs_free(name);

    if (jerry_value_has_error_flag(rval)) {
        return rval;
    }

    // return this object to allow chaining
    return jerry_acquire_value(this);
}

static ZJS_DECL_FUNC(emit_event)
{
    // args: event name[, additional pass-through args]
    ZJS_VALIDATE_ARGS(Z_STRING);

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char event[size];
    zjs_copy_jstring(argv[0], event, &size);
    if (!size) {
        return zjs_error("event name is too long");
    }

    // FIXME: This is supposed to return true if there were listeners; false,
    //   otherwise. Check that it's behaving correctly. Doesn't look like it.
    return jerry_create_boolean(zjs_trigger_event(this, event, argv + 1,
                                                  argc - 1, NULL, NULL));
}

static ZJS_DECL_FUNC(remove_listener)
{
    // args: event name, callback
    ZJS_PRINT("REMOVE LISTENER\n");
    ZJS_VALIDATE_ARGS(Z_STRING, Z_FUNCTION);

    ZVAL event_emitter = zjs_get_property(this, ZJS_HIDDEN_PROP("event"));

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char event[size];
    zjs_copy_jstring(argv[0], event, &size);
    if (!size) {
        return zjs_error("event name is too long");
    }

    ZVAL map = zjs_get_property(event_emitter, "map");
    ZVAL event_obj = zjs_get_property(map, event);

    // Event object to hold callback ID and eventually listener arguments
    if (!jerry_value_is_object(event_obj)) {
        ERR_PRINT("event object not found\n");
        return ZJS_UNDEFINED;
    }

    s32_t callback_id = get_callback_id(event_obj);
    if (callback_id != -1) {
        zjs_remove_callback_list_func(callback_id, argv[1]);
    } else {
        ERR_PRINT("callback_id not found for '%s'\n", event);
    }

    return jerry_acquire_value(this);
}

static ZJS_DECL_FUNC(remove_all_listeners)
{
    // args: event name
    ZJS_PRINT("REMOVE ALL LISTENERS\n");
    ZJS_VALIDATE_ARGS(Z_STRING);

    ZVAL event_emitter = zjs_get_property(this, ZJS_HIDDEN_PROP("event"));

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char event[size];
    zjs_copy_jstring(argv[0], event, &size);
    if (!size) {
        return zjs_error("event name is too long");
    }

    ZVAL map = zjs_get_property(event_emitter, "map");
    ZVAL event_obj = zjs_get_property(map, event);

    // Event object to hold callback ID and eventually listener arguments
    if (!jerry_value_is_object(event_obj)) {
        ERR_PRINT("event object not found\n");
        return ZJS_UNDEFINED;
    }

    s32_t callback_id = get_callback_id(event_obj);
    if (callback_id != -1) {
        zjs_remove_callback(callback_id);

        ZVAL name = jerry_create_string((const jerry_char_t *)event);
        jerry_delete_property(map, name);
    } else {
        ERR_PRINT("callback_id not found for '%s'\n", event);
    }

    zjs_obj_add_number(event_emitter, 0, "numEvents");

    return jerry_acquire_value(this);
}

bool foreach_event_name(const jerry_value_t prop_name,
                        const jerry_value_t prop_value,
                        void *data)
{
    ZJS_PRINT("FOREACH EVENT NAME\n");
    event_names_t *names = (event_names_t *)data;

    jerry_set_property_by_index(names->name_array, names->idx++, prop_name);
    return true;
}

static ZJS_DECL_FUNC(get_event_names)
{
    ZJS_PRINT("GET EVENT NAMES\n");
    event_names_t names;

    ZVAL event_emitter = zjs_get_property(this, ZJS_HIDDEN_PROP("event"));
    u32_t num_events = get_num_events(event_emitter);
    ZVAL map = zjs_get_property(event_emitter, "map");

    names.idx = 0;
    names.name_array = jerry_create_array(num_events);

    jerry_foreach_object_property(map, foreach_event_name, &names);

    return names.name_array;
}

static ZJS_DECL_FUNC(get_max_listeners)
{
    ZJS_PRINT("GET MAX LISTENERS\n");
    ZVAL event_emitter = zjs_get_property(this, ZJS_HIDDEN_PROP("event"));
    u32_t max_listeners = get_max_event_listeners(event_emitter);
    return jerry_create_number(max_listeners);
}

static ZJS_DECL_FUNC(set_max_listeners)
{
    // args: max count
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    ZVAL event_emitter = zjs_get_property(this, ZJS_HIDDEN_PROP("event"));

    double num = jerry_get_number_value(argv[0]);
    if (num < 0) {
        return zjs_error("max listener value must be a positive integer");
    }
    zjs_obj_add_number(event_emitter, num, "maxListeners");

    return jerry_acquire_value(this);
}

static ZJS_DECL_FUNC(get_listener_count)
{
    // args: event name
    ZJS_PRINT("GET LISTENER COUNT\n");
    ZJS_VALIDATE_ARGS(Z_STRING);

    ZVAL event_emitter = zjs_get_property(this, ZJS_HIDDEN_PROP("event"));

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char event[size];
    zjs_copy_jstring(argv[0], event, &size);
    if (!size) {
        return zjs_error("event name is too long");
    }

    ZVAL map = zjs_get_property(event_emitter, "map");
    ZVAL event_obj = zjs_get_property(map, event);

    if (!jerry_value_is_object(event_obj)) {
        return jerry_create_number(0);
    }

    s32_t callback_id = get_callback_id(event_obj);
    int count = 0;
    if (callback_id != -1) {
        count = zjs_get_num_callbacks(callback_id);
    } else {
        ERR_PRINT("callback_id not found for '%s'\n", event);
    }

    return jerry_create_number(count);
}

static ZJS_DECL_FUNC(get_listeners)
{
    // args: event name
    ZJS_PRINT("GET LISTENERS\n");
    ZJS_VALIDATE_ARGS(Z_STRING);

    ZVAL event_emitter = zjs_get_property(this, ZJS_HIDDEN_PROP("event"));

    jerry_size_t size = ZJS_MAX_EVENT_NAME_SIZE;
    char event[size];
    zjs_copy_jstring(argv[0], event, &size);
    if (!size) {
        return zjs_error("event name is too long");
    }

    ZVAL map = zjs_get_property(event_emitter, "map");
    ZVAL event_obj = zjs_get_property(map, event);

    if (!jerry_value_is_object(event_obj)) {
        return zjs_error("event object not found");
    }

    s32_t callback_id = get_callback_id(event_obj);
    if (callback_id == -1) {
        ERR_PRINT("callback_id not found for '%s'\n", event);
        return ZJS_UNDEFINED;
    }

    int count;
    int i;
    // not using ZVAL because this is a pointer to values and one we will return
    jerry_value_t *func_array = zjs_get_callback_func_list(callback_id, &count);
    jerry_value_t ret_array = jerry_create_array(count);
    for (i = 0; i < count; ++i) {
        jerry_set_property_by_index(ret_array, i, func_array[i]);
    }

    return ret_array;
}

zjs_callback_id emit_id = -1;

typedef struct emit_event {
    jerry_value_t obj;
    zjs_pre_emit pre;
    zjs_post_emit post;
    u32_t length;  // length of user data
    char data[0];  // data is user data followed by null-terminated event name
} emit_event_t;

static void emit_event_callback(void *handle, const void *args) {
    ZJS_PRINT("EMIT EVENT CB\n");
    const emit_event_t *emit = (const emit_event_t *)args;
    ZJS_PRINT("*** obj = %p, pre = %p, post = %p, len = %d\n",
              emit->obj, emit->pre, emit->post, emit->length);

    // prepare arguments for the event
    jerry_value_t argv[MAX_EVENT_ARGS];
    jerry_value_t *argp = argv;
    u32_t argc = 0;
    if (emit->pre) {
        ZJS_PRINT("BEFORE...\n");
        emit->pre(argv, &argc, emit->data, emit->length);
        ZJS_PRINT("AFTER...\n");
    }
    ZJS_PRINT("NEXT...\n");
    if (argc == 0) {
        ZJS_PRINT("HERE...\n");
        argp = NULL;
    }
    ZJS_PRINT("argv[0]: %p, argp[0]: %p\n", argv[0], argp[0]);

    // emit the event
    zjs_emit_event(emit->obj, emit->data + emit->length, argp, argc);
    // TODO: possibly do something different depending on success/failure?

    // free args
    if (emit->post) {
        // TODO: figure out what is needed for args here
        emit->post(argv);
    }
}

void zjs_defer_emit_event(jerry_value_t obj, const char *event,
                          const void *buffer, int bytes,
                          zjs_pre_emit pre, zjs_post_emit post)
{
    ZJS_PRINT("DEFER EMIT\n");
    // requires: don't exceed MAX_EVENT_ARGS in pre function
    //  effects: threadsafe way to schedule an event to be triggered from the
    //             main thread in the next event loop pass
    int namelen = strlen(event);
    int len = sizeof(emit_event_t) + namelen + 1 + bytes;
    char buf[len];
    emit_event_t *emit = (emit_event_t *)buf;
    emit->obj = obj;
    emit->pre = pre;
    emit->post = post;
    emit->length = bytes;
    memcpy(emit->data, buffer, bytes);
    strcpy(emit->data + bytes, event);
    ZJS_PRINT("EMIT ID: %d\n", emit_id);
    zjs_signal_callback(emit_id, buf, len);
}

bool zjs_emit_event(jerry_value_t obj, const char *event_name,
                    const jerry_value_t argv[], u32_t argc)
{
    ZJS_PRINT("EMIT EVENT\n");
    // effects: emits event now, should only be called from main thread
    ZJS_GET_HANDLE_OR_NULL(obj, emitter_t, handle, emitter_type_info);
    if (!handle) {
        ERR_PRINT("no handle found\n");
        return false;
    }

    // find the event among our defined events
    event_t *event = ZJS_LIST_FIND_CMP(event_t, handle->events, compare_name,
                                       event_name);
    if (!event) {
        ZJS_PRINT("Warning: event not found: %s\n", event_name);
        return false;
    }

    ZJS_PRINT("argv[0]: %p, argc: %d\n", argv[0], argc);

    // call the listeners in order
    listener_t *listener = event->listeners;
    while (listener) {
        ZVAL rval = jerry_call_function(listener->func, obj, argv, argc);
        listener = listener->next;
    }
    return true;
}

bool zjs_trigger_event(jerry_value_t obj,
                       const char *event,
                       const jerry_value_t argv[],
                       u32_t argc,
                       zjs_post_event post,
                       void *h)
{
    ZJS_PRINT("TRIGGER EMIT\n");
    ZVAL event_emitter = zjs_get_property(obj, ZJS_HIDDEN_PROP("event"));
    ZVAL map = zjs_get_property(event_emitter, "map");
    ZVAL event_obj = zjs_get_property(map, event);

    if (!jerry_value_is_object(event_obj)) {
        DBG_PRINT("event object not found\n");
        return false;
    }

    s32_t callback_id = get_callback_id(event_obj);
    if (callback_id == -1) {
        ERR_PRINT("callback_id not found\n");
        return false;
    }

    event_trigger_t *trigger = zjs_malloc(sizeof(event_trigger_t));
    if (!trigger) {
        ERR_PRINT("could not allocate trigger, out of memory\n");
        return false;
    }
    trigger->handle = h;
    trigger->post = post;

    zjs_edit_callback_handle(callback_id, trigger);

    zjs_signal_callback(callback_id, argv, argc * sizeof(jerry_value_t));

    DBG_PRINT("triggering event '%s', args_cnt=%lu, callback_id=%ld\n", event,
              argc, callback_id);

    return true;
}

bool zjs_trigger_event_now(jerry_value_t obj,
                           const char *event,
                           const jerry_value_t argv[],
                           u32_t argc,
                           zjs_post_event post,
                           void *h)
{
    ZJS_PRINT("TRIGGER NOW\n");
    ZVAL event_emitter = zjs_get_property(obj, ZJS_HIDDEN_PROP("event"));
    ZVAL map = zjs_get_property(event_emitter, "map");
    ZVAL event_obj = zjs_get_property(map, event);

    if (!jerry_value_is_object(event_obj)) {
        ERR_PRINT("event object not found\n");
        return false;
    }

    s32_t callback_id = get_callback_id(event_obj);
    if (callback_id == -1) {
        ERR_PRINT("callback_id not found\n");
        return false;
    }

    event_trigger_t *trigger = zjs_malloc(sizeof(event_trigger_t));
    if (!trigger) {
        ERR_PRINT("could not allocate trigger, out of memory\n");
        return false;
    }
    trigger->handle = h;
    trigger->post = post;

    zjs_edit_callback_handle(callback_id, trigger);

    zjs_call_callback(callback_id, argv, argc);

    return true;
}

void zjs_make_event(jerry_value_t obj, jerry_value_t prototype)
{
    ZVAL event_obj = jerry_create_object();

    zjs_obj_add_number(event_obj, DEFAULT_MAX_LISTENERS, "maxListeners");
    zjs_obj_add_number(event_obj, 0, "numEvents");

    ZVAL map = jerry_create_object();
    zjs_set_property(event_obj, "map", map);

    jerry_value_t proto = zjs_event_emitter_prototype;
    if (jerry_value_is_object(prototype)) {
        jerry_set_prototype(prototype, proto);
        proto = prototype;
    }
    jerry_set_prototype(obj, proto);

    zjs_obj_add_object(obj, event_obj, ZJS_HIDDEN_PROP("event"));

    emitter_t *emitter = zjs_malloc(sizeof(emitter_t));
    emitter->max_listeners = 10;
    emitter->events = NULL;
    jerry_set_object_native_pointer(obj, emitter, &emitter_type_info);
}

static ZJS_DECL_FUNC(event_constructor)
{
    jerry_value_t new_emitter = jerry_create_object();
    zjs_make_event(new_emitter, ZJS_UNDEFINED);
    return new_emitter;
}

jerry_value_t zjs_event_init()
{
    zjs_native_func_t array[] = {
        { add_listener, "on" },
        { add_listener, "addListener" },
        { emit_event, "emit" },
        { remove_listener, "removeListener" },
        { remove_all_listeners, "removeAllListeners" },
        { get_event_names, "eventNames" },
        { get_max_listeners, "getMaxListeners" },
        { get_listener_count, "listenerCount" },
        { get_listeners, "listeners" },
        { set_max_listeners, "setMaxListeners" },
        { NULL, NULL }
    };
    zjs_event_emitter_prototype = jerry_create_object();
    zjs_obj_add_functions(zjs_event_emitter_prototype, array);
    zjs_obj_add_number(zjs_event_emitter_prototype,
                       (double)DEFAULT_MAX_LISTENERS, "defaultMaxListeners");

    emit_id = zjs_add_c_callback(NULL, emit_event_callback);
    ZJS_PRINT("CREATE EMIT ID: %d\n", emit_id);

    return jerry_create_external_function(event_constructor);
}

void zjs_event_cleanup()
{
    jerry_release_value(zjs_event_emitter_prototype);

    // TODO: Clean up emit_id I guess
}
