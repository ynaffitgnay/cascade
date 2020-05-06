#ifndef aos_h__
#define aos_h__
// Normal includes
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>
#include <string>
#include <iostream>
#include <map>
#include <queue>

#define SOCKET_NAME "/tmp/aos_daemon.socket"
#define SOCKET_FAMILY AF_UNIX
#define SOCKET_TYPE SOCK_STREAM

#define BACKLOG 128

enum class aos_socket_command {
    CNTRLREG_READ_REQUEST,
    CNTRLREG_READ_RESPONSE,
    CNTRLREG_WRITE_REQUEST,
    CNTRLREG_WRITE_RESPONSE,
    BULKDATA_READ_REQUEST,
    BULKDATA_READ_RESPONSE,
    BULKDATA_WRITE_REQUEST,
    BULKDATA_WRITE_RESPONSE
};

enum class aos_errcode {
    SUCCESS = 0,
    RETRY, // for reads
    ALIGNMENT_FAILURE,
    PROTECTION_FAILURE,
    APP_DOES_NOT_EXIST,
    TIMEOUT,
    UNKNOWN_FAILURE
};

struct aos_app_handle  {
    uint64_t slot_id;
    uint64_t app_id;
    uint64_t key;
};

struct aos_socket_command_packet {
    aos_socket_command command_type;
    uint64_t slot_id;
    uint64_t app_id;
    uint64_t addr64;
    uint64_t data64; 
};

struct aos_socket_response_packet {
    aos_errcode errorcode;
    uint64_t    data64;
};

#endif // end aos_h__
