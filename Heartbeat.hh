#ifndef INFINIT_HEARTBEAT_HH
# define INFINIT_HEARTBEAT_HH

# include <reactor/thread.hh>
# include <reactor/network/udp-socket.hh>

namespace infinit
{
  namespace heartbeat
  {
    reactor::Thread*
    start(reactor::network::UDPSocket& s);
  } /* heartbeat */
} /* infinit */

#endif /* end of include guard: INFINIT_HEARTBEAT_HH */
