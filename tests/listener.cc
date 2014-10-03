
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <thread>
#include <xdrpp/srpc.h>
#include <xdrpp/rpcbind.hh>
#include <xdrpp/socket.h>
#include "tests/xdrtest.hh"

using namespace std;
using namespace xdr;

using namespace testns;


class xdrtest2_server {
public:
  using rpc_interface_type = xdrtest2;

  void null2();
  std::unique_ptr<ContainsEnum> nonnull2(std::unique_ptr<u_4_12> arg);
  void ut(std::unique_ptr<uniontest> arg);
};


void
xdrtest2_server::null2()
{
  
  cout << "null2" << endl;
  
}

std::unique_ptr<ContainsEnum>
xdrtest2_server::nonnull2(std::unique_ptr<u_4_12> arg)
{
  std::unique_ptr<ContainsEnum> res(new ContainsEnum);
  
  cout << xdr_to_string(*arg, "nonnull2 arg");
  res->c(::RED).foo() = "Hello, world\n";
  
  return res;
}

void
xdrtest2_server::ut(std::unique_ptr<uniontest> arg)
{
  
  cout << xdr_to_string(*arg, "ut arg");
  
}

int
main(int argc, char **argv)
{
  if (argc > 1 && !strcmp(argv[1], "-s")) {
    xdrtest2_server s;
    rpc_tcp_listener rl;
    rl.register_service(s);
    rl.run();
  }
  else if (argc > 1 && !strcmp(argv[1], "-c")) {
    auto fd = tcp_connect_rpc(argc > 2 ? argv[2] : nullptr,
			      xdrtest2::program, xdrtest2::version);
    srpc_client<xdrtest2> c{fd.get()};

    c.null2();

    u_4_12 u(12);
    u.f12().i = 1977;
    auto r = c.nonnull2(u);
    cout << xdr_to_string(*r, "nonnull2 reply");
  }
  else
    cerr << "need -s or -c option" << endl;
  return 0;
}
