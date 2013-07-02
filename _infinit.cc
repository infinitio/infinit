#include <boost/program_options.hpp>

#include <common/common.hh>

#include <elle/cast.hh>
#include <elle/io/Piece.hh>
#include <elle/log.hh>
#include <elle/log/TextLogger.hh>
#include <elle/network/Interface.hh>
#include <elle/network/Locus.hh>
#include <elle/serialize/PairSerializer.hxx>
#include <elle/serialize/extract.hh>
#include <elle/utility/Parser.hh>

#include <agent/Agent.hh>

#include <satellites/satellite.hh>

#include <etoile/Etoile.hh>
#include <etoile/Manifest.hh>
#include <etoile/depot/Depot.hh>

#include <etoile/abstract/Group.hh>
#include <etoile/wall/Access.hh>
#include <etoile/wall/Attributes.hh>
#include <etoile/wall/Directory.hh>
#include <etoile/wall/File.hh>
#include <etoile/wall/Group.hh>
#include <etoile/wall/Link.hh>
#include <etoile/wall/Object.hh>
#include <etoile/wall/Path.hh>

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
#include <Portal.hh>
#include <Program.hh>
#include <version.hh>

ELLE_LOG_COMPONENT("infinit");

static
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
  try
  {
    store(parse_command_line(argc, argv, options), vm);
    notify(vm);
  }
  catch (invalid_command_line_syntax const& e)
  {
    throw elle::Exception(elle::sprintf("command line error: %s", e.what()));
  }

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

static
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

  lune::Descriptor descriptor(Infinit::User, Infinit::Network);

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
  ELLE_DEBUG("hole constructed");
#ifdef INFINIT_HORIZON
  ELLE_DEBUG("INFINIT_HORIZON enable");
  horizon::hole(hole.get());
#endif

  std::unique_ptr<etoile::Etoile> etoile(
    new etoile::Etoile(agent::Agent::Identity.pair(),
                       hole.get(),
                       descriptor.meta().root()));
  infinit::Portal<etoile::RPC> etoile_portal(
    "portal", [&](etoile::RPC& rpcs)
    {
      using std::placeholders::_1;
      using std::placeholders::_2;
      using std::placeholders::_3;
      using std::placeholders::_4;
      rpcs.accessconsult = std::bind(&etoile::wall::Access::consult,
                                     std::ref(*etoile.get()), _1, _2, _3);
      rpcs.accessgrant = std::bind(&etoile::wall::Access::Grant,
                                   std::ref(*etoile.get()), _1, _2, _3);
      rpcs.accesslookup = std::bind(&etoile::wall::Access::lookup,
                                    std::ref(*etoile.get()), _1, _2);
      rpcs.accessrevoke = std::bind(&etoile::wall::Access::Revoke,
                                    std::ref(*etoile.get()), _1, _2);
      rpcs.groupadd = std::bind(&etoile::wall::Group::Add,
                                std::ref(*etoile.get()), _1, _2);
      rpcs.groupconsult = std::bind(&etoile::wall::Group::Consult,
                                    std::ref(*etoile.get()), _1, _2, _3);
      rpcs.groupcreate = std::bind(&etoile::wall::Group::Create,
                                   std::ref(*etoile.get()), _1);
      rpcs.groupdestroy = std::bind(&etoile::wall::Group::Destroy,
                                    std::ref(*etoile.get()), _1);
      rpcs.groupdiscard = std::bind(&etoile::wall::Group::Discard,
                                    std::ref(*etoile.get()), _1);
      rpcs.groupinformation = std::bind(&etoile::wall::Group::Information,
                                        std::ref(*etoile.get()), _1);
      rpcs.groupload = std::bind(&etoile::wall::Group::Load,
                                 std::ref(*etoile.get()), _1);
      rpcs.grouplookup = std::bind(&etoile::wall::Group::Lookup,
                                   std::ref(*etoile.get()), _1, _2);
      rpcs.groupremove = std::bind(&etoile::wall::Group::Remove,
                                   std::ref(*etoile.get()), _1, _2);
      rpcs.groupstore = std::bind(&etoile::wall::Group::Store,
                                  std::ref(*etoile.get()), _1);
      rpcs.objectload = std::bind(&etoile::wall::Object::load,
                                  std::ref(*etoile.get()), _1);
      rpcs.objectinformation = std::bind(&etoile::wall::Object::information,
                                         std::ref(*etoile.get()), _1);
      rpcs.objectdiscard = std::bind(&etoile::wall::Object::discard,
                                     std::ref(*etoile.get()), _1);
      rpcs.objectstore = std::bind(&etoile::wall::Object::store,
                                   std::ref(*etoile.get()), _1);
      rpcs.fileload = std::bind(&etoile::wall::File::load,
                                std::ref(*etoile.get()), _1);
      rpcs.filecreate = std::bind(&etoile::wall::File::create,
                                  std::ref(*etoile.get()));
      rpcs.fileread = std::bind(&etoile::wall::File::read,
                                std::ref(*etoile.get()), _1, _2, _3);
      rpcs.filewrite = std::bind(&etoile::wall::File::write,
                                 std::ref(*etoile.get()), _1, _2, _3);
      rpcs.filediscard = std::bind(&etoile::wall::File::discard,
                                   std::ref(*etoile.get()), _1);
      rpcs.filestore = std::bind(&etoile::wall::File::store,
                                 std::ref(*etoile.get()), _1);
      rpcs.linkcreate = std::bind(&etoile::wall::Link::create,
                                  std::ref(*etoile.get()));
      rpcs.linkbind = std::bind(&etoile::wall::Link::bind,
                                std::ref(*etoile.get()), _1, _2);
      rpcs.linkresolve = std::bind(&etoile::wall::Link::resolve,
                                   std::ref(*etoile.get()), _1);
      rpcs.linkstore = std::bind(&etoile::wall::Link::store,
                                 std::ref(*etoile.get()), _1);
      rpcs.directorycreate = std::bind(&etoile::wall::Directory::create,
                                       std::ref(*etoile.get()));
      rpcs.directoryload = std::bind(&etoile::wall::Directory::load,
                                     std::ref(*etoile.get()), _1);
      rpcs.directoryadd = std::bind(&etoile::wall::Directory::add,
                                    std::ref(*etoile.get()), _1, _2, _3);
      rpcs.directoryconsult = std::bind(&etoile::wall::Directory::consult,
                                        std::ref(*etoile.get()), _1, _2, _3);
      rpcs.directorydiscard = std::bind(&etoile::wall::Directory::discard,
                                        std::ref(*etoile.get()), _1);
      rpcs.directorystore = std::bind(&etoile::wall::Directory::store,
                                      std::ref(*etoile.get()), _1);
      rpcs.pathresolve = std::bind(&etoile::wall::Path::resolve,
                                   std::ref(*etoile.get()), _1);
      rpcs.attributesset = std::bind(&etoile::wall::Attributes::set,
                                     std::ref(*etoile.get()), _1, _2, _3);
      rpcs.attributesget = std::bind(&etoile::wall::Attributes::get,
                                     std::ref(*etoile.get()), _1, _2);
      rpcs.attributesfetch = std::bind(&etoile::wall::Attributes::fetch,
                                       std::ref(*etoile.get()), _1);
    });

#ifdef INFINIT_HORIZON
  horizon::etoile(etoile.get());
#endif

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

  etoile.reset();

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
