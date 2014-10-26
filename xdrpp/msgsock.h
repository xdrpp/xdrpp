// -*- C++ -*-

//! \file msgsock.h Send and receive delimited messages over
//! non-blocking sockets.

#ifndef _XDRPP_MSGSOCK_H_INCLUDED_
#define _XDRPP_MSGSOCK_H_INCLUDED_ 1

#include <deque>
#include <xdrpp/message.h>
#include <xdrpp/pollset.h>

namespace xdr {

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
 * body (possibly including the next message length).  This could be
 * fixed to read at least a little bit more data speculatively and
 * reduce the number of system calls.
 */
class msg_sock {
public:
  using rcb_t = std::function<void(msg_ptr)>;

  template<typename T> msg_sock(pollset &ps, sock_t s, T &&rcb,
				size_t maxmsglen = 0x100000)
    : ps_(ps), s_(s), maxmsglen_(maxmsglen), rcb_(std::forward<T>(rcb)) {
    init();
  }
  msg_sock(pollset &ps, sock_t s) : msg_sock(ps, s, nullptr) {}
  ~msg_sock();
  msg_sock &operator=(msg_sock &&) = delete;

  template<typename T> void setrcb(T &&rcb) {
    rcb_ = std::forward<T>(rcb);
    initcb();
  }

  size_t wsize() const { return wsize_; }
  void putmsg(msg_ptr &b);
  void putmsg(msg_ptr &&b) { putmsg(b); }
  //! Returns pointer to a \c bool that becomes \c true once the
  //! msg_sock has been deleted.
  std::shared_ptr<const bool> destroyed_ptr() const { return destroyed_; }
  pollset &get_pollset() { return ps_; }

private:
  pollset &ps_;
  const sock_t s_;
  const size_t maxmsglen_;
  std::shared_ptr<bool> destroyed_{std::make_shared<bool>(false)};

  rcb_t rcb_;
  uint32_t nextlen_;
  msg_ptr rdmsg_;
  size_t rdpos_ {0};

  std::deque<msg_ptr> wqueue_;
  size_t wsize_ {0};
  size_t wstart_ {0};
  bool wfail_ {false};

  static constexpr bool eagain(int err) {
    return err == EAGAIN || err == EWOULDBLOCK || err == EINTR;
  }
  char *nextlenp() { return reinterpret_cast<char *>(&nextlen_); }
  uint32_t nextlen() const { return swap32le(nextlen_); }

  void init();
  void initcb();
  void input();
  void pop_wbytes(size_t n);
  void output(bool cbset);
};

//! Functor wrapper around \c msg_sock::putmsg.  Mostly useful because
//! std::function implementations avoid memory allocation with \c
//! operator() but not when other methods are passed to \c std::bind.
struct msg_sock_put_t {
  msg_sock *ms_;
  constexpr msg_sock_put_t(msg_sock *ms) : ms_(ms) {}
  void operator()(msg_ptr b) const { ms_->putmsg(b); }
};

}

#endif // !_XDRPP_MSGSOCK_H_INCLUDED_
