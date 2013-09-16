#include <memory>

#include <elle/network/Interface.hh>

#include <reactor/network/nat.hh>
#include <reactor/network/resolve.hh>

#include <agent/Agent.hh>

#include <common/common.hh>

#include <papier/Descriptor.hh>

#include <hole/implementations/local/Implementation.hh>
#include <hole/implementations/remote/Implementation.hh>
#include <hole/implementations/slug/Manifest.hh>
#include <hole/implementations/slug/Slug.hh>

#include <papier/Authority.hh>
#include <papier/Passport.hh>

#include <plasma/meta/Client.hh>

#include <HoleFactory.hh>
#include <Infinit.hh>
#include <Portal.hh>

ELLE_LOG_COMPONENT("infinit.HoleFactory");

namespace infinit
{
  class PortaledSlug:
    public hole::implementations::slug::Slug
  {
  public:
    typedef hole::implementations::slug::Slug Super;
    PortaledSlug(hole::storage::Storage& storage,
                 papier::Passport const& passport,
                 papier::Authority const& authority,
                 reactor::network::Protocol protocol,
                 std::vector<elle::network::Locus> const& members,
                 int port,
                 reactor::Duration connection_timeout,
                 std::unique_ptr<reactor::network::UDPSocket> socket):
      Super(storage, passport, authority,
            protocol, members, port, connection_timeout, std::move(socket)),
      _portal(
        "slug",
        [&](hole::implementations::slug::control::RPC& rpcs)
        {
          rpcs.slug_connect = std::bind(&Slug::portal_connect,
                                        this,
                                        std::placeholders::_1,
                                        std::placeholders::_2,
                                        true);
        }
        )
    {}

    ~PortaledSlug()
    {}

    Portal<hole::implementations::slug::control::RPC> _portal;
  };

  std::unique_ptr<hole::Hole>
  hole_factory(papier::Descriptor const& descriptor,
               hole::storage::Storage& storage,
               papier::Passport const& passport,
               papier::Authority const& authority,
               std::vector<elle::network::Locus> const& members,
               std::string const& _meta_host,
               uint16_t _meta_port,
               std::string const& token)
  {
    switch (descriptor.meta().model().type)
      {
        case hole::Model::TypeLocal:
        {
          return std::unique_ptr<hole::Hole>(
            new hole::implementations::local::Implementation(
              storage, passport, authority));
          break;
        }
        // case hole::Model::TypeRemote:
        // {
        //   // Retrieve the locus.
        //   elle::network::Locus locus = *set.loci.begin();
        //   return std::unique_ptr<hole::Hole>(
        //     new hole::implementations::remote::Implementation(
        //       storage, passport, authority, locus));
        // }
        case hole::Model::TypeSlug:
        {
          int port = Infinit::Configuration["hole"].Get("slug.port", 0);
          int timeout_int =
            Infinit::Configuration["hole"].Get("slug.timeout", 5000);
          reactor::Duration timeout =
            boost::posix_time::milliseconds(timeout_int);
          std::string protocol_str =
            Infinit::Configuration["hole"].Get<std::string>("protocol", "udt");
          protocol_str =
            Infinit::Configuration["hole"].Get<std::string>("slug.protocol",
                                                            protocol_str);
          reactor::network::Protocol protocol;
          if (protocol_str == "tcp")
            protocol = reactor::network::Protocol::tcp;
          else if (protocol_str == "udt")
            protocol = reactor::network::Protocol::udt;
          else
            throw elle::Exception
              (elle::sprintf("invalid transport protocol: %s", protocol_str));

          // Punch NAT.
          auto& sched = *reactor::Scheduler::scheduler();
          std::unique_ptr<reactor::network::UDPSocket> socket;
          boost::asio::ip::udp::endpoint pub;
          try
          {
#if defined(REACTOR_HAVE_STUN)
            reactor::nat::NAT nat(sched);
            std::string stun_host = common::stun::host(),
              stun_port = std::to_string(common::stun::port());

            ELLE_DEBUG("connecting to stun host %s:%s", stun_host, stun_port);
            auto host = reactor::network::resolve_udp(sched, stun_host,
                                                      stun_port);
            auto breach = nat.map(host);

            using reactor::nat::Breach;
            if (breach.nat_behavior() == Breach::NatBehavior::EndpointIndependentMapping ||
                breach.nat_behavior() == Breach::NatBehavior::DirectMapping)
            {
              ELLE_TRACE("breach done: %s", breach.mapped_endpoint());
              socket = std::move(breach.take_handle());
              pub = breach.mapped_endpoint();
            }
            else
              throw elle::Exception{"invalid mapping behavior"};
#endif
          }
          catch (elle::Exception const& e)
          {
            // Nat punching failed
            ELLE_TRACE("punch failed: %s", e.what());
          }

          auto* slug = new PortaledSlug(storage, passport, authority,
                                        protocol, members, port, timeout,
                                        std::move(socket));

          // Create the hole.
          std::unique_ptr<hole::Hole> hole(slug);

          std::string meta_host = _meta_host.empty() ? common::meta::host() :
                                                       _meta_host;
          uint16_t meta_port = (_meta_port == 0) ? common::meta::port() :
                                                   _meta_port;
          ELLE_TRACE_SCOPE("publish breached addresses to meta(%s,%s)",
                           meta_host, meta_port);
          {
            plasma::meta::Client client(meta_host, meta_port);
            try
            {
              std::vector<std::pair<std::string, uint16_t>> addresses;
              auto interfaces = elle::network::Interface::get_map(
                elle::network::Interface::Filter::only_up
                | elle::network::Interface::Filter::no_loopback
                | elle::network::Interface::Filter::no_autoip
                );
                for (auto const& pair: interfaces)
                  if (pair.second.ipv4_address.size() > 0 &&
                      pair.second.mac_address.size() > 0)
                  {
                    auto const &ipv4 = pair.second.ipv4_address;
                    addresses.emplace_back(ipv4, slug->port());
                  }
                ELLE_DEBUG("addresses: %s", addresses);
              std::vector<std::pair<std::string, uint16_t>> public_addresses;
              if (pub.port() != 0)
                public_addresses.push_back(std::pair<std::string, uint16_t>
                                           (pub.address().to_string(),
                                            pub.port()));
              client.token(token.empty() ? agent::Agent::meta_token : token);

              ELLE_DEBUG("public_addresses: %s", public_addresses);
              // client.connect_device(descriptor.meta().id(),
              //                       passport.id(),
              //                       addresses,
              //                       public_addresses);
            }
            catch (std::exception const& err)
            {
              ELLE_ERR("Cannot update device port: %s",
                       err.what()); // XXX[to improve]
            }
          }

          return hole;
        }
        case hole::Model::TypeCirkle:
        {
          /* XXX
          // allocate the instance.
          this->_implementation =
          new implementations::cirkle::Implementation(network);
          */
          ELLE_ABORT("Cirkle implementation is disabled for now");
          break;
        }
        default:
        {
          static boost::format fmt("unknown or not-yet-supported model '%u'");
          throw elle::Exception(str(fmt % descriptor.meta().model().type));
        }
      }

    elle::unreachable();
  }
}
