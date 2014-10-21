#include <surface/gap/Rounds.hh>

#include <infinit/oracles/meta/Client.hh>

#include <station/Station.hh>
#include <station/Host.hh>
#include <station/AlreadyConnected.hh>

#include <reactor/Barrier.hh>
#include <reactor/Scope.hh>
#include <reactor/exception.hh>
#include <reactor/network/buffer.hh>
#include <reactor/network/fingerprinted-socket.hh>
#include <reactor/scheduler.hh>

#include <elle/With.hh>
#include <elle/format/json/Dictionary.hh>
#include <elle/container/vector.hh>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>

ELLE_LOG_COMPONENT("infinit.surface.gap.Rounds");

static const std::vector<unsigned char> fingerprint =
{
  0x98, 0x55, 0xEF, 0x72, 0x1D, 0xFC, 0x1B, 0xF5, 0xEA, 0xF5,
  0x35, 0xC5, 0xF9, 0x32, 0x85, 0x38, 0x38, 0x2C, 0xCA, 0x91
};

namespace surface
{
  namespace gap
  {
    Round::Round(std::string const& name)
      : _name(name)
    {}

    Round::~Round()
    {}

    AddressRound::AddressRound(std::string const& name,
                               Endpoints endpoints)
      : Round(name)
      , _endpoints(std::move(endpoints))
    {}

    std::unique_ptr<station::Host>
    AddressRound::connect(station::Station& station)
    {
      return elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
      {
        std::unique_ptr<station::Host> host;
        reactor::Barrier found;
        for (auto const& endpoint: this->_endpoints)
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
        found.wait(2_sec);
        if (host)
          return std::move(host);
        return std::unique_ptr<station::Host>();
      };
    }

    std::unique_ptr<station::Host>
    AddressRound::_connect(station::Station& station,
                           std::pair<std::string, int> const& endpoint)
    {
      try
      {
        auto const& ip = endpoint.first;
        auto const& port = endpoint.second;
        ELLE_DEBUG("%s: try to negociate connection with %s:%s",
                   *this, ip, port);
        // XXX: This statement is ok while we are connecting to one peer.
        auto res = station.connect(ip, port);
        ELLE_LOG("%s: connection to %s:%s succeed",
                 *this, ip, port);
        return res;
      }
      catch (station::AlreadyConnected const&)
      {
        ELLE_LOG("%s: connection to %s:%s succeed (already connected)",
                 *this, endpoint.first, endpoint.second);
      }
      catch (reactor::Terminate const&)
      {
        throw ;
      }
      catch (std::exception const& e)
      {
        ELLE_WARN("%s: connection to %s:%s failed: %s",
                  *this, endpoint.first, endpoint.second,
                  elle::exception_string());
      }
      return nullptr;
    }

    void
    AddressRound::print(std::ostream& stream) const
    {
      stream << "direct connection round";
    };

    FallbackRound::FallbackRound(std::string const& name,
                                 oracles::meta::Client const& meta,
                                 std::string const& uid)
      : Round(name)
      , _meta(meta)
      , _uid(uid)
    {}

    std::unique_ptr<station::Host>
    FallbackRound::connect(station::Station& station)
    {
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      ELLE_TRACE("%s: get fallback from meta", *this);
      auto fallback = this->_meta.fallback(this->_uid);
      ELLE_TRACE_SCOPE("%s: connect to SSL apertus %s:%s",
                       *this,
                       fallback.fallback_host,
                       fallback.fallback_port_ssl);
      std::unique_ptr<reactor::network::FingerprintedSocket> sock(
        new reactor::network::FingerprintedSocket(
          fallback.fallback_host,
          boost::lexical_cast<std::string>(fallback.fallback_port_ssl),
          fingerprint));
      sock->shutdown_asynchronous(true);
      ELLE_DUMP("%s: send transaction key %s (of length: %i)",
                *this, this->_uid, this->_uid.size());
      char version = 0;
      sock->write(elle::ConstWeakBuffer(&version, 1));
      char c = this->_uid.size();
      sock->write(elle::ConstWeakBuffer(&c, 1));
      sock->write(elle::ConstWeakBuffer(this->_uid));
      // Wait for sync bit.
      ELLE_TRACE("%s: wait for peer on apertus", *this);
      sock->read(1);
      return elle::make_unique<station::Host>(std::move(sock));
    }

    void
    FallbackRound::print(std::ostream& stream) const
    {
      stream << "fallback round";
    };
  }
}
