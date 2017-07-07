// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __zjs_util_h__
#define __zjs_util_h__

// The util code is only for the X86 side

#include <stdlib.h>
#include "jerryscript.h"
#include "zjs_common.h"
#include "zjs_error.h"

#define ZJS_UNDEFINED jerry_create_undefined()

#ifdef DEBUG_BUILD
#define ZJS_HIDDEN_PROP(n) n
#else
#define ZJS_HIDDEN_PROP(n) "\377" n
#endif


/**
 * Call malloc but if it fails, run JerryScript garbage collection and retry
 *
 * @param size  Number of bytes to allocate
 */
void *zjs_malloc_with_retry(size_t size);

#ifdef ZJS_LINUX_BUILD
#define zjs_malloc(sz) malloc(sz)
#define zjs_free(ptr) free(ptr)
#else
#include <zephyr.h>
#ifdef ZJS_TRACE_MALLOC
#define zjs_malloc(sz) ({void *zjs_ptr = zjs_malloc_with_retry(sz); ZJS_PRINT("%s:%d: allocating %lu bytes (%p)\n", __func__, __LINE__, (u32_t)sz, zjs_ptr); zjs_ptr;})
#define zjs_free(ptr) (ZJS_PRINT("%s:%d: freeing %p\n", __func__, __LINE__, ptr), free(ptr))
#else
#define zjs_malloc(sz) ({void *zjs_ptr = zjs_malloc_with_retry(sz); if (!zjs_ptr) {ERR_PRINT("malloc failed\n");} zjs_ptr;})
#define zjs_free(ptr) free(ptr)
#endif  // ZJS_TRACE_MALLOC
#endif  // ZJS_LINUX_BUILD

void zjs_set_property(const jerry_value_t obj, const char *name,
                      const jerry_value_t prop);
void zjs_set_readonly_property(const jerry_value_t obj, const char *name,
                               const jerry_value_t prop);
jerry_value_t zjs_get_property (const jerry_value_t obj, const char *name);
bool zjs_delete_property(const jerry_value_t obj, const char *str);

typedef struct zjs_native_func {
    void *function;
    const char *name;
} zjs_native_func_t;

/**
 * Add a series of functions described in array funcs to obj.
 *
 * @param obj    JerryScript object to add the functions to
 * @param funcs  Array of zjs_native_func_t, terminated with {NULL, NULL}
 */
void zjs_obj_add_functions(jerry_value_t obj, zjs_native_func_t *funcs);

void zjs_obj_add_boolean(jerry_value_t obj, bool flag, const char *name);
void zjs_obj_add_readonly_boolean(jerry_value_t obj, bool flag,
                                  const char *name);
void zjs_obj_add_function(jerry_value_t obj, void *function, const char *name);
void zjs_obj_add_object(jerry_value_t parent, jerry_value_t child,
                        const char *name);
void zjs_obj_add_string(jerry_value_t obj, const char *str, const char *name);
void zjs_obj_add_readonly_string(jerry_value_t obj, const char *str,
                                 const char *name);
void zjs_obj_add_number(jerry_value_t obj, double num, const char *name);
void zjs_obj_add_readonly_number(jerry_value_t obj, double num,
                                 const char *name);

bool zjs_obj_get_boolean(jerry_value_t obj, const char *name, bool *flag);
bool zjs_obj_get_string(jerry_value_t obj, const char *name, char *buffer,
                        int len);
bool zjs_obj_get_double(jerry_value_t obj, const char *name, double *num);
bool zjs_obj_get_uint32(jerry_value_t obj, const char *name, u32_t *num);
bool zjs_obj_get_int32(jerry_value_t obj, const char *name, s32_t *num);

/**
 * Copy a JerryScript string into a supplied char * buffer.
 *
 * @param jstr    A JerryScript string value.
 * @param buffer  A char * buffer with at least *maxlen bytes.
 * @param maxlen  Pointer to a maximum size to be written to the buffer. If the
 *                  string size with a null terminator would exceed *maxlen,
 *                  only a null terminator will be written to the buffer and
 *                  *maxlen will be set to 0. If the string is successfully
 *                  copied, *maxlen will be set to the bytes copied (not
 *                  counting the null terminator). If *maxlen is 0, behavior is
 *                  undefined.
 */
void zjs_copy_jstring(jerry_value_t jstr, char *buffer, jerry_size_t *maxlen);

