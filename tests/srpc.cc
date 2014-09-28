
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <thread>
#include <xdrc/srpc.h>
#include "xdrtest.hh"

using namespace std;
using namespace xdr;

void
getmsg(int fd)
{
  msg_ptr p;
  try { p = read_message(fd); }
  catch (const std::exception &e) {
    cerr << "caught " << e.what() << endl;
    close(fd);
    return;
  }

  cerr << "got " << p->size() << "-byte message" << endl;

  xdr_get g(p);
  rpc_msg hdr;
  archive(g, hdr);
  
  cerr << xdr_to_string(hdr);

  close(fd);
}

void
sendreq(int fd)
{
  testns::xdrtest2_t::client<synchronous_client> sc(fd);

  u_4_12 arg(12);
  arg.f12().i = 77;
  arg.f12().d = 3.141592654;
  testns::ContainsEnum ce = sc.nonnull2(arg);

  cout << xdr_to_string(ce);
}

int
main()
{
  int fds[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1) {
    perror("socketpair");
    exit(1);
  }

  thread t1(sendreq, fds[0]);
  getmsg(fds[1]);
  
  t1.join();

  return 0;
}
