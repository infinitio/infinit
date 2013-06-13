#include <boost/program_options.hpp>

#include <common/common.hh>

#include <elle/cast.hh>
#include <elle/io/Piece.hh>
#include <elle/log.hh>
#include <elle/log/TextLogger.hh>
#include <elle/network/Interface.hh>
#include <elle/network/Locus.hh>
#include <elle/serialize/extract.hh>
#include <elle/utility/Parser.hh>

#include <agent/Agent.hh>

#include <satellites/satellite.hh>

#include <etoile/Etoile.hh>
#include <etoile/depot/Depot.hh>

#include <hole/Hole.hh>
#include <hole/storage/Directory.hh>
#ifdef INFINIT_HORIZON
# include <horizon/Horizon.hh>
#endif

#include <lune/Descriptor.hh>
#include <lune/Lune.hh>

#include <plasma/meta/Client.hh>

#include <CrashReporter.hh>
#include <HoleFactory.hh>
#include <Infinit.hh>
#include <Program.hh>
#include <version.hh>

ELLE_LOG_COMPONENT("infinit");

boost::program_options::variables_map
parse_options(int argc, char** argv)
{
  using namespace boost::program_options;
  options_description options("Allowed options");
  options.add_options()
    ("help,h", "display the help")
    ("user,u", value<std::string>(), "specify the name of the user")
    ("network,n", value<std::string>(), "specify the name of the network")
    ("mountpoint,m", value<std::string>(), "specify the mount point")
    ("peer", value<std::vector<std::string>>(),
     "specify the peers to connect to");

  variables_map vm;
  store(parse_command_line(argc, argv, options), vm);
  notify(vm);

  if (vm.count("help"))
  {
    std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << options;
    std::cout << std::endl;
    std::cout << "Infinit " INFINIT_VERSION
      " Copyright (c) 2013 infinit.io All rights reserved." << std::endl;
    throw infinit::Exit(0);
  }

  return vm;
}

void
Infinit(elle::Natural32 argc, elle::Character* argv[])
{
  // set up the program.
  if (elle::concurrency::Program::Setup("Infinit") == elle::Status::Error)
    throw elle::Exception("unable to set up the program");

  auto options = parse_options(argc, argv);

  if (options.count("user"))
    Infinit::User = options["user"].as<std::string>();
  else
    throw elle::Exception("missing command line option: user");

  if (options.count("network"))
    Infinit::Network = options["network"].as<std::string>();
  else
    throw elle::Exception("missing command line option: network");

  if (options.count("mountpoint"))
    Infinit::Mountpoint = options["mountpoint"].as<std::string>();
  else
  {
    // Nothing to do, keep the mounpoint empty.
    // XXX[to fix later though]
  }

  std::vector<elle::network::Locus> members;
  if (options.count("peer"))
    for (auto const& peer: options["peer"].as<std::vector<std::string>>())
      members.push_back(elle::network::Locus(peer));

  // initialize the Lune library.
  if (lune::Lune::Initialize() == elle::Status::Error)
    throw elle::Exception("unable to initialize Lune");

  // initialize Infinit.
  if (Infinit::Initialize() == elle::Status::Error)
    throw elle::Exception("unable to initialize Infinit");

  // initialize the Agent library.
  if (agent::Agent::Initialize() == elle::Status::Error)
    throw elle::Exception("unable to initialize Agent");

  // // Create the NAT Manipulation class
  // elle::nat::NAT NAT(elle::concurrency::scheduler());
  // std::vector<std::pair<std::string, uint16_t>> public_addresses;


  // // By default, try to open a hole in the nat.
  // try
  //   {
  //     ELLE_DEBUG_SCOPE("start hole punching on %s:%d",
  //                      common::longinus::host(),
  //                      common::longinus::port());
  //     elle::nat::Hole pokey = NAT.punch(common::longinus::host(),
  //                                        common::longinus::port());

  //     public_addresses.push_back(pokey.public_endpoint());
  //   }
  // catch (elle::Exception &e)
  //   {
  //     ELLE_WARN("NAT punching error: %s", e.what());
  //   }

  nucleus::proton::Network network(Infinit::Network);

  hole::storage::Directory storage{
    network,
    common::infinit::network_shelter(Infinit::User, Infinit::Network)
  };

  ELLE_DEBUG("loading passport");
  elle::Passport passport{
    elle::serialize::from_file(common::infinit::passport_path(Infinit::User))
  };

  ELLE_DEBUG("constructing hole");
  std::unique_ptr<hole::Hole> hole(
    infinit::hole_factory(storage, passport, Infinit::authority(), members));
  etoile::depot::hole(hole.get());
  ELLE_DEBUG("hole constructed");
#ifdef INFINIT_HORIZON
  ELLE_DEBUG("INFINIT_HORIZON enable");
  horizon::hole(hole.get());
#endif

  // initialize the Etoile library.
  if (etoile::Etoile::Initialize() == elle::Status::Error)
    throw elle::Exception("unable to initialize Etoile");

  // initialize the horizon.
  if (!Infinit::Mountpoint.empty())
#ifdef INFINIT_HORIZON
    horizon::Horizon::Initialize(*reactor::Scheduler::scheduler());
#else
  throw elle::Exception("horizon was disabled at compilation time "
                           "but a mountpoint was given on the command line");
#endif

  // launch the program.
  elle::concurrency::Program::Launch();

  // delete the parser.
  delete Infinit::Parser;
  Infinit::Parser = nullptr;

#ifdef INFINIT_HORIZON
  // clean the horizon.
  if (!Infinit::Mountpoint.empty())
    if (horizon::Horizon::Clean() == elle::Status::Error)
      throw elle::Exception("unable to clean the horizon");
#endif

  // clean the Etoile library.
  if (etoile::Etoile::Clean() == elle::Status::Error)
    throw elle::Exception("unable to clean Etoile");

  // clean the Agent library.
  if (agent::Agent::Clean() == elle::Status::Error)
    throw elle::Exception("unable to clean Agent");

  hole.reset(nullptr);

  // clean Infinit.
  if (Infinit::Clean() == elle::Status::Error)
    throw elle::Exception("unable to clean Infinit");

  // clean Lune
  if (lune::Lune::Clean() == elle::Status::Error)
    throw elle::Exception("unable to clean Lune");
}

int
main(int argc, char* argv[])
{
  auto _main = [&]
  {
    Infinit(argc, argv);
  };
  return infinit::satellite_main("8infinit", std::move(_main));
}
