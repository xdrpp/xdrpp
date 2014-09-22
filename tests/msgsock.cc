
#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include <sys/socket.h>
#include <sodium.h>
#include <xdrc/msgsock.h>

#include <xdrc/printer.h>

using namespace std;
using namespace xdr;

void
echoserver(int fd)
{
  PollSet ps;
  bool done {false};
  SeqSock ss(&ps, fd, nullptr);
  int i = 0;

  ss.setrcb([&done,&ss,&i](MsgBuf b) {
      cerr << "echoing #" << ++i << " (" << b->size() << " bytes)" << endl;
      if (b)
	ss.putmsg(b);
      else
	done = true;
    });

  while (!done && ps.pending())
    ps.poll();
}

void
echoclient(int fd)
{
  PollSet ps;
  SeqSock ss { &ps, fd, nullptr };
  unsigned int i = 1;

  {
    MsgBuf b (i);
    ss.putmsg(b);
  }
  ss.setrcb([&i,&ss](MsgBuf b) {
      assert(b->size() == i);
      cerr << b->size() << ": " << hexdump(b->data(), b->size()) << std::endl;
      for (unsigned j = 0; j < b->size(); j++) {
	//assert(unsigned(b->data()[j]) == (unsigned(i) & 0xff));
      }
      ++i;
      b = MsgBuf(i);
      for (unsigned j = 0; j < i; j++)
	b->data()[j] = i;
      ss.putmsg(b);
    });

  while (i < 100 && ps.pending())
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

  thread t1 (echoclient, fds[0]);
  echoserver(fds[1]);
  t1.join();

  return 0;
}
