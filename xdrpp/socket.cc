
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <xdrpp/rpcbind.hh>
#include <xdrpp/socket.h>
#include <xdrpp/srpc.h>

namespace xdr {

using std::string;

namespace {

std::vector<rpcb> registered_services;

void
run_cleanup()
{
  try {
    auto fd = tcp_connect(nullptr, "sunrpc");
    srpc_client<xdr::RPCBVERS4> c{fd.get()};
    for (const auto &arg : registered_services)
      c.RPCBPROC_UNSET(arg);
  }
  catch (...) {}
}

void
set_cleanup()
{
  static struct once {
    once() { atexit(run_cleanup); }
  } o;
}

}

string
addrinfo_to_string(const addrinfo *ai)
{
  std::ostringstream os;
  bool first{true};
  while (ai) {
    if (first)
      first = false;
    else
      os << ", ";
    string h, p;
    get_numinfo(ai->ai_addr, ai->ai_addrlen, &h, &p);
    if (h.find(':') == string::npos)
      os << h << ':' << p;
    else
      os << '[' << h << "]:" << p;
    ai = ai -> ai_next;
  }
  return os.str();
}

void
set_nonblock(int fd)
{
  int n;
  if ((n = fcntl (fd, F_GETFL)) == -1
      || fcntl (fd, F_SETFL, n | O_NONBLOCK) == -1)
    throw std::system_error(errno, std::system_category(), "O_NONBLOCK");
}

void
set_close_on_exec(int fd)
{
  int n;
  if ((n = fcntl (fd, F_GETFD)) == -1
      || fcntl (fd, F_SETFD, n | FD_CLOEXEC) == -1)
    throw std::system_error(errno, std::system_category(), "F_SETFD");
}

void
really_close(int fd)
{
  while (close(fd) == -1)
    if (errno != EINTR) {
      std::cerr << "really_close: " << std::strerror(errno) << std::endl;
      return;
    }
}

struct gai_category_impl : public std::error_category {
  const char *name() const noexcept override { return "DNS"; }
  string message(int err) const override { return gai_strerror(err); }
};

const std::error_category &
gai_category()
{
  static gai_category_impl cat;
  return cat;
}

namespace {

string
cat_host_service(const char *host, const char *service)
{
  string target;
  if (host) {
    if (std::strchr(host, ':')) {
      target += "[";
      target += host;
      target += "]";
    }
    else
      target = host;
  }
  if (service) {
    target += ":";
    target += service;
  }
  return target;
}

}

unique_addrinfo get_addrinfo(const char *host, int socktype,
			     const char *service, int family)
{
  addrinfo hints, *res;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = socktype;
  hints.ai_family = family;
  hints.ai_flags = AI_ADDRCONFIG;
  int err = getaddrinfo(host, service, &hints, &res);
  if (err)
    throw std::system_error(err, gai_category(),
			    cat_host_service(host, service));
  return unique_addrinfo(res);
}

int
parse_uaddr_port(const string &uaddr)
{
  std::size_t low = uaddr.rfind('.');
  if (low == string::npos || low == 0)
    return -1;
  std::size_t high = uaddr.rfind('.', low - 1);
  if (high == string::npos)
    return -1;

  try {
    int hb = std::stoi(uaddr.substr(high+1));
    int lb = std::stoi(uaddr.substr(low+1));
    if (hb < 0 || hb > 255 || lb < 0 || lb > 255)
      return -1;
    return hb << 8 | lb;
  }
  catch (std::invalid_argument &) {
    return -1;
  }
  catch (std::out_of_range &) {
    return -1;
  }
}

string
make_uaddr(const sockaddr *sa, socklen_t salen)
{
  string host, portstr;
  get_numinfo(sa, salen, &host, &portstr);
  unsigned port = std::stoul(portstr);
  if (port == 0 || port >= 65536)
    throw std::system_error(std::make_error_code(std::errc::invalid_argument),
			    "bad port number");
  host += "." + std::to_string(port >> 8) + "." + std::to_string(port & 0xff);
  return host;
}

string
make_uaddr(int fd)
{
  union {
    struct sockaddr sa;
    struct sockaddr_storage ss;
  };
  socklen_t salen{sizeof ss};
  std::memset(&ss, 0, salen);
  if (getsockname(fd, &sa, &salen) == -1)
    throw std::system_error(errno, std::system_category(), "getsockname");
  return make_uaddr(&sa, salen);
}

void
rpcbind_register(const sockaddr *sa, socklen_t salen,
		 std::uint32_t prog, std::uint32_t vers)
{
  set_cleanup();

  auto fd = tcp_connect(nullptr, "sunrpc", sa->sa_family);
  srpc_client<xdr::RPCBVERS4> c{fd.get()};

  rpcb arg;
  arg.r_prog = prog;
  arg.r_vers = vers;
  arg.r_netid = sa->sa_family == AF_INET6 ? "tcp6" : "tcp";
  arg.r_addr = make_uaddr(sa, salen);
  arg.r_owner = std::to_string(geteuid());
  c.RPCBPROC_UNSET(arg);
  auto res = c.RPCBPROC_SET(arg);
  if (!*res)
    throw std::system_error(std::make_error_code(std::errc::address_in_use),
			    "RPCBPROC_SET");
  registered_services.push_back(arg);
}

void
rpcbind_register(int fd, std::uint32_t prog, std::uint32_t vers)
{
  union {
    struct sockaddr sa;
    struct sockaddr_storage ss;
  };
  socklen_t salen{sizeof ss};
  std::memset(&ss, 0, salen);
  if (getsockname(fd, &sa, &salen) == -1)
    throw std::system_error(errno, std::system_category(), "getsockname");
  rpcbind_register(&sa, salen, prog, vers);
}

void get_numinfo(const sockaddr *sa, socklen_t salen,
		 string *host, string *serv)
{
  char hostbuf[NI_MAXHOST];
  char servbuf[NI_MAXSERV];
  int err = getnameinfo(sa, salen, hostbuf, sizeof(hostbuf),
			servbuf, sizeof(servbuf),
			NI_NUMERICHOST|NI_NUMERICSERV);
  if (err)
    throw std::system_error(err, gai_category(), "getnameinfo");
  if (host)
    *host = hostbuf;
  if (serv)
    *serv = servbuf;
}

unique_fd
tcp_connect1(const addrinfo *ai, bool ndelay)
{
  // XXX
  //std::cerr << "connecting to " << addrinfo_to_string(ai) << std::endl;

  unique_fd fd {socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)};
  if (!fd)
    throw std::system_error(errno, std::system_category(), "socket");
  if (ndelay)
    set_nonblock(fd.get());
  if (connect(fd.get(), ai->ai_addr, ai->ai_addrlen) == -1
      && errno != EINPROGRESS)
    fd.clear();
  return fd;
}

