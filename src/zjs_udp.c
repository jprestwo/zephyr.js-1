// Copyright (c) 2016, Intel Corporation.

#include <zephyr.h>
#include <net/ip_buf.h>
#include <net/net_core.h>
#include <net/net_socket.h>

#include <string.h>
#include <stdlib.h>

#include "zjs_common.h"
#include "zjs_util.h"
#include "zjs_udp.h"
#include "zjs_event.h"

/* Organization-local 239.192.0.0/14 */
#define MCAST_IPADDR4 { { { 239, 192, 0, 2 } } }
/* admin-local, dynamically allocated multicast address */
#define MCAST_IPADDR6 { { { 0xff, 0x84, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x2 } } }


typedef enum {
    TYPE_UDP4,
    TYPE_UDP6
} UdpType;

typedef struct {
    UdpType type;
    struct net_context *udp_recv;
    struct net_context *mcast_recv;
} udp_socket;

// print an IPv6 Zephyr structure in human readable format
static void print_ip6(struct in6_addr* ip6_addr)
{
    int i;
    for (i = 0; i < 8; i++) {
        ZJS_PRINT("%04x", ip6_addr->in6_u.u6_addr16[i]);
        if (i != 7) {
            ZJS_PRINT(":");
        }
    }
    ZJS_PRINT("\n");
}

/*
 * Get a 16 bit hex pair from a string
 */
static uint16_t hex_to_int(char* hex)
{
    char digits[4];
    uint8_t len = 0;
    char* i = hex;
    uint16_t ret = 0;

    // get/set the number of digits
    while (*i != ':' && *i != '\0') {
        digits[len++] = *i;
        i++;
    }

    // iterate over the digits and convert them to a 16 bit number
    int j;
    for (j = 0; j < len; j++) {
        uint8_t cur = 0;
        if (digits[j] >= '0' && digits[j] <= '9') {
            cur = digits[j] - '0';
        } else if (digits[j] >= 'a' && digits[j] <= 'f') {
            cur = digits[j] - 'a' + 10;
        } else if (digits[j] >= 'A' && digits[j] <= 'F') {
            cur = digits[j] - 'A' + 10;
        }
        ret = (ret << 4) | (cur & 0xF);
    }

    return ret;
}

// convert an IPv6 string to a Zephyr in6_addr struct
static void zjs_ip6_to_u16(const char* ip, struct in6_addr* in6addr)
{
    uint8_t idx = 0;
    memset(&in6addr->in6_u.u6_addr16, 0, sizeof(uint16_t) * 8);
    char* i = (char*)ip;

    // Set the first 16 bit pair
    in6addr->in6_u.u6_addr16[idx++] = hex_to_int(i);

    // start by getting as many 16 bit pairs until we find ::
    // if :: is never found then we have a un-shortened IPv6 string
    while (*i != '\0') {
        if (*i == ':') {
            if (*(i + 1) == ':') {
                // found ::, now fill in the end of the address in reverse
                goto Reverse;
            } else {
                in6addr->in6_u.u6_addr16[idx++] = hex_to_int(i + 1);
            }
        }
        i++;
    }
    return;

Reverse:
    idx = 7;
    uint8_t ridx = strlen(ip);
    char* j = (char*)ip + ridx - 1;
    // start at the end, back-filling the remaining 16 bit pairs until we get
    // to :: again. Anything in between is all zero's (set by memset)
    while (ridx--) {
        if (*j == ':' && *(j - 1) == ':') {
            in6addr->in6_u.u6_addr16[idx--] = hex_to_int(j + 1);
            break;
        } else if (*j == ':') {
            in6addr->in6_u.u6_addr16[idx--] = hex_to_int(j + 1);
        }
        j--;
    }
    return;
}

// convert an IPv4 string to a uint32_t
static void zjs_ip4_to_u32(const char* ip, struct in_addr* in4addr)
{
    uint8_t dots[3];
    int one, two, three, four;
    char* i = (char*)ip;
    uint8_t cnt = 0;
    while (*i != '\0') {
        if (*i == '.') {
            dots[cnt++] = i - ip + 1;
            *i = '\0';
        }
        i++;
    }
    one = atoi(ip);
    two = atoi(ip + dots[0]);
    three = atoi(ip + dots[1]);
    four = atoi(ip + dots[2]);

    in4addr->in4_u.u4_addr32[0] = ((uint8_t)one << 24) |
                                  ((uint8_t)two << 16) |
                                  ((uint8_t)three << 8) |
                                  ((uint8_t)four);
}

static void udp_recv(void* handle)
{
    udp_socket* sock = (udp_socket*)handle;
    struct net_buf *buf;

    buf = net_receive(sock->udp_recv, 0);
    if (buf) {
        ZJS_PRINT("GOT DATA!\n");
        return;
    }
}

static jerry_value_t udp_address(const jerry_value_t function_obj,
                                 const jerry_value_t this,
                                 const jerry_value_t argv[],
                                 const jerry_length_t argc)
{
    return ZJS_UNDEFINED;
}

