#include <cstdio>
#include <cstdint>

#include <iostream>
#include <string>

namespace taosocks {
    enum class taosocks_version {
        v10 = 0x01,
    };

    enum class taosocks_command {
        connect,
        send,
        recv,
        close,
    };

    enum class command_status {
        succeeded,
        failed,
        not_found,
    };

    enum class addr_type {
        ipv4,
        domain,
        ipv6, 
    };

    enum class request_status {
        succeeded,
        failed,
        network_unreadchable,
        host_unreachable,
        refused_by_host,
        address_type_unsupported,
    };

    class taosocks_server_t {
    public:
        taosocks_server_t()
        {}

        ~taosocks_server_t()
        {}

    private:
        int handle_hello(const uint8_t* data, int len);
        int handle_request(const uint8_t* data, int len);
        int handle_send(const uint8_t* data, int len);
        int handle_recv(const uint8_t* data, int len);
        int handle_close(const uint8_t* data, int len);
    };
}
