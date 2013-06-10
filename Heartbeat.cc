#include <Heartbeat.hh>

#include <elle/log.hh>

#include <common/common.hh>

#include <reactor/sleep.hh>
#include <reactor/scheduler.hh>
#include <reactor/network/buffer.hh>
#include <reactor/network/exception.hh>
#include <reactor/network/udt-socket.hh>

ELLE_LOG_COMPONENT("infinit.heartbeat");

static const int max_tries = 10;
static const int sleep_time = 5;

namespace infinit
{
  namespace heartbeat
  {
    namespace network = reactor::network;

    reactor::Thread*
    start(network::UDPSocket& sock)
    {
      auto& sched = *reactor::Scheduler::scheduler();
      auto heartbeat = [&]
      {
        namespace network = reactor::network;

        auto* ptr = &sock;
        auto th_sleep = [&] (int sec)
        {
          reactor::Sleep s{sched, boost::posix_time::seconds{sec}};
          s.run();
        };

        try
        {
          network::UDTSocket hsocket(sched,
                                    *ptr,
                                    common::heartbeat::host(),
                                    std::to_string(common::heartbeat::port()));

          int tries = max_tries;
          for (int i = 0; i < tries; ++i)
          {
            std::string msg{"echo"};
            hsocket.write(network::Buffer{msg});
            msg.resize(512);
            size_t bytes = hsocket.read_some(network::Buffer{msg});
            msg.resize(bytes);
            ELLE_DUMP("received %s", msg);
            th_sleep(sleep_time);
          }
        }
        catch (network::ConnectionClosed const&)
        {
          ELLE_LOG("lose connection with %s", socket);
        }
        catch (elle::Exception const& e)
        {
          ELLE_LOG("exception handled: %s", e.what());
        }
      };
      return new reactor::Thread{sched, "heartbeat", std::move(heartbeat)};
    }
  } /* heartbeat */
} /* infinit */
