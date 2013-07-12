#include <surface/gap/Rounds.hh>
#include <reactor/network/tcp-socket.hh>
#include <reactor/network/buffer.hh>
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
    Round::Round():
      _name{}
    {}

    Round::~Round()
    {
    }

    Round::Round(Round&& r):
      _name{std::move(r._name)}
    {}

    Round::Round(Round const& r):
      _name{r._name}
    {
    }

    std::vector<std::string>
    AddressRound::endpoints()
    {
      return this->_endpoints;
    }

    void
    AddressRound::endpoints(std::vector<std::string> const& addrs)
    {
      this->_endpoints = addrs;
    }

    FallbackRound::FallbackRound(std::string const& host,
                                 int port,
                                 std::string const& uid):
      Round{},
      _host{host},
      _port{port},
      _uid{uid}
    {
      ELLE_TRACE("creating FallbackRound(%s, %s, %s)", host, port, uid);
    }

    std::vector<std::string>
    FallbackRound::endpoints()
    {
      // Check if the endpoints are not cached.
      if (!this->_endpoints.empty())
        return this->_endpoints;

      // else, quick talk with apertus to get the new endpoints.
      auto& sched = *reactor::Scheduler::scheduler();
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
      ELLE_TRACE("Have %s", address);
      this->_endpoints = {address};
      return this->_endpoints;
    }
  }
}
