
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include <sys/socket.h>
#include <sodium.h>
#include "xdrc/msgsock.h"

using namespace std;
using namespace xdr;

void
getmsgbuf(MsgBuf b)
{
  if (b)
    cerr << this_thread::get_id() << " received " << b->size()
	 << " bytes" << endl << b->data() << endl;
  else
    cerr << this_thread::get_id() << " received " << strerror(errno) << '\n';
}

void
testsock(int fd)
{
  PollSet ps;
  SeqSock ss (&ps, fd, getmsgbuf);
#if 0
  ostringstream oss;
  oss << "I am " << this_thread::get_id() << " and my counter is "
      << ++i << endl;
  string s(oss.str());
  MsgBuf b (s.size());
  memcpy(b->data(), s.data(), s.size());
  cerr << "sending: " << s << endl;
  ss.putmsg(b);
  cerr << "sent: " << s << endl;
#else
  MsgBuf b(1);
  b->data()[0] = 'A';
  ss.putmsg(b);
#endif
  while (ps.pending())
    ps.poll();
}

int
main(int argc, char **argv)
{
  int fds[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1) {
    perror("socketpair");
    exit(1);
  }

  thread t1 (testsock, fds[0]);
  testsock(fds[1]);
  t1.join();

  return 0;
}
