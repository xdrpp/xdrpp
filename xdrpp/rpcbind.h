// -*- C++ -*-

#include <xdrpp/socket.h>

namespace xdr {

//! Create a TCP connection to an RPC server on \c host, first
//! querying \c rpcbind on \c host to determine the port.
unique_sock tcp_connect_rpc(const char *host,
			    std::uint32_t prog, std::uint32_t vers,
			    int family = AF_UNSPEC);

//! Register a service listening on \c sa with \c rpcbind.
void rpcbind_register(const sockaddr *sa, socklen_t salen,
		      std::uint32_t prog, std::uint32_t vers);
void rpcbind_register(sock_t s, std::uint32_t prog, std::uint32_t vers);

//! Extract the port number from an RFC1833 / RFC5665 universal
//! network address (uaddr).
int parse_uaddr_port(const std::string &uaddr);

//! Create a uaddr for a local address or file descriptor.
std::string make_uaddr(const sockaddr *sa, socklen_t salen);
std::string make_uaddr(sock_t s);

}
