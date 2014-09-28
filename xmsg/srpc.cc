
#include <cerrno>
#include <cstring>
#include <system_error>
#include <unistd.h>
#include <xdrc/srpc.h>
#include <xdrc/types.h>

namespace xdr {

msg_ptr
read_message(int fd)
{
  std::uint32_t len;
  std::size_t n = read(fd, &len, 4);
  if (n == -1)
    throw std::system_error(errno, std::system_category(), "xdr::read_message");
  if (n < 4)
    throw xdr_bad_message_size("read_message: premature EOF");
  if (n & 3)
    throw xdr_bad_message_size("read_message: received size not multiple of 4");
  if (n >= 0x80000000)
    throw xdr_bad_message_size("read_message: received size too big");

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
  std::size_t n = write(fd, m->raw_data(), m->raw_size());
  if (n == -1)
    throw std::system_error(errno, std::system_category(),
			    "xdr::write_message");
  // If this assertion fails, the file descriptor may have had
  // O_NONBLOCK set, which is not allowed for the synchronous
  // interface.
  assert(n == m->raw_size());
}

}
