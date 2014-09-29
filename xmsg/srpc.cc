
#include <cerrno>
#include <cstring>
#include <iostream>
#include <system_error>
#include <unistd.h>
#include <xdrc/srpc.h>
#include <xdrc/types.h>

namespace xdr {

inline rpc_msg &
rpc_mkerr(rpc_msg &m, accept_stat stat)
{
  m.body.mtype(REPLY).rbody().stat(MSG_ACCEPTED).areply()
    .reply_data.stat(stat);
  return m;
}

inline rpc_msg &
rpc_mkerr(rpc_msg &m, reject_stat stat)
{
  m.body.mtype(REPLY).rbody().stat(MSG_DENIED).rreply().stat(stat);
  switch(stat) {
  case RPC_MISMATCH:
    m.body.rbody().rreply().mismatch_info().low = 2;
    m.body.rbody().rreply().mismatch_info().high = 2;
    break;
  case AUTH_ERROR:
    m.body.rbody().rreply().rj_why() = AUTH_FAILED;
    break;
  }
  return m;
}

inline rpc_msg &
rpc_mkerr(rpc_msg &m, auth_stat stat)
{
  m.body.mtype(REPLY).rbody().stat(MSG_DENIED).rreply()
    .stat(AUTH_ERROR).rj_why() = stat;
  return m;
}

msg_ptr
read_message(int fd)
{
  std::uint32_t len;
  ssize_t n = read(fd, &len, 4);
  if (n == -1)
    throw std::system_error(errno, std::system_category(), "xdr::read_message");
  if (n < 4)
    throw xdr_bad_message_size("read_message: premature EOF");
  if (n & 3)
    throw xdr_bad_message_size("read_message: received size not multiple of 4");
  if (n >= 0x80000000)
    throw xdr_bad_message_size("read_message: received size too big");

  len = swap32le(len);
  msg_ptr m = message_t::alloc(len);
  n = read(fd, m->data(), len);
  if (n == -1)
    throw std::system_error(errno, std::system_category(),
			    "xdr::read_message");
  if (n != len)
    throw xdr_bad_message_size("read_message: premature EOF");

  return m;
}

void
write_message(int fd, const msg_ptr &m)
{
  ssize_t n = write(fd, m->raw_data(), m->raw_size());
  if (n < 0)
    throw std::system_error(errno, std::system_category(),
			    "xdr::write_message");
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

void
rpc_server::register_server_base(server_base *s)
{
  servers_[s->prog_][s->vers_].reset(s);
}

msg_ptr
rpc_server::dispatch(msg_ptr m)
{
  xdr_get g(m);
  rpc_msg hdr;
  archive(g, hdr);
  if (hdr.body.mtype() != CALL)
    throw xdr_runtime_error("rpc_server received non-CALL message");

  if (hdr.body.cbody().rpcvers != 2)
    return xdr_to_msg(rpc_mkerr(hdr, RPC_MISMATCH));

  auto prog = servers_.find(hdr.body.cbody().prog);
  if (prog == servers_.end())
    return xdr_to_msg(rpc_mkerr(hdr, PROG_UNAVAIL));

  auto vers = prog->second.find(hdr.body.cbody().vers);
  if (vers == prog->second.end()) {
    rpc_mkerr(hdr, PROG_MISMATCH);
    hdr.body.rbody().areply().reply_data.mismatch_info().low =
      prog->second.cbegin()->first;
    hdr.body.rbody().areply().reply_data.mismatch_info().high =
      prog->second.crbegin()->first;
    return xdr_to_msg(hdr);
  }

  try { return vers->second->process(hdr, g); }
  catch (const xdr_runtime_error &e) {
    std::cerr << xdr_to_string(hdr, e.what());
    return xdr_to_msg(rpc_mkerr(hdr, GARBAGE_ARGS));
  }
}

void
srpc_server::run()
{
  for (;;)
    write_message(fd_, dispatch(read_message(fd_)));
}

}
