#include <surface/gap/Rounds.hh>

#include <station/Station.hh>
#include <station/Host.hh>
#include <station/AlreadyConnected.hh>

#include <reactor/Barrier.hh>
#include <reactor/Scope.hh>
#include <reactor/exception.hh>
#include <reactor/network/buffer.hh>
#include <reactor/network/tcp-socket.hh>
#include <reactor/scheduler.hh>

#include <elle/With.hh>
#include <elle/format/json/Dictionary.hh>
#include <elle/serialize/JSONArchive.hh>
#include <elle/container/vector.hh>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>

ELLE_LOG_COMPONENT("infinit.surface.gap.Rounds");

namespace surface
{
  namespace gap
  {
    static
    std::unique_ptr<station::Host>
    _connect(station::Station& station,
             std::string const& endpoint)
    {
      try
      {
        std::vector<std::string> result;
        boost::split(result, endpoint, boost::is_any_of(":"));

        auto const &ip = result[0];
        auto const &port = result[1];
        ELLE_DEBUG("try to negociate connection with %s", endpoint);
        // XXX: This statement is ok while we are connecting to one peer.
        auto res = station.connect(ip, std::stoi(port));
        ELLE_LOG("connection to %s succeed", endpoint);
        return res;
      }
      catch (station::AlreadyConnected const&)
      {
        ELLE_LOG("connection to %s succeed (already connected)", endpoint);
      }
      catch (reactor::Terminate const&)
      {
        throw ;
      }
      catch (std::exception const& e)
      {
        ELLE_WARN("connection to %s failed: %s",
                  endpoint, elle::exception_string());
      }
      return nullptr;
    }

    Round::Round(std::string const& name):
      _name{name},
      _endpoints{}
    {}

    Round::~Round()
    {
    }

    /*----------.
    | Printable |
    `----------*/
    void
    Round::print(std::ostream& stream) const
    {
      stream << this->type() << " " << this->_name << " with "
             << this->_endpoints.size() << " endpoint(s): " << this->_endpoints;
    };

    AddressRound::AddressRound(std::string const& name,
                               std::vector<std::string>&& endpoints):
      Round(name)
    {
      this->_endpoints = std::move(endpoints);
      ELLE_TRACE("creating AddressRound(%s, %s)", name, this->_endpoints);
    }

    std::unique_ptr<station::Host>
    AddressRound::connect(station::Station& station)
    {
      return elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
      {
        std::unique_ptr<station::Host> host;
        reactor::Barrier found;
        for (std::string const& endpoint: this->_endpoints)
        {
          scope.run_background(
            elle::sprintf("connect_try(%s)", endpoint),
            [&]
            {
              host = _connect(station, endpoint);
              found.open();
            });
        }

        found.wait(10_sec);
        return std::move(host);
      };
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

    std::unique_ptr<station::Host>
    FallbackRound::connect(station::Station& station)
    {
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      auto& sched = *reactor::Scheduler::scheduler();
      ELLE_LOG("%s: contact master Apertus: %s:%s",
               *this, this->_host, this->_port);
      reactor::network::TCPSocket sock{sched, this->_host, this->_port};
      elle::format::json::Dictionary dict;

      dict["_id"] = this->_uid;
      dict["request"] = "add_link";
      ELLE_DEBUG("%s: request to apertus: %s", *this, dict.repr());

      sock.write(elle::ConstWeakBuffer(dict.repr() + "\n")) ;

      std::string data(512, '\0');
      size_t bytes = sock.read_some(reactor::network::Buffer(data));
      data.resize(bytes);
      std::stringstream ss;
      ss << data;
      dict = elle::format::json::parse(ss)->as_dictionary();
      std::string address = dict["endpoint"].as_string();
      ELLE_TRACE("%s: got slave apertus: %s", *this, address);
      this->_endpoints = {address};

      return elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
      {
        std::unique_ptr<station::Host> host;
        reactor::Barrier found;
        for (std::string const& endpoint: this->_endpoints)
        {
          scope.run_background(
            elle::sprintf("connect_try(%s)", endpoint),
            [&]
            {
              host = _connect(station, endpoint);
              found.open();
            });
        }

        found.wait();
        return host;
      };
    }
  }
}
