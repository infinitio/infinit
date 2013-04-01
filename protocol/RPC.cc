#include <elle/print.hh>

#include <reactor/exception.hh>

#include <protocol/RPC.hh>

ELLE_LOG_COMPONENT("infinit.protocol.RPC");

namespace infinit
{
  namespace protocol
  {
    BaseRPC::BaseRPC(ChanneledStream& channels)
      : _channels(channels)
      , _id(0)
      , _threads()
    {}

    void
    BaseRPC::_terminate_rpcs()
    {
      ELLE_TRACE_SCOPE("%s: terminate running RPCs", *this);
      for (auto const& thread: this->_threads)
        thread->first->terminate_now();
    }
  }
}
