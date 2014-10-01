
#include <xdrpp/marshal.h>

namespace xdr {

msg_ptr
message_t::alloc(std::size_t size)
{
  // In RPC, the high bit means a continuation packet follows, which
  // we don't implement but reserve for future compatibility.
  assert(size < 0x80000000);
  void *raw = operator new(offsetof(message_t, buf_[size + 4]));
  if (!raw)
    throw std::bad_alloc();
  message_t *m = new (raw) message_t (size);
  *reinterpret_cast<std::uint32_t *>(m->raw_data()) =
    swap32le(size | 0x80000000);
  return msg_ptr(m);
}

void
marshal_base::get_bytes(const std::uint32_t *&pr, void *buf, std::size_t len)
{
  const char *p = reinterpret_cast<const char *>(pr);
  std::memcpy(buf, p, len);
  p += len;
  while (len & 3) {
    ++len;
    if (*p++ != '\0')
      throw xdr_should_be_zero("Non-zero padding bytes encountered");
  }
  pr = reinterpret_cast<const std::uint32_t *>(p);
}

void
marshal_base::put_bytes(std::uint32_t *&pr, const void *buf, std::size_t len)
{
  char *p = reinterpret_cast<char *>(pr);
  std::memcpy(p, buf, len);
  p += len;
  while (len & 3) {
    ++len;
    *p++ = '\0';
  }
  pr = reinterpret_cast<std::uint32_t *>(p);
}

}
