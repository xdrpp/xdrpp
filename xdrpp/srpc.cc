
#include <cerrno>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <xdrpp/exception.h>
#include <xdrpp/srpc.h>

namespace xdr {

bool xdr_trace_client = std::getenv("XDR_TRACE_CLIENT");

msg_ptr
read_message(int fd)
{
  std::uint32_t len;
  ssize_t n = read(fd, &len, 4);
  if (n == -1)
    throw xdr_system_error("xdr::read_message");
  if (n < 4)
    throw xdr_bad_message_size("read_message: premature EOF");
  if (n & 3)
    throw xdr_bad_message_size("read_message: received size not multiple of 4");
  if (n >= 0x80000000)
    throw xdr_bad_message_size("read_message: received size too big");

  len = swap32le(len);
  if (len & 0x80000000)
    len &= 0x7fffffff;
  else
    throw xdr_bad_message_size("read_message: message fragments unimplemented");

  msg_ptr m = message_t::alloc(len);
  n = read(fd, m->data(), len);
  if (n == -1)
    throw xdr_system_error("xdr::read_message");
  if (n != len)
    throw xdr_bad_message_size("read_message: premature EOF");

  return m;
}

void
write_message(int fd, const msg_ptr &m)
{
  ssize_t n = write(fd, m->raw_data(), m->raw_size());
  if (n == -1)
    throw xdr_system_error("xdr::write_message");
  // If this assertion fails, the file descriptor may have had
  // O_NONBLOCK set, which is not allowed for the synchronous
  // interface.
  assert(std::size_t(n) == m->raw_size());
}

uint32_t xid_counter;

void
prepare_call(uint32_t prog, uint32_t vers, uint32_t proc, rpc_msg &hdr)
{
  hdr.xid = ++xid_counter;
  hdr.body.mtype(CALL);
  hdr.body.cbody().rpcvers = 2;
  hdr.body.cbody().prog = prog;
  hdr.body.cbody().vers = vers;
  hdr.body.cbody().proc = proc;
}

rpc_tcp_listener::rpc_tcp_listener(unique_fd &&fd, bool reg)
  : listen_fd_(fd ? std::move(fd) : tcp_listen()),
    use_rpcbind_(reg)
{
  set_close_on_exec(listen_fd_.get());
  ps_.fd_cb(listen_fd_.get(), pollset::Read,
	    std::bind(&rpc_tcp_listener::accept_cb, this));
}

rpc_tcp_listener::~rpc_tcp_listener()
{
  ps_.fd_cb(listen_fd_.get(), pollset::Read);
}

void
rpc_tcp_listener::accept_cb()
{
  int fd = accept(listen_fd_.get(), nullptr, 0);
  if (fd == -1) {
    std::cerr << "rpc_tcp_listener: accept: " << std::strerror(errno)
	      << std::endl;
    return;
  }
  set_close_on_exec(fd);
  msg_sock *ms = new msg_sock(ps_, fd);
  ms->setrcb(std::bind(&rpc_tcp_listener::receive_cb, this, ms,
		       std::placeholders::_1));
}

void
rpc_tcp_listener::receive_cb(msg_sock *ms, msg_ptr mp)
{
  if (!mp) {
    delete ms;
    return;
  }
  try {
    dispatch(nullptr, std::move(mp), msg_sock_put_t(ms));
  }
  catch (const xdr_runtime_error &e) {
    std::cerr << e.what() << std::endl;
    delete ms;
  }
}

void
rpc_tcp_listener::run()
{
  while (ps_.pending())
    ps_.poll();
}

void
srpc_server::run()
{
  for (;;)
    dispatch(nullptr, read_message(fd_),
	     std::bind(write_message, fd_, std::placeholders::_1));
}

}