unique_fd
tcp_connect(const addrinfo *ai)
{
  unique_fd fd;
  errno = EADDRNOTAVAIL;
  for (; ai && !fd; ai = ai->ai_next)
    if ((fd = tcp_connect1(ai)))
      return fd;
  throw std::system_error(errno, std::system_category(), "connect");
}

unique_fd
tcp_connect(const char *host, const char *service, int family)
{
  return tcp_connect(get_addrinfo(host, SOCK_STREAM, service, family));
}

unique_fd
tcp_connect_rpc(const char *host, std::uint32_t prog, std::uint32_t vers,
		int family)
{
  unique_addrinfo ail = get_addrinfo(host, SOCK_STREAM, "sunrpc", family);

  for (const addrinfo *ai = ail.get(); ai; ai = ai->ai_next) {
    try {
      auto fd = tcp_connect1(ai);
      srpc_client<xdr::RPCBVERS4> c{fd.get()};

      rpcb arg;
      arg.r_prog = prog;
      arg.r_vers = vers;
      arg.r_netid = ai->ai_family == AF_INET6 ? "tcp6" : "tcp";
      arg.r_addr = make_uaddr(fd.get());
      auto res = c.RPCBPROC_GETADDR(arg);

      int port = parse_uaddr_port(*res);
      if (port == -1)
	continue;

      switch (ai->ai_family) {
      case AF_INET:
	((sockaddr_in *) ai->ai_addr)->sin_port = htons(port);
	break;
      case AF_INET6:
	((sockaddr_in6 *) ai->ai_addr)->sin6_port = htons(port);
	break;
      }
      fd.clear();
      if ((fd = tcp_connect1(ai)))
	return fd;
    }
    catch(const std::system_error &) {}
  }
  throw std::system_error(std::make_error_code(std::errc::connection_refused),
			  "Could not obtain port from rpcbind");
}

unique_fd
tcp_listen(const char *service, int family)
{
  addrinfo hints, *res;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = family;
  hints.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;
  int err = getaddrinfo(nullptr, service ? service : "0", &hints, &res);
  if (err)
    throw std::system_error(err, gai_category(), "AI_PASSIVE");
  unique_addrinfo ai{res};

  // XXX
  //std::cerr << "listening at " << addrinfo_to_string(ai.get()) << std::endl;

  unique_fd fd {socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)};
  if (!fd)
    throw std::system_error(errno, std::system_category(), "socket");
  if (bind(fd.get(), ai->ai_addr, ai->ai_addrlen) == -1)
    throw std::system_error(errno, std::system_category(), "bind");
  if (listen (fd.get(), 5) == -1)
    throw std::system_error(errno, std::system_category(), "listen");
  return fd;
}

}
