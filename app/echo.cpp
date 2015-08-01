//
// Copyright 2015 Raphael Javaux <raphaeljavaux@gmail.com>
// University of Liege.
//
// Echo server. Replies to requests on a port with a copy of the received
// message.
//
// Usage: ./app/echo <link> <ipv4> <TCP port>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <cstdio>
#include <cstdlib>

#include "driver/cpu.hpp"       // bind_to_dataplane()
#include "driver/mpipe.hpp"     // mpipe_t
#include "util/macros.hpp"      // TCP_MPIPE_DEBUG, COLOR_GRN

using namespace std;

using namespace tcp_mpipe::driver;
using namespace tcp_mpipe::net;

#define ECHO_COLOR     COLOR_GRN
#define ECHO_DEBUG(MSG, ...)                                                   \
    TCP_MPIPE_DEBUG("ECHO", ECHO_COLOR, MSG, ##__VA_ARGS__)

// Parsed CLI arguments.
struct args_t {
    char                            *link_name;
    net_t<ipv4_addr_t>              ipv4_addr;
    mpipe_t::tcp_mpipe_t::port_t    tcp_port;
};

static void _print_usage(char **argv);

// Parses CLI arguments.
//
// Fails on a malformed command.
static bool _parse_args(int argc, char **argv, args_t *args);

static void _do_nothing(void)
{
}

int main(int argc, char **argv)
{
    args_t args;
    if (!_parse_args(argc, argv, &args))
        return EXIT_FAILURE;

    cpu::bind_to_dataplane(0);

    mpipe_t mpipe(args.link_name, args.ipv4_addr);

    ECHO_DEBUG(
        "Starts the echo server on interface %s (%s) with %s as IPv4 address "
        "on port %d",
        args.link_name,
        mpipe_t::ethernet_mpipe_t::addr_t::to_alpha(mpipe.data_link.addr),
        mpipe_t::ipv4_mpipe_t::addr_t::to_alpha(args.ipv4_addr), args.tcp_port
    );

    mpipe.data_link.ipv4.tcp.listen(
        args.tcp_port,
        [tcp=&mpipe.data_link.ipv4.tcp]
        (mpipe_t::tcp_mpipe_t::tcb_id_t tcb_id) {
            ECHO_DEBUG(
                "New connection from %s:%" PRIu16 " on port %" PRIu16,
                mpipe_t::ipv4_mpipe_t::addr_t::to_alpha(tcb_id.raddr),
                tcb_id.rport.host(), tcb_id.lport.host()
            );

            return (mpipe_t::tcp_mpipe_t::conn_handlers_t) {
                [tcp, tcb_id](mpipe_t::cursor_t in)
                {
                    size_t size = in.size();

                    in.read_with(
                        [size](const char *buffer)
                        {
                            ECHO_DEBUG(
                                "Received %zu bytes: %.*s", size, (int) size,
                                buffer
                            );
                        }, size
                    );

                    tcp->send(
                        tcb_id, size,
                        [in](size_t offset, mpipe_t::cursor_t out) mutable
                        {
                            in.drop(offset)
                              .take(out.size())
                              .for_each(
                                [&out](const char * buffer, size_t buffer_size)
                                {
                                    out = out.write(buffer, buffer_size);
                                }
                            );
                        },

                        _do_nothing
                    );
                },

                [tcp, tcb_id]()
                {
                    // Closes when the remote closes the connection.
                    tcp->close(tcb_id);
                },

                _do_nothing, _do_nothing
            };
        }
    );

    // Runs the application.
    mpipe.run();

    mpipe.close();

    return EXIT_SUCCESS;
}

static void _print_usage(char **argv)
{
    fprintf(stderr, "Usage: %s <link> <ipv4> <TCP port>\n", argv[0]);
}

static bool _parse_args(int argc, char **argv, args_t *args)
{
    if (argc != 4) {
        _print_usage(argv);
        return false;
    }

    args->link_name = argv[1];

    struct in_addr in_addr;
    if (inet_aton(argv[2], &in_addr) != 1) {
        fprintf(stderr, "Failed to parse the IPv4.\n");
        _print_usage(argv);
        return false;
    }
    args->ipv4_addr = ipv4_addr_t::from_in_addr(in_addr);

    args->tcp_port = atoi(argv[3]);

    return true;
}

#undef ECHO_COLOR
#undef ECHO_DEBUG