/**
 * Allocate a char * buffer on the heap and copy the JerryScript string to it.
 *
 * @param jstr    A JerryScript string value.
 * @param maxlen  Pointer to a maximum size for the returned string. If NULL or
 *                  pointing to 0, there is no limit to the string size
 *                  returned. If not NULL, the actual length of the string will
 *                  be written to *maxlen. If the call succeeds, the buffer
 *                  returned will be truncated to the given maxlen with a null
 *                  terminator. You can use zjs_copy_jstring if you'd rather
 *                  fail than truncate on error.
 * @return A new null-terminated string (which must be freed with zjs_free) or
 *          NULL on failure.
 */
char *zjs_alloc_from_jstring(jerry_value_t jstr, jerry_size_t *maxlen);

bool zjs_hex_to_byte(const char *buf, u8_t *byte);

void zjs_default_convert_pin(u32_t orig, int *dev, int *pin);

u16_t zjs_compress_32_to_16(u32_t num);
u32_t zjs_uncompress_16_to_32(u16_t num);

void zjs_print_error_message(jerry_value_t error, jerry_value_t func);

/**
 * Macro to declare a standard JerryScript external function in a shorter form
 *
 * @param name  The name of the function to declare
 *
 * Note: This hides the fact that you have function_obj, this, argv, and argc
 *   variables available in the function.
 *
 * Example:
 * static ZJS_DECL_FUNC(zjs_my_api)
 * {
 *     // do something useful; often using those hidden args
 *     return ZJS_UNDEFINED;
 * };
 */
#define ZJS_DECL_FUNC(name)                               \
    jerry_value_t name(const jerry_value_t function_obj,  \
                       const jerry_value_t this,          \
                       const jerry_value_t argv[],        \
                       const jerry_length_t argc)

/**
 * Macro to declare a function that takes the JerryScript args plus more
 *
 * @param name  The name of the function to declare
 * @param ...   List of other arguments to declare after the standard ones
 *
 * Example:
 * static ZJS_DECL_FUNC_ARGS(zjs_my_api_with_args, int mode)
 * {
 *     // do something useful, using mode and often the hidden args
 *     return ZJS_UNDEFINED;
 * };
 */
#define ZJS_DECL_FUNC_ARGS(name, ...)                     \
    jerry_value_t name(const jerry_value_t function_obj,  \
                       const jerry_value_t this,          \
                       const jerry_value_t argv[],        \
                       const jerry_length_t argc,         \
                       __VA_ARGS__)

/**
 * Macro to call a function declared with ZJS_DECL_FUNC_ARGS from another API
 *
 * @param name  The name of the function to call
 * @param ...   List of other arguments to pass
 *
 * Example:
 * jerry_value_t rval = ZJS_CHAIN_FUNC_ARGS(zjs_my_api_with_args, 1);
 */
#define ZJS_CHAIN_FUNC_ARGS(name, ...)                 \
    name(function_obj, this, argv, argc, __VA_ARGS__)

/**
 * Release a jerry_value_t passed by reference
 */
void zjs_free_value(const jerry_value_t *value);

/**
 * Macro to declare a jerry_value_t and have it released automatically
 *
 * If you're going to be returning the value from a function, you usually don't
 * want this, as ownership will transfer to the caller (unless you acquire it on
 * return). However, if you have error return paths that should release the
 * value, then it might still be a good idea.
 *
 * Also, don't use this for a variable that holds a C copy of a value that was
 * passed into you, as those are owned by the caller.
 */
#define ZVAL const jerry_value_t __attribute__ ((__cleanup__(zjs_free_value)))

/**
 * A non-const version of ZVAL
 *
 * This is for when you need to initialize a ZVAL from more than one path. It
 * should be used sparingly, because this is less safe; it's possible to
 * overwrite a value and forget to release the old one.
 */
#define ZVAL_MUTABLE jerry_value_t __attribute__ ((__cleanup__(zjs_free_value)))

//
// ztypes (for argument validation)
//

// Z_OPTIONAL means the argument isn't required, this must come first if present
#define Z_OPTIONAL  "?"

// Z_ANY matches any type (i.e. ignores it) - only makes sense for required arg
#define Z_ANY       "a"
// the rest all match specific type by calling jerry_value_is_* function
#define Z_ARRAY     "b"
#define Z_BOOL      "c"
#define Z_FUNCTION  "d"
#define Z_NULL      "e"
#define Z_NUMBER    "f"
// NOTE: Z_OBJECT will match arrays and functions too, because they are objects
#define Z_OBJECT    "g"
#define Z_STRING    "h"
// NOTE: If this test passes, you're guaranteed zjs_buffer_find will succeed
#define Z_BUFFER    "i"
#define Z_UNDEFINED "j"

