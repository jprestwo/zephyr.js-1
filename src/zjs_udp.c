// Copyright (c) 2016, Intel Corporation.

#include <zephyr.h>
#include <sections.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <net/nbuf.h>
#include <net/net_if.h>
#include <net/net_core.h>
#include <net/net_context.h>
#include <net/net_mgmt.h>

#include "zjs_common.h"
#include "zjs_util.h"
#include "zjs_udp.h"
#include "zjs_event.h"
#include "zjs_buffer.h"

/* Organization-local 239.192.0.0/14 */
#define MCAST_IPADDR4 { { { 239, 192, 0, 2 } } }
/* admin-local, dynamically allocated multicast address */
#define MCAST_IPADDR6 { { { 0xff, 0x84, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x2 } } }

#define MY_IP4ADDR { { { 192, 0, 2, 1 } } }

static struct in_addr in4addr_my = MY_IP4ADDR;

typedef enum {
    TYPE_UDP4,
    TYPE_UDP6
} UdpType;

typedef struct {
    UdpType type;
    uint32_t port;
    struct net_context *udp_recv;
    struct net_context *mcast_recv;
    jerry_value_t sock_obj;
} udp_socket;

static void addr_to_string(sa_family_t family, void* src, char* dest)
{
    if (family == AF_INET) {
        uint8_t octets[4];
        struct in_addr* address = (struct in_addr*)src;
        int i;
        for (i = 0; i < 4; i++) {
            octets[i] = (uint8_t)((address->in4_u.u4_addr32[0] >> (i * 8)) & 0xff);
        }
        sprintf(dest, "%u.%u.%u.%u", octets[0], octets[1], octets[2], octets[3]);
    } else {

    }
}

static jerry_value_t udp_send(const jerry_value_t function_obj,
                              const jerry_value_t this,
                              const jerry_value_t argv[],
                              const jerry_length_t argc)
{
    void* data;
    uint32_t size;

    if (jerry_value_is_object(argv[0])) {
        // buffer or array
        if (jerry_value_is_array(argv[0])) {
            //TODO: add support for array
            return ZJS_UNDEFINED;
        } else {
            zjs_buffer_t* buffer = zjs_buffer_find(argv[0]);
            data = buffer->buffer;
            size = buffer->bufsize;
        }
    } else if (jerry_value_is_string(argv[0])) {
        data = (void*)zjs_alloc_from_jstring(argv[0], &size);
    } else {
        ERR_PRINT("invalid arguments\n");
        return ZJS_UNDEFINED;
    }
    return ZJS_UNDEFINED;
}

static void udp_recv(struct net_context *context,
                     struct net_buf *buf,
                     int status,
                     void *user_data)
{
    udp_socket* sock_handle = (udp_socket*)user_data;
    uint32_t msg_size = net_nbuf_appdatalen(buf);
    char family[5];
    char* src_address = NULL;

    if (sock_handle->type == TYPE_UDP4) {
        src_address = zjs_malloc(16);
        memset(src_address, 0, 16);
        memcpy(family, "IPv4", 4);
        addr_to_string(AF_INET, &NET_IPV4_BUF(buf)->src, src_address);
    } else {
        memcpy(family, "IPv6", 4);
    }
    family[4] = '\0';

    jerry_value_t rinfo_val = jerry_create_object();

    zjs_obj_add_string(rinfo_val, family, "family");
    zjs_obj_add_number(rinfo_val, (double)msg_size, "size");
    zjs_obj_add_number(rinfo_val, (double)sock_handle->port, "port");
    if (src_address) {
        zjs_obj_add_string(rinfo_val, src_address, "address");
        zjs_free(src_address);
    }
    jerry_value_t buffer_val = zjs_buffer_create(msg_size);
    zjs_buffer_t* buffer = zjs_buffer_find(buffer_val);

    memcpy(buffer->buffer, net_nbuf_appdata(buf), msg_size);

    net_nbuf_unref(buf);

    jerry_value_t argv[2];
    argv[0] = buffer_val;
    argv[1] = rinfo_val;

    zjs_trigger_event(sock_handle->sock_obj, "message", argv, 2, NULL, NULL);
}

static inline bool get_context(struct net_context **udp_recv4)
{
    int ret;
#if defined(CONFIG_NET_IPV4) && defined(CONFIG_NET_UDP)
    ret = net_context_get(AF_INET, SOCK_DGRAM, IPPROTO_UDP, udp_recv4);
    if (ret < 0) {
        ERR_PRINT("Cannot get network context for IPv4 UDP (%d)",
            ret);
        return false;
    }
#endif

    return true;
}

static void setup_udp_recv(struct net_context *udp_recv4, void* handle)
{
    int ret;
#if defined(CONFIG_NET_IPV4)
    ret = net_context_recv(udp_recv4, udp_recv, 0, handle);
    if (ret < 0) {
        ERR_PRINT("Cannot receive IPv4 UDP packets");
    }
#endif /* CONFIG_NET_IPV4 */
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
    int ret;
    uint32_t size = 40;
    char ip[size];
    uint32_t port = 0;
    uint8_t has_cb = 0;
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
                has_cb = 1;
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
                has_cb = 1;
            }
        }
    }

    udp_socket* sock_handle;

    if (!jerry_get_object_native_handle(this, (uintptr_t*)&sock_handle)) {
        ERR_PRINT("native handle not found\n");
        return ZJS_UNDEFINED;
    }

    sock_handle->port = port;
#if defined(CONFIG_NET_IPV4)
    struct sockaddr_in my_addr4 = { 0 };
#endif

#if defined(CONFIG_NET_IPV4)
    my_addr4.sin_family = AF_INET;
    my_addr4.sin_port = htons(4242);
#endif

    ret = net_context_bind(sock_handle->udp_recv, (struct sockaddr *)&my_addr4,
            sizeof(struct sockaddr_in));
    if (ret < 0) {
        ERR_PRINT("Cannot bind IPv4 UDP port %d (%d)", ntohs(my_addr4.sin_port), ret);
        return false;
    }

    setup_udp_recv(sock_handle->udp_recv, sock_handle);

    if (has_cb) {
        zjs_add_event_listener(this, "listening", cb_val);
    }

    zjs_trigger_event(this, "listening", NULL, 0, NULL, NULL);

    return ZJS_UNDEFINED;
}

static inline void init_app(void)
{

#if defined(CONFIG_NET_IPV4)
    //net_dhcpv4_start(net_if_get_default());

    if (net_addr_pton(AF_INET, "192.0.2.1", (struct sockaddr *)&in4addr_my) < 0) {
        ERR_PRINT("Invalid IPv4 address %s", "192.0.2.1");
        return;
    }

    net_if_ipv4_addr_add(net_if_get_default(), &in4addr_my, NET_ADDR_MANUAL, 0);
#endif /* CONFIG_NET_IPV4 */
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
    sock_handle->sock_obj = socket;

    jerry_set_object_native_handle(socket, (uintptr_t)sock_handle, NULL);

    zjs_obj_add_function(socket, udp_address, "address");
    zjs_obj_add_function(socket, udp_bind_socket, "bind");

    zjs_make_event(socket, ZJS_UNDEFINED);

    if (argc > 1) {
        zjs_add_event_listener(socket, "message", argv[1]);
    }
    init_app();
    get_context(&sock_handle->udp_recv);

    return socket;
}

jerry_value_t zjs_udp_init()
{
    jerry_value_t udp_obj = jerry_create_object();
    zjs_obj_add_function(udp_obj, udp_create_socket, "createSocket");
    return udp_obj;
}
