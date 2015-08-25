#include <cstdio>
#include <cstdint>

#include <iostream>
#include <string>

namespace taosocks {
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
