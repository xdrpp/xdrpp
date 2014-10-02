
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/uio.h>

#include <xdrpp/msgsock.h>

namespace xdr {

msg_sock::~msg_sock()
{
  ps_.fd_cb(fd_, pollset::ReadWrite);
  really_close(fd_);
  *destroyed_ = true;
}

void
msg_sock::init()
{
  set_nonblock(fd_);
  initcb();
}

void
msg_sock::initcb()
{
  if (rcb_)
    ps_.fd_cb(fd_, pollset::Read, [this](){ input(); });
  else
    ps_.fd_cb(fd_, pollset::Read);
}

void
msg_sock::input()
{
  std::shared_ptr<bool> destroyed{destroyed_};
  for (int i = 0; i < 3 && !*destroyed; i++) {
    if (rdmsg_) {
      iovec iov[2];
      iov[0].iov_base = rdmsg_->data() + rdpos_;
      iov[0].iov_len = rdmsg_->size() - rdpos_;
      iov[1].iov_base = nextlenp();
      iov[1].iov_len = sizeof nextlen_;
      ssize_t n = readv(fd_, iov, 2);
      if (n <= 0) {
	if (n < 0 && eagain(errno))
	  return;
	if (n == 0)
	  errno = ECONNRESET;
	else
	  std::cerr << "msg_sock::input: " << std::strerror(errno) << std::endl;
	rcb_(nullptr);
	return;
      }
      rdpos_ += n;
      if (rdpos_ >= rdmsg_->size()) {
	rdpos_ -= rdmsg_->size();
	rcb_(std::move(rdmsg_));
	if (*destroyed)
	  return;
      }
    }
    else if (rdpos_ < sizeof nextlen_) {
      ssize_t n = read(fd_, nextlenp() + rdpos_, sizeof nextlen_ - rdpos_);
      if (n <= 0) {
	if (n < 0 && eagain(errno))
	  return;
	if (n == 0)
	  errno = rdpos_ ? ECONNRESET : 0;
	else
	  std::cerr << "msg_sock::input: " << std::strerror(errno) << std::endl;
	rcb_(nullptr);
	return;
      }
      rdpos_ += n;
    }

    if (rdmsg_ || rdpos_ < sizeof nextlen_)
      return;
    size_t len = nextlen();
    if (!(len & 0x80000000)) {
      std::cerr << "msgsock: message fragments unimplemented" << std::endl;
      errno = ECONNRESET;
      rcb_(nullptr);
      return;
    }
    len &= 0x7fffffff;
    if (!len) {
      rdpos_ = 0;
      rcb_(message_t::alloc(0));
      continue;
    }

    if (len <= maxmsglen_) {
      // Length comes from untrusted source; don't crash if can't alloc
      try { rdmsg_ = message_t::alloc(len); }
      catch (const std::bad_alloc &) {
	std::cerr << "msg_sock: allocation of " << len << "-byte message failed"
		  << std::endl;
      }
    }
    else {
      std::cerr << "msg_sock: rejecting " << len << "-byte message (too long)"
		<< std::endl;
      ps_.fd_cb(fd_, pollset::Read);
    }
    if (rdmsg_)
      rdpos_ = 0;
    else {
      errno = E2BIG;
      rcb_(nullptr);
    }
  }
}

void
msg_sock::putmsg(msg_ptr &mb)
{
  if (wfail_) {
    mb.reset();
    return;
  }

  bool was_empty = !wsize_;
  wsize_ += mb->raw_size();
  wqueue_.emplace_back(mb.release());
  if (was_empty)
    output(false);
}

void
msg_sock::pop_wbytes(size_t n)
{
  if (n == 0)
    return;
  assert (n <= wsize_);
  wsize_ -= n;
  size_t frontbytes = wqueue_.front()->raw_size() - wstart_;
  if (n < frontbytes) {
    wstart_ += n;
    return;
  }
  n -= frontbytes;
  wqueue_.pop_front();
  while (n > 0 && n >= (frontbytes = wqueue_.front()->raw_size())) {
    n -= frontbytes;
    wqueue_.pop_front();
  }
  wstart_ = n;
}

void
msg_sock::output(bool cbset)
{
  static constexpr size_t maxiov = 8;
  size_t i = 0;
  iovec v[maxiov];
  for (auto b = wqueue_.begin(); i < maxiov && b != wqueue_.end(); ++b, ++i) {
    if (i) {
      v[i].iov_len = (*b)->raw_size();
      v[i].iov_base = const_cast<char *> ((*b)->raw_data());
    }
    else {
      v[i].iov_len = (*b)->raw_size() - wstart_;
      v[i].iov_base = const_cast<char *> ((*b)->raw_data()) + wstart_;
    }
  }
  ssize_t n = writev(fd_, v, i);
  if (n <= 0) {
    if (n != -1 || !eagain(errno)) {
      wfail_ = true;
      wsize_ = wstart_ = 0;
      wqueue_.clear();
    }
    return;
  }
  pop_wbytes(n);

  if (wsize_ && !cbset)
    ps_.fd_cb(fd_, pollset::Write, [this](){ output(true); });
  else if (!wsize_ && cbset)
    ps_.fd_cb(fd_, pollset::Write);
}

}
