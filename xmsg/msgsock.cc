
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/uio.h>
#include "msgsock.h"

namespace xdr {

MsgBuf::MsgBuf(size_t len)
{
  if (void *p = ::operator new(offsetof(MsgBufImpl, buf_[len])))
    reset(new (p) MsgBufImpl(len));
  else
    throw std::bad_alloc();
}

SeqSock::~SeqSock()
{
  ps_.fd_cb(fd_, PollSet::ReadWrite);
  really_close(fd_);
}

void
SeqSock::init()
{
  set_nonblock(fd_);
  ps_.fd_cb(fd_, PollSet::Read, [this](){ input(); });
}

void
SeqSock::input()
{
  if (rdmsg_) {
    iovec iov[2];
    iov[0].iov_base = rdmsg_->data() + rdpos_;
    iov[0].iov_len = rdmsg_->size() - rdpos_;
    iov[1].iov_base = nextlenp();
    iov[1].iov_len = sizeof nextlen_;
    ssize_t n = readv(fd_, iov, 2);
    if (n <= 0) {
      std::cerr << "rdmsg_->size() = " << rdmsg_->size()
		<< ", rdpos_ = " << rdpos_
		<< ", iov[0].iov_len = " << iov[0].iov_len
		<< std::endl;
      std::cerr << "SeqSock::input: " << std::strerror(errno) << std::endl;
      if (n < 0 && eagain(errno))
	return;
      if (n == 0)
	errno = ECONNRESET;
      rcb_(nullptr);
      return;
    }
    rdpos_ += n;
    if (rdpos_ >= rdmsg_->size()) {
      rdpos_ -= rdmsg_->size();
      rcb_ (std::move(rdmsg_));
    }
  }
  else if (rdpos_ < sizeof nextlen_) {
    ssize_t n = read(fd_, nextlenp() + rdpos_, sizeof nextlen_ - rdpos_);
    if (n <= 0) {
      std::cerr << "read failed with " << strerror(errno) << std::endl;
      if (n < 0 && eagain(errno))
	return;
      if (n == 0)
	errno = rdpos_ ? ECONNRESET : 0;
      rcb_(nullptr);
      return;
    }
    rdpos_ += n;
  }
  if (!rdmsg_ && rdpos_ == sizeof nextlen_) {
    size_t len = nextlen();
    if (len <= maxmsglen_) {
      // Length comes from untrusted source; don't crash if can't alloc
      try { rdmsg_ = MsgBuf(len); }
      catch (const std::bad_alloc &) {
	std::cerr << "SeqSock: allocation of " << len << "-byte message failed"
		  << std::endl;
      }
    }
    else {
      std::cerr << "SeqSock: rejecting " << len << "-byte message (too long)"
		<< std::endl;
      ps_.fd_cb(fd_, PollSet::Read);
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
SeqSock::putmsg(MsgBuf &mb)
{
  if (wfail_) {
    mb.reset();
    return;
  }

  bool was_empty = !wsize_;
  wsize_ += mb->rawSize();
  wqueue_.emplace_back(mb.release());
  if (was_empty)
    output(false);
}

void
SeqSock::pop_wbytes(size_t n)
{
  if (n == 0)
    return;
  std::cerr << "n " << n << ", wsize " << wsize_ << std::endl;
  assert (n <= wsize_);
  wsize_ -= n;
  size_t frontbytes = wqueue_.front()->rawSize() - wstart_;
  if (n < frontbytes) {
    wstart_ += n;
    return;
  }
  n -= frontbytes;
  wqueue_.pop_front();
  while (n > 0 && n >= (frontbytes = wqueue_.front()->rawSize())) {
    n -= frontbytes;
    wqueue_.pop_front();
  }
  wstart_ = n;
}

void
SeqSock::output(bool cbset)
{
  std::cerr << "SeqSock::output\n";

  static constexpr size_t maxiov = 8;
  size_t i = 0;
  iovec v[maxiov];
  for (auto b = wqueue_.begin(); i < maxiov && b != wqueue_.end(); ++b, ++i) {
    if (i) {
      v[i].iov_len = (*b)->rawSize();
      v[i].iov_base = const_cast<char *> ((*b)->rawData());
    }
    else {
      v[i].iov_len = (*b)->rawSize() - wstart_;
      v[i].iov_base = const_cast<char *> ((*b)->rawData() + wstart_);
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
    ps_.fd_cb(fd_, PollSet::Write, [this](){ output(true); });
  else if (!wsize_ && cbset)
    ps_.fd_cb(fd_, PollSet::Write);
}

}
