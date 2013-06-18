#include <satellites/progress/Progress.hh>
#include <satellites/satellite.hh>

#include <elle/utility/Parser.hh>
#include <elle/io/Piece.hh>
#include <elle/io/Path.hh>
#include <elle/system/system.hh>
#include <elle/log.hh>

#include <reactor/network/tcp-socket.hh>
#include <reactor/scheduler.hh>

#include <protocol/Serializer.hh>

#include <etoile/gear/Identifier.hh>
#include <etoile/path/Chemin.hh>
#include <etoile/path/Way.hh>
#include <etoile/portal/Manifest.hh>

#include <nucleus/neutron/Range.hh>
#include <nucleus/neutron/Record.hh>
#include <nucleus/neutron/Subject.hh>

#include <lune/Lune.hh>
#include <lune/Phrase.hh>

#include <agent/Agent.hh>
#include <common/common.hh>

#include <Program.hh>

#include <boost/foreach.hpp>
#include <limits>

ELLE_LOG_COMPONENT("infinit.satellites.progress.Progress");

namespace satellite
{
  reactor::network::TCPSocket* Progress::socket = nullptr;
  infinit::protocol::Serializer* Progress::serializer = nullptr;
  infinit::protocol::ChanneledStream* Progress::channels = nullptr;
  etoile::portal::RPC* Progress::rpcs = nullptr;

  /// Ward helper to make sure objects are discarded on errors.
  class Ward
  {
  public:
    Ward(etoile::gear::Identifier const& id):
      _id(id),
      _clean(true)
    {}

    ~Ward()
    {
      if (_clean && Progress::socket != nullptr)
        Progress::rpcs->objectdiscard(this->_id);
    }

    void release()
    {
      _clean = false;
    }

  private:
    etoile::gear::Identifier _id;
    bool _clean;
  };

  void
  Progress::connect()
  {
    // Load the phrase.
    lune::Phrase        phrase;
    phrase.load(Infinit::User, Infinit::Network, "portal");

    // Connect to the server.
    Progress::socket =
      new reactor::network::TCPSocket(*reactor::Scheduler::scheduler(),
                                      elle::String("127.0.0.1"),
                                      phrase.port);
    Progress::serializer =
      new infinit::protocol::Serializer(*reactor::Scheduler::scheduler(), *socket);
    Progress::channels =
      new infinit::protocol::ChanneledStream(*reactor::Scheduler::scheduler(),
                                             *serializer);
    Progress::rpcs = new etoile::portal::RPC(*channels);

    if (!Progress::rpcs->authenticate(phrase.pass))
      throw reactor::Exception("unable to authenticate to Etoile");
  }

  void
  Progress::display()
  {
    elle::Natural64 _size(0);
    elle::Natural64 _progress(0);

    // Connect to etoile.
    Progress::connect();

    // (1) Get the transfer size from the root directory.
    {
      // Resolve the path to the root directory.
      etoile::path::Chemin chemin(
        Progress::rpcs->pathresolve(
          etoile::path::Way(elle::system::path::separator)));

      // Load the root directory.
      etoile::gear::Identifier directory(
        Progress::rpcs->directoryload(chemin));

      Ward ward_directory(directory);

      // Then, retrieve the size of the transfer.
      nucleus::neutron::Trait size(
        Progress::rpcs->attributesget(directory,
                                      "infinit:transfer:size"));

      if (size == nucleus::neutron::Trait::null())
      {
        ELLE_DEBUG("no 'size' attribute present");

        throw std::runtime_error("no transfer size attribute present");
      }

      ELLE_DEBUG("'size' attribute retrieved: %s", size.value());

      // Set the size variable.
      _size = boost::lexical_cast<elle::Natural64>(size.value());

      // Discard the directory since unchanged.
      Progress::rpcs->directorydiscard(directory);

      ward_directory.release();
    }

    // (2) Get the progress attribute from the specific file.
    {
      // The way to the progress-specific file.
      elle::String root(elle::String(1, elle::system::path::separator) +
                        ".progress");
      etoile::path::Way way(root);

      etoile::path::Chemin* chemin(nullptr);

      try
        {
          // Resolve the file.
          chemin = new etoile::path::Chemin(Progress::rpcs->pathresolve(way));
        }
      catch (...)
        {
          ELLE_DEBUG("no '.progress' file present");

          std::cout << "0" << std::endl;
          return;
        }

      // Load the progress file.
      etoile::gear::Identifier identifier(Progress::rpcs->fileload(*chemin));

      delete chemin;

      Ward ward(identifier);

      // Retrieve the progress and size attributes.
      nucleus::neutron::Trait progress(
        Progress::rpcs->attributesget(identifier,
                                      "infinit:transfer:progress"));

      if (progress == nucleus::neutron::Trait::null())
        {
          ELLE_DEBUG("no 'progress' attribute retrieved");

          std::cout << "0" << std::endl;
          return;
        }

      ELLE_DEBUG("'progress' attribute retrieved: %s", progress.value());

      // Set the progress variable.
      _progress = boost::lexical_cast<elle::Natural64>(progress.value());

      // Discard the file.
      Progress::rpcs->filediscard(identifier);

      ward.release();
    }

    std::cout << _progress << " " << _size << std::endl;
  }

  void
  Progress(elle::Natural32 argc,
           elle::Character* argv[])
  {
    // XXX Infinit::Parser is not deleted in case of errors

    // set up the program.
    if (elle::concurrency::Program::Setup("Progress") == elle::Status::Error)
      throw elle::Exception("unable to set up the program");

    // initialize the Lune library.
    if (lune::Lune::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Lune");

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

    // parse.
    if (Infinit::Parser->Parse() == elle::Status::Error)
      throw elle::Exception("unable to parse the command line");

    // test the option.
    if (Infinit::Parser->Test("Help") == true)
      {
        // display the usage.
        Infinit::Parser->Usage();

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

    // initialize the Agent library.
    if (agent::Agent::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Agent");

    // display the progress.
    Progress::display();

    // delete the parser.
    delete Infinit::Parser;
    Infinit::Parser = nullptr;

    // clean the Agent library.
    if (agent::Agent::Clean() == elle::Status::Error)
      throw elle::Exception("unable to clean Agent");

    // clean Lune
    if (lune::Lune::Clean() == elle::Status::Error)
      throw elle::Exception("unable to clean Lune");
  }

}

//
// ---------- main ------------------------------------------------------------
//

int                     main(int                                argc,
                             char**                             argv)
{
  return satellite_main("8progress", [&] {
                          satellite::Progress(argc, argv);
                        });
}
