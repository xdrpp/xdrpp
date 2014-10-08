
#include <xdrpp/arpc.h>

using namespace std;
using namespace xdr;

void
check_rpc_success_header()
{
  msg_ptr m1 (xdr_to_msg(rpc_msg(7, REPLY)));
  msg_ptr m2 (xdr_to_msg(rpc_success_hdr(7)));

  assert(m1->size() == m2->size());
  assert(!memcmp(m1->data(), m2->data(), m1->size()));
}

int
main()
{
  check_rpc_success_header();
  return 0;
}
