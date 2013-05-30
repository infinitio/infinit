#include <memory>

#include <elle/network/Interface.hh>

#include <reactor/network/nat.hh>
#include <reactor/network/resolve.hh>

#include <agent/Agent.hh>

#include <common/common.hh>

#include <lune/Descriptor.hh>
#include <lune/Set.hh>

#include <hole/implementations/local/Implementation.hh>
#include <hole/implementations/remote/Implementation.hh>
#include <hole/implementations/slug/Manifest.hh>
#include <hole/implementations/slug/Slug.hh>
#include <hole/Authority.hh>
#include <hole/Passport.hh>

#include <plasma/meta/Client.hh>

#include <Heartbeat.hh>
#include <HoleFactory.hh>
#include <Infinit.hh>
#include <Portal.hh>

namespace infinit
{
  class PortaledSlug:
    public hole::implementations::slug::Slug
  {
  public:
    typedef hole::implementations::slug::Slug Super;
    PortaledSlug(hole::storage::Storage& storage,
                 elle::Passport const& passport,
                 elle::Authority const& authority,
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
                                        std::placeholders::_2);
          rpcs.slug_wait = std::bind(&Slug::portal_wait,
                                     this,
                                     std::placeholders::_1,
                                     std::placeholders::_2);
        }
        )
    {}


    Portal<hole::implementations::slug::control::RPC> _portal;
  };

  std::unique_ptr<hole::Hole>
  hole_factory(hole::storage::Storage& storage,
               elle::Passport const& passport,
               elle::Authority const& authority)
  {
    lune::Descriptor descriptor(Infinit::User, Infinit::Network);

    lune::Set set;
    if (lune::Set::exists(Infinit::User, Infinit::Network) == true)
      set.load(Infinit::User, Infinit::Network);

    switch (descriptor.meta().model().type)
      {
        case hole::Model::TypeLocal:
        {
          return std::unique_ptr<hole::Hole>(
            new hole::implementations::local::Implementation(
              storage, passport, authority));
          break;
        }
        case hole::Model::TypeRemote:
        {
          // Retrieve the locus.
          if (set.loci.size() != 1)
            {
              static boost::format fmt("there should be a single locus "
                                       "in the network's set (%u)");
              throw std::runtime_error(str(fmt % set.loci.size()));
            }
          elle::network::Locus locus = *set.loci.begin();
          return std::unique_ptr<hole::Hole>(
            new hole::implementations::remote::Implementation(
              storage, passport, authority, locus));
        }
        case hole::Model::TypeSlug:
        {
          std::vector<elle::network::Locus> members;
          // FIXME: Restore sets at some point. Maybe.
          // for (elle::network::Locus const& locus: set.loci)
          //   members.push_back(locus);
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
            reactor::nat::NAT nat(sched);

            auto host = reactor::network::resolve_udp(sched,
                                                      common::longinus::host(),
                                                      std::to_string(3478));
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
          }
          catch (elle::Exception const& e)
          {
            // Nat punching failed
            ELLE_TRACE("punch failed: %s", e.what());
          }

          // If the punch succeed, we start the heartbite thread.
          if (socket)
            start_heartbeat(*socket);

          auto* slug = new PortaledSlug(storage, passport, authority,
                                        protocol, members, port, timeout,
                                        std::move(socket));

          // Create the hole.
          std::unique_ptr<hole::Hole> hole(slug);

          ELLE_TRACE("send addresses to meta")
          {
            lune::Descriptor descriptor(Infinit::User, Infinit::Network);
            plasma::meta::Client client(common::meta::host(), common::meta::port());
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
              client.token(agent::Agent::meta_token);

              ELLE_DEBUG("public_addresses: %s", public_addresses);
              client.network_connect_device(descriptor.meta().id(),
                                            passport.id(),
                                            addresses,
                                            public_addresses);
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
          elle::abort("Cirkle implementation is disabled for now");
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
