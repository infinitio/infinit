#ifndef INFINIT_HEARBEAT_HH
# define INFINIT_HEARBEAT_HH

# include <reactor/thread.hh>
# include <reactor/network/udp-socket.hh>

namespace heartbeat
{
  reactor::Thread*
  start(reactor::network::UDPSocket& s);
} /* infinit */

#endif /* end of include guard: INFINIT_HEARBEAT_HH */
