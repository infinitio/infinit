#include <common/common.hh>

#include <elle/cast.hh>
#include <elle/io/Piece.hh>
#include <elle/log.hh>
#include <elle/log/TextLogger.hh>
#include <elle/network/Interface.hh>
#include <elle/serialize/extract.hh>
#include <elle/utility/Parser.hh>

#include <agent/Agent.hh>

#include <satellites/satellite.hh>

#include <etoile/Etoile.hh>
#include <etoile/depot/Depot.hh>

#include <hole/Hole.hh>
#include <hole/storage/Directory.hh>
#include <horizon/Horizon.hh>

#include <lune/Descriptor.hh>
#include <lune/Lune.hh>

#include <plasma/meta/Client.hh>

#include <CrashReporter.hh>
#include <HoleFactory.hh>
#include <Infinit.hh>
#include <Program.hh>

ELLE_LOG_COMPONENT("infinit");

void
Infinit(elle::Natural32 argc, elle::Character* argv[])
{
  // set up the program.
  if (elle::concurrency::Program::Setup("Infinit") == elle::Status::Error)
    throw elle::Exception("unable to set up the program");

  // allocate a new parser.
  Infinit::Parser = new elle::utility::Parser(argc, argv);

  // specify a program description.
  if (Infinit::Parser->Description(Infinit::Copyright) == elle::Status::Error)
    throw elle::Exception("unable to set the description");

  // register the options.
  if (Infinit::Parser->Register(
        "Help",
        'h',
        "help",
        "display the help",
        elle::utility::Parser::KindNone) == elle::Status::Error)
    throw elle::Exception("unable to register the option");

  // register the option.
  if (Infinit::Parser->Register(
        "User",
        'u',
        "user",
        "specifies the name of the user",
        elle::utility::Parser::KindRequired) == elle::Status::Error)
    throw elle::Exception("unable to register the option");

  // register the option.
  if (Infinit::Parser->Register(
        "Network",
        'n',
        "network",
        "specifies the name of the network",
        elle::utility::Parser::KindRequired) == elle::Status::Error)
    throw elle::Exception("unable to register the option");

  // register the option.
  if (Infinit::Parser->Register(
        "Mountpoint",
        'm',
        "mountpoint",
        "specifies the mount point",
        elle::utility::Parser::KindRequired) == elle::Status::Error)
    throw elle::Exception("unable to register the option");

  // parse.
  if (Infinit::Parser->Parse() == elle::Status::Error)
    throw elle::Exception("unable to parse the command line");

  // test the option.
  if (Infinit::Parser->Test("Help") == true)
    {
      // display the usage.
      Infinit::Parser->Usage();

      // quit.
      return;
    }

  // retrieve the user name.
  if (Infinit::Parser->Value("User",
                             Infinit::User) == elle::Status::Error)
    {
      // display the usage.
      Infinit::Parser->Usage();

      throw elle::Exception("unable to retrieve the user name");
    }

  // retrieve the network name.
  if (Infinit::Parser->Value("Network",
                             Infinit::Network) == elle::Status::Error)
    {
      // display the usage.
      Infinit::Parser->Usage();

      throw elle::Exception("unable to retrieve the network name");
    }

  // Retrieve the mount point.
  if (Infinit::Parser->Test("Mountpoint") == true)
    {
      if (Infinit::Parser->Value("Mountpoint",
                                 Infinit::Mountpoint) == elle::Status::Error)
        throw elle::Exception("unable to retrieve the mountpoint");
    }
  else
    {
      // Nothing to do, keep the mounpoint empty.
      // XXX[to fix later though]
    }

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
    infinit::hole_factory(storage, passport, Infinit::authority()));
  etoile::depot::hole(hole.get());
  ELLE_DEBUG("hole constructed");
#ifdef INFINIT_HORIZON
  ELLE_DEBUG("INFINIT_HORIZON enable");
  horizon::hole(hole.get());
#endif
  ELLE_DEBUG("joining hole");
  hole->join();

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

  hole->leave();
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
