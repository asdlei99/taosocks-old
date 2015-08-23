#include <stdint.h>
#include <cassert>

#include <iostream>
#include <string>

#include <process.h>

#include "socket.hpp"

namespace taosocks {
    enum class auth_methods {
        none            = 0x00,
        gssapi          = 0x01,
        login           = 0x02,
        iana_start      = 0x03,
        iana_end        = 0x7F,
        private_start   = 0x80,
        private_end     = 0xFE,
        unacceptable    = 0xFF,
    };

    enum class socks_version {
        v4  = 0x04,
        v4a = v4,
        v5  = 0x05,
    };

    enum class socks_command {
        tcp_stream  = 0x01,
        tcp_binding = 0x02,
        udp_port    = 0x03,    
    };

    enum class addr_type {
        ipv4    = 0x01,
        domain  = 0x03,
        ipv6    = 0x04,
    };

    enum class request_status {
        granted                     = 0x00,
        failure                     = 0x01,
        not_allowed                 = 0x02,
        network_unreadchable        = 0x03,
        host_unreachable            = 0x04,
        refused_by_host             = 0x05,
        ttl_expired                 = 0x06,
        command_not_supported       = 0x07,
        address_type_unsupported    = 0x08,
    };

    class socks_server {
    public:
        socks_server(client_t& client)
            : _client(client)
            , _socket_client(nullptr)
        {}

        ~socks_server()
        {
            delete _socket_client;
        }

    public:
        void run() {
            if (!auth()) return;
            if (!request()) return;
            if (!respond()) return;
        }

    protected:
        bool auth() {
            uint8_t c;

            // incoming
            c = read_byte();
            assert(c == (uint8_t)socks_version::v5);

            c = read_byte();
            assert(c == 1);

            c = read_byte();
            assert(c == (uint8_t)auth_methods::none);

            // outgoing
            write_byte((uint8_t)socks_version::v5);

            write_byte((uint8_t)auth_methods::none);

            return true;
        }

        bool request() {
            uint8_t c;

            c = read_byte();
            assert(c == (uint8_t)socks_version::v5);

            c = read_byte();
            assert(c == (uint8_t)socks_command::tcp_stream);

            c = read_byte();
            assert(c == 0);

            c = read_byte();
            assert(c == (uint8_t)addr_type::domain);

            c = read_byte();
            char* pdomain = new char[c];
            if (::recv(_client.fd, pdomain, c, 0) != c)
                throw "request error.";

            std::string domain(pdomain, c);
            delete[] pdomain;

            uint16_t net_port;
            if (::recv(_client.fd, (char*)&net_port, 2, 0) != 2)
                throw "request error.";

            _domain = domain;
            _port = ntohs(net_port);

            std::string ip = resolve(domain.c_str());
            in_addr addr;
            addr.S_un.S_addr = ::inet_addr(ip.c_str());

            _socket_client = new socket_client(ip.c_str(), _port);
            _socket_client->connect();

            write_byte((uint8_t)socks_version::v5);
            write_byte((uint8_t)request_status::granted);
            write_byte(0);
            write_byte((uint8_t)addr_type::ipv4);
            ::send(_client.fd, (char*)&addr.S_un.S_addr, 4, 0);
            ::send(_client.fd, (char*)&net_port, 2, 0);

            return true;
        }

        bool respond() {
            SOCKET cfd = _client.fd;
            SOCKET sfd = _socket_client->_fd;

            int n;
            for (;;) {
                fd_set rfds;
                FD_ZERO(&rfds);

                FD_SET(sfd, &rfds);
                FD_SET(cfd, &rfds);

                n = ::select(-1, &rfds, nullptr, nullptr, nullptr);
                if (n== -1) break;
                else if (n == 0) continue;

                uint8_t buf[10240];
                u_long count;

                if (FD_ISSET(cfd, &rfds)) {
                    ::ioctlsocket(cfd, FIONREAD, &count);
                    count = min(sizeof(buf), count);

                    assert(::recv(cfd, (char*)buf, count, 0) == count);
                    assert(::send(sfd, (char*)buf, count, 0) == count);
                }

                if (FD_ISSET(sfd, &rfds)) {
                    ::ioctlsocket(sfd, FIONREAD, &count);
                    count = min(sizeof(buf), count);

                    assert(::recv(sfd, (char*)buf, count, 0) == count);
                    assert(::send(cfd, (char*)buf, count, 0) == count);
                }
            }

            return true;
        }

        uint8_t read_byte() {
            uint8_t c;
            if (::recv(_client.fd, (char*)&c, 1, 0) == 1) {
                return c;
            }
            else{
                throw "read_byte error.";
            }
        }

        bool write_byte(uint8_t c) {
            if (::send(_client.fd, (char*)&c, 1, 0) == 1)
                return true;
            else
                throw "write_byte error.";
        }
    protected:
        socket_client*  _socket_client;
        client_t&       _client;
        std::string     _domain;
        uint16_t        _port;
    };
}

unsigned int __stdcall worker_thread(void* ud) {
    auto& queue = *reinterpret_cast<taosocks::client_queue*>(ud);
    taosocks::client_t* c = nullptr;

    while (c = &queue.pop()) {
        std::cout << "serving...\n";
        taosocks::socks_server server(*c);
        server.run();
    }

    return 0;
}

void create_worker_threads(taosocks::client_queue& queue) {
    SYSTEM_INFO si;
    ::GetSystemInfo(&si);

    DWORD n_threads = si.dwNumberOfProcessors * 2;
    for (int i = 0; i < (int)n_threads; i++){
        _beginthreadex(nullptr, 0, worker_thread, &queue, 0, nullptr);
    }
}

int main() {

    taosocks::win_sock wsa;

    taosocks::client_queue queue;
    taosocks::socket_server server("127.0.0.1", 1080, 1);
    server.start();

    create_worker_threads(queue);

    taosocks::client_t client;
    while (server.accept(&client)) {
        std::cout << "accepted...\n";
        queue.push(client);
    }

    return 0;
}