static jerry_value_t udp_bind_socket(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc)
{
    uint32_t size = 40;
    char ip[size];
    uint32_t port = 0;
    jerry_value_t cb_val;
    jerry_value_t ip_val;

    if (argc >= 1) {
        if (jerry_value_is_object(argv[0])) {
            // options object
            zjs_obj_get_uint32(argv[0], "port", &port);
            ip_val = zjs_get_property(argv[0], "address");
            zjs_copy_jstring(ip_val, ip, &size);
            jerry_release_value(ip_val);
            if (argc == 2 && jerry_value_is_function(argv[1])) {
                // callback
                cb_val = argv[1];
            }
        } else if (jerry_value_is_number(argv[0])){
            // port
            port = jerry_get_number_value(argv[0]);
            if (argc == 2 && jerry_value_is_string(argv[1])) {
                // address
                zjs_copy_jstring(argv[1], ip, &size);
            }
            if (argc == 3 && jerry_value_is_function(argv[2])) {
                // callback
                cb_val = argv[2];
            }
        }
    }

    udp_socket* sock_handle;

    if (!jerry_get_object_native_handle(this, (uintptr_t*)&sock_handle)) {
        ERR_PRINT("native handle not found\n");
        return ZJS_UNDEFINED;
    }

    struct net_addr mcast_addr;
    struct net_addr any_addr;
    struct net_addr my_addr;

    if (sock_handle->type == TYPE_UDP6) {
        const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;

        any_addr.in6_addr = in6addr_any;
        any_addr.family = AF_INET6;

        zjs_ip6_to_u16(ip, &my_addr.in6_addr);
        my_addr.family = AF_INET6;
    } else {
        const struct in_addr in4addr_any = { { { 0 } } };

        any_addr.in_addr = in4addr_any;
        any_addr.family = AF_INET;

        zjs_ip4_to_u32(ip, &my_addr.in_addr);
        my_addr.family = AF_INET;

        ZJS_PRINT("IP Address: 0x%08x\n", my_addr.in_addr.in4_u.u4_addr32[0]);
    }

    net_init();

    sock_handle->udp_recv = net_context_get(IPPROTO_UDP,
                                            &any_addr, 0,
                                            &my_addr, port);
    if (!sock_handle->udp_recv) {
        ZJS_PRINT("cannot get network context\n");
        return ZJS_UNDEFINED;
    }

    ZJS_PRINT("binding %s to port %u\n", ip, port);

    return ZJS_UNDEFINED;
}

static jerry_value_t udp_create_socket(const jerry_value_t function_obj,
                                       const jerry_value_t this,
                                       const jerry_value_t argv[],
                                       const jerry_length_t argc)
{
    udp_socket* sock_handle;
    uint8_t reuseAddr = 0;
    uint8_t type = TYPE_UDP4;
    jerry_value_t type_val;

    if (argc > 1) {
        if (!jerry_value_is_function(argv[1])) {
            ERR_PRINT("second parameter must be listener callback\n");
            return ZJS_UNDEFINED;
        }
    }
    if (jerry_value_is_string(argv[0])) {
        type_val = argv[0];
    } else if (jerry_value_is_object(argv[0])) {
        type_val = zjs_get_property(argv[0], "type");
        if (!jerry_value_is_string(type_val)) {
            ERR_PRINT("options object must have 'type' string property\n");
            return ZJS_UNDEFINED;
        }
        jerry_value_t reuse_val = zjs_get_property(argv[0], "reuseAddr");
        if (jerry_value_is_boolean(reuse_val)) {
            reuseAddr = jerry_get_boolean_value(reuse_val);
        }
        jerry_release_value(reuse_val);
    } else {
        ERR_PRINT("invalid parameters\n");
        return ZJS_UNDEFINED;
    }
    jerry_size_t size = 5;
    char type_str[size];
    zjs_copy_jstring(type_val, type_str, &size);
    if (!size) {
        ERR_PRINT("type must be 'udp4' or 'udp6'\n");
        return ZJS_UNDEFINED;
    }
    if (strncmp(type_str, "udp4", 4) == 0) {
        type = TYPE_UDP4;
    } else if (strncmp(type_str, "udp6", 4) == 0) {
        type = TYPE_UDP6;
    } else {
        ERR_PRINT("type must be 'udp4' or 'udp6'\n");
        return ZJS_UNDEFINED;
    }

    ZJS_PRINT("Opening UDP socket, type=%u, reuseAddr=%u\n", type, reuseAddr);

    jerry_value_t socket = jerry_create_object();

    sock_handle = zjs_malloc(sizeof(udp_socket));
    sock_handle->type = type;

    jerry_set_object_native_handle(socket, (uintptr_t)sock_handle, NULL);

    zjs_obj_add_function(socket, udp_address, "address");
    zjs_obj_add_function(socket, udp_bind_socket, "bind");

    zjs_make_event(socket, ZJS_UNDEFINED);

    if (argc > 1) {
        zjs_add_event_listener(socket, "message", argv[1]);
    }

#if 0
    ZJS_PRINT("0x%08x\n", zjs_ip4_to_u32("192.168.0.1"));
    ZJS_PRINT("\n\n");
    struct in6_addr ip6_addr;

    zjs_ip6_to_u16("ffff:0000:0000:0000:0000:1111:1234:5432", &ip6_addr);
    print_ip6(&ip6_addr);
    zjs_ip6_to_u16("ffff::1111:1234:5432", &ip6_addr);
    print_ip6(&ip6_addr);
    zjs_ip6_to_u16("ffff::0001:1:0011", &ip6_addr);
    print_ip6(&ip6_addr);
#endif

    zjs_register_service_routine(sock_handle, udp_recv);

    return socket;
}

jerry_value_t zjs_udp_init()
{
    jerry_value_t udp_obj = jerry_create_object();
    zjs_obj_add_function(udp_obj, udp_create_socket, "createSocket");
    return udp_obj;
}