enum {
    ZJS_VALID_REQUIRED,
    ZJS_VALID_OPTIONAL,
    ZJS_SKIP_OPTIONAL,
    ZJS_INTERNAL_ERROR = -1,
    ZJS_INVALID_ARG = -2,
    ZJS_INSUFFICIENT_ARGS = -3
};

int zjs_validate_args(const char *expectations[], const jerry_length_t argc,
                      const jerry_value_t argv[]);

/**
 * Macro to validate existing argv based on a list of expected argument types.
 *
 * NOTE: Expects this, function_obj, argc and argv to exist as in a JerryScript
 *       native function.
 *
 * @param optcount  A pointer to an int to receive count of optional args found,
 *                    or NULL if not needed.
 * @param offset    Integer offset of arg in argv to start with (normally 0)
 * @param typestr   Each remaining comma-separated argument to the macro
 *                    corresponds to an argument in argv; each argument should
 *                    be a space-separated list of "ztypes" from defines above.
 *
 * Example: ZJS_VALIDATE_ARGS(Z_NUMBER, Z_OBJECT Z_NULL, Z_OPTIONAL Z_FUNCTION);
 * This requires argv[0] to be a number type, argv[1] to be an object type
 *   or null,  and argv[2] may or may not exist, but if it does it must be a
 *   function type. (Implicitly, argc will have to be at least 2.) Arguments
 *   beyond what are specified are allowed and ignored.
 */
#define ZJS_VALIDATE_ARGS_FULL(optcount, offset, ...)                       \
    int optcount = zjs_validate_args((const char *[]){ __VA_ARGS__, NULL }, \
                                     argc - offset, argv + offset);         \
    if (optcount <= ZJS_INVALID_ARG) {                                      \
        return TYPE_ERROR("invalid arguments");                             \
    }

// Use this if you need an offset
#define ZJS_VALIDATE_ARGS_OFFSET(offset, ...)           \
    ZJS_VALIDATE_ARGS_FULL(zjs_validate_rval, offset, __VA_ARGS__)

// Use this if you need the number of optional args found
#define ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, ...)       \
    ZJS_VALIDATE_ARGS_FULL(optcount, 0, __VA_ARGS__)

// Normally use this as a shortcut
#define ZJS_VALIDATE_ARGS(...)                      \
    ZJS_VALIDATE_ARGS_FULL(zjs_validate_rval, 0, __VA_ARGS__)

/**
 * Macro to check if the argument list was expected, but not return an error.
 * This is useful if you want to reject a promise based on bad arguments instead
 * of automatically returning an error.
 *
 * NOTE: Expects argc and argv to exist as in a JerryScript native function
 */
#define ZJS_CHECK_ARGS(...) \
    (zjs_validate_args((const char *[]){ __VA_ARGS__, NULL }, argc, argv) \
     <= ZJS_INVALID_ARG) ? 1 : 0

/**
 * Checks for a boolean property and returns it via result.
 *
 * Returns TypeError if property is not a boolean.
 *
 * @param value   A valid JS object.
 * @param prop    A string property name.
 * @param result  Receives result; should already be set to default.
 *
 * @return 0 on success, negative number on failure
 */
int zjs_require_bool_if_prop(jerry_value_t obj, const char *prop, bool *result);

/**
 * Returns an error from calling function if obj.prop exists but is not a
 *   boolean.
 */
#define ZJS_REQUIRE_BOOL_IF_PROP(value, prop, result)         \
    if (zjs_require_bool_if_prop(value, prop, result) < 0) {  \
        return TYPE_ERROR("bool required");                   \
    }

typedef struct str2int {
    const char *first;
    int second;
} str2int_t;

/**
 * Checks for a string property obj.prop and maps it to an int via map array.
 *
 * @param obj     A valid JS object.
 * @param prop    A string property name.
 * @param map     An array of mappings from string to int; final one with NULL.
 * @param maxlen  The max length of string to expect in the property.
 * @param result  Receives results; should already be set to default.
 *
 *  *result will hold the mapped value if a match is found; if no such
 *  property exists, *result will be untouched; if the property is not a
 *  string or holds a value that doesn't match, returns a constant above.
 *
 * @return 0 on success, negative number on failure
 */
int zjs_require_string_if_prop_map(jerry_value_t obj, const char *prop,
                                   str2int_t map[], int maxlen, int *result);

/**
 * Returns an error from calling function if obj.prop exists but is not a
 *   string, or if string is not one of the allowed values in map.
 */
