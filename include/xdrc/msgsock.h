// -*- C++ -*-

#ifndef _MSGSOCK_H_
#define _MSGSOCK_H_ 1

//! \file msgsock.h Send and receive delimited messages over
//! non-blocking sockets.

#include <deque>
#include <xdrc/pollset.h>
#include <xdrc/endian.h>

namespace xdr {

constexpr uint32_t
swap32(uint32_t v)
{
  return v << 24 | (v & 0xff00) << 8 | (v >> 8 & 0xff00) | v >> 24;
}

//! Byteswap 32-bit value on big-endian machines, do nothing on little
//! endian.
constexpr uint32_t
swap32ifbe(uint32_t v)
{
  return xdr::is_big_endian ? swap32(v) : v;
}

class MsgBufImpl {
  //! Bytes in buf_ in little endian order
  const uint32_t lelen_;
  char buf_[1];
  MsgBufImpl(size_t len) : lelen_(swap32ifbe(len)) {}
public:
  friend class MsgBuf;

  char *data() { return buf_; }
  const char *data() const { return buf_; }
  size_t size() const { return swap32ifbe(lelen_); }

  //! Buffer prefixed by 4-byte length in little-endian
  const char *rawData() const { return reinterpret_cast<const char *>(this); }
  //! Size of 4-byte length plus data
  size_t rawSize() const { return sizeof(lelen_) + size(); }
};

class MsgBuf : public std::unique_ptr<MsgBufImpl> {
public:
  using std::unique_ptr<MsgBufImpl>::unique_ptr;
  //! Allocate a buffer of a particular size.  \throws std::bad_alloc
  //! if <tt>::operator new</tt> returns \c nullptr.
  MsgBuf(size_t len);
  // XXX not sure why the default constructor is not delegated
  MsgBuf() = default;
};

/** \brief Send and receive a series of delimited messages on a stream
 * socket.
 *
 * The format is simple:  A 4-byte length (in little-endian format)
 * followed by that many bytes.  The implementation is optimized for
 * having many sockets each receiving a small number of messages, as
 * opposed to receiving many messages over the same socket.
 *
 * Currently this calls read once or twice per message to get the
 * exact length before allocating buffer space and reading the message
 * body (possibly including the next message length).  This should
 * really be fixed to read at least a little bit more data
 * speculatively and reduce the number of system calls.
 */
class SeqSock {
public:
  using rcb_t = std::function<void(MsgBuf)>;

  template<typename T> SeqSock(PollSet *ps, int fd, T &&rcb,
			       size_t maxmsglen = 0x100000)
    : ps_(*ps), fd_(fd), maxmsglen_(maxmsglen), rcb_(std::forward<T>(rcb)) {
    init();
  }
  ~SeqSock();
  SeqSock &operator=(SeqSock &&) = delete;

  size_t wsize() const { return wsize_; }
  void putmsg(MsgBuf &b);

private:
  PollSet &ps_;
  const int fd_;
  const size_t maxmsglen_;

  rcb_t rcb_;
  uint32_t nextlen_;
  MsgBuf rdmsg_;
  size_t rdpos_ {0};

  std::deque<MsgBuf> wqueue_;
  size_t wsize_ {0};
  size_t wstart_ {0};
  bool wfail_ {false};

  static constexpr bool eagain(int err) {
    return err == EAGAIN || err == EWOULDBLOCK || err == EINTR;
  }
  char *nextlenp() { return reinterpret_cast<char *>(&nextlen_); }
  uint32_t nextlen() const { return swap32ifbe(nextlen_); }

  void init();
  void input();
  void pop_wbytes(size_t n);
  void output(bool cbset);
};

}

#endif /* !_MSGSOCK_H_ */
