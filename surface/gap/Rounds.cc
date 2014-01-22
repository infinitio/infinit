#include <surface/gap/Rounds.hh>

# include <infinit/oracles/meta/Client.hh>

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

    std::unique_ptr<reactor::network::TCPSocket>
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
              if (host != nullptr)
                found.open();
            });
        }

        found.wait(10_sec);
        if (host)
          return std::move(host->release());
        return std::unique_ptr<reactor::network::TCPSocket>();
      };
    }

    FallbackRound::FallbackRound(std::string const& name,
                                 oracles::meta::Client const& meta,
                                 std::string const& uid):
      Round(name),
      _meta(meta),
      _uid{uid}
    {
      ELLE_TRACE("creating FallbackRound(%s, %s)", name, uid);
    }

    std::unique_ptr<reactor::network::TCPSocket>
    FallbackRound::connect(station::Station& station)
    {
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      ELLE_DEBUG("%s: get fallback from meta", *this);
      std::string fallback =
        this->_meta.fallback(this->_uid).fallback;

      ELLE_DEBUG("%s: connect to apertus %s", *this, fallback);
      std::vector<std::string> splited;
      boost::algorithm::split(splited, fallback, boost::is_any_of(":"));

      std::string host = splited[0];
      int port = std::stoi(splited[1]);

      ELLE_TRACE("%s: contact apertus: %s:%s", *this, host, port);

      std::unique_ptr<reactor::network::TCPSocket> sock(
        new reactor::network::TCPSocket(host, port));

      ELLE_LOG("%i: %s", this->_uid.size(), this->_uid);
      sock->write(elle::ConstWeakBuffer(elle::sprintf("%c",(char) this->_uid.size())));
      sock->write(elle::ConstWeakBuffer(elle::sprintf("%s", this->_uid)));

      return std::move(sock);
    }
  }
}
