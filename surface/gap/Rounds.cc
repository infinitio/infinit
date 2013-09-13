#include <surface/gap/Rounds.hh>

#include <reactor/exception.hh>
#include <reactor/network/buffer.hh>
#include <reactor/network/tcp-socket.hh>
#include <reactor/scheduler.hh>

#include <elle/format/json/Dictionary.hh>
#include <elle/serialize/JSONArchive.hh>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>

ELLE_LOG_COMPONENT("infinit.surface.gap.Rounds");

namespace surface
{
  namespace gap
  {
    Round::Round(std::string const& name):
      _name{name}
    {}

    Round::~Round()
    {
    }

    AddressRound::AddressRound(std::string const& name,
                               std::vector<std::string>&& endpoints):
      Round(name),
      _endpoints(std::move(endpoints))
    {
      ELLE_TRACE("creating AddressRound(%s, %s)", name, this->_endpoints);
    }

    std::vector<std::string>
    AddressRound::endpoints()
    {
      return this->_endpoints;
    }

    FallbackRound::FallbackRound(std::string const& name,
                                 std::string const& host,
                                 int port,
                                 std::string const& uid):
      Round(name),
      _host{host},
      _port{port},
      _uid{uid}
    {
      ELLE_TRACE("creating FallbackRound(%s, %s, %s, %s)", name, host, port, uid);
    }

    std::vector<std::string>
    FallbackRound::endpoints()
    {
      try
      {
        // Check if the endpoints are not cached.
        if (!this->_endpoints.empty())
          return this->_endpoints;

        // else, quick talk with apertus to get the new endpoints.
        ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
        auto& sched = *reactor::Scheduler::scheduler();
        ELLE_LOG("%s: contact Apertus: %s:%s", *this, this->_host, this->_port);
        reactor::network::TCPSocket sock{sched, this->_host, this->_port};
        elle::format::json::Dictionary dict;

        dict["_id"] = this->_uid;
        dict["request"] = "add_link";
        sock.write(reactor::network::Buffer(dict.repr() + "\n"));

        std::string data(512, '\0');
        size_t bytes = sock.read_some(reactor::network::Buffer(data));
        data.resize(bytes);
        std::stringstream ss;
        ss << data;
        dict = elle::format::json::parse(ss)->as_dictionary();
        std::string address = dict["endpoint"].as_string();
        ELLE_TRACE("%s: got %s from Apertus", address);
        this->_endpoints = {address};
        return this->_endpoints;
      }
      catch (reactor::Terminate const&)
      {
        throw;
      }
      catch (std::exception const&)
      {
        ELLE_ERR("%s: unable to contact fall back server: %s",
                 *this, elle::exception_string());
        return this->_endpoints; // empty list.
      }
    }
  }
}