#define ZJS_REQUIRE_STR_IF_PROP_MAP(obj, prop, map, maxlen, result)  \
    {                                                                \
        int rval = zjs_require_string_if_prop_map(obj, prop, map,    \
                                                  maxlen, result);   \
        if (rval < 0) {                                              \
            return TYPE_ERROR("one of specific strings required");   \
        }                                                            \
    }

void free_handle_nop(void *h);

#ifndef ZJS_LINUX_BUILD
#ifndef ZJS_ASHELL
/*
 * Unblock the main loop
 */
void zjs_loop_unblock(void);

/*
 * Block in the main loop for a specified amount of time
 */
void zjs_loop_block(int time);

/*
 * Initialize the main loop blocking semaphore
 */
void zjs_loop_init(void);
#else
#define zjs_loop_unblock() do {} while(0)
#define zjs_loop_block(time) do {} while(0)
#define zjs_loop_init() do {} while(0)
#endif
#endif

// Type definition to be used with macros below
// struct list_item {
//     int value;
//     struct list_item *next;
// } list_item_t;
// list_item_t *my_list = NULL;

// Looks for and returns a node found in a list.
// @param type          Struct type of list
// @param list          Pointer to list head
// @param cmp_element   Name of element in node struct to compare
// @param cmp_to        Value to compare node element to

// Example:
//    list_item_t *item = ZJS_FIND_NODE(list_item_t, my_list, value, 42);

// The above will return a list item whos `value` parameter == 42
#define ZJS_LIST_FIND(type, list, cmp_element, cmp_to) \
   ({ \
       type *found = NULL; \
       type *cur = list; \
       while (cur) { \
           if (cur->cmp_element == cmp_to) { \
               found = cur; \
               break; \
           } \
           cur = cur->next; \
       } \
       found; \
   })

// Append a new node to the end of a list
// Example:
//    list_item_t *item = new_list_item(...);
//    ZJS_APPEND_NODE(list_item_t, my_list, item);
#define ZJS_LIST_APPEND(type, list, p) \
    { \
        type **pnext = &list; \
        while (*pnext) { \
            pnext = &(*pnext)->next; \
        } \
        *pnext = p; \
    }

// Prepend a new node to the beginning of a list
// Example:
//    list_item_t *item = new_list_item(...);
//    ZJS_PREPEND_NODE(list_item_t, my_list, item);
#define ZJS_LIST_PREPEND(type, list, p) \
    { \
        p->next = list; \
        list = p; \
    }

// Remove a node from a list. Returns 1 if the node was removed.
// Example:
//     void remove(list_item_t *to_remove) {
//         ZJS_REMOVE_NODE(list_item_t, my_list, to_remove);
//     }
#define ZJS_LIST_REMOVE(type, list, p) \
    ({ \
        u8_t removed = 0; \
        type *cur = list; \
        if (p == list) { \
            list = p->next; \
        } else { \
            while (cur->next) { \
                if (cur->next == p) { \
                    cur->next = p->next; \
                    removed = 1; \
                    break; \
                } \
                cur = cur->next; \
            } \
        } \
        removed; \
    })

// Free and iterate over a linked list, calling a callback for each list item
// and removing the current item at each iteration.
#define ZJS_LIST_FREE(type, list, callback) \
    { \
        while (list) { \
            type *tmp = list->next; \
            callback(list); \
            list = tmp; \
        } \
    }

// Get the length of a linked list
#define ZJS_LIST_LENGTH(type, list) \
    ({ \
        int ret = 0; \
        type *i = list; \
        while (i) { \
            ret++; \
            i = i->next; \
        } \
        ret; \
    })

#define ZJS_GET_HANDLE(obj, type, var, info) \
    type *var; \
    { \
        void *native; \
        const jerry_object_native_info_t *tmp; \
        if (!jerry_get_object_native_pointer(obj, &native, &tmp)) { \
            return zjs_error("no native handle"); \
        } \
        if (tmp != &info) { \
            return zjs_error("handle was incorrect type"); \
        } \
        var = (type *)native; \
    }

// FIXME: Quick hack to allow context into the regular macro while fixing build
//        for call sites without JS binding context
#define ZJS_GET_HANDLE_ALT(obj, type, var, info) \
    type *var; \
    { \
        void *native; \
        const jerry_object_native_info_t *tmp; \
        if (!jerry_get_object_native_pointer(obj, &native, &tmp)) { \
            return zjs_error_context("no native handle", 0, 0); \
        } \
        if (tmp != &info) { \
            return zjs_error_context("handle was incorrect type", 0, 0);  \
        } \
        var = (type *)native; \
    }

#endif  // __zjs_util_h__
