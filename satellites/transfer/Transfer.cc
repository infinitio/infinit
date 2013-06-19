#include <satellites/transfer/Transfer.hh>
#include <satellites/satellite.hh>

#include <elle/utility/Parser.hh>
#include <elle/io/Piece.hh>
#include <elle/io/Path.hh>
#include <elle/system/system.hh>

#include <reactor/scheduler.hh>
#include <reactor/network/tcp-socket.hh>

#include <protocol/Serializer.hh>

#include <etoile/gear/Identifier.hh>
#include <etoile/path/Chemin.hh>
#include <etoile/portal/Manifest.hh>

#include <nucleus/neutron/Range.hh>
#include <nucleus/neutron/Record.hh>
#include <nucleus/neutron/Subject.hh>

#include <lune/Lune.hh>
#include <lune/Phrase.hh>
#include <lune/Descriptor.hh>

#include <common/common.hh>

#include <agent/Agent.hh>

#include <elle/log.hh>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <limits>

#include <iostream>
#include <fstream>

#include <Program.hh>

ELLE_LOG_COMPONENT("infinit.satellites.transfer.Transfer");

namespace satellite
{
  reactor::network::TCPSocket* Transfer::socket = nullptr;
  infinit::protocol::Serializer* Transfer::serializer = nullptr;
  infinit::protocol::ChanneledStream* Transfer::channels = nullptr;
  etoile::portal::RPC* Transfer::rpcs = nullptr;
  lune::Descriptor* Transfer::descriptor = nullptr;

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
      if (_clean && Transfer::socket != nullptr)
        {
          try
            {
              Transfer::rpcs->objectdiscard(this->_id);
            }
          catch (...)
            {
              // Do nothing, we may already be throwing an exception.
            }
        }
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
  Transfer::connect()
  {
    ELLE_TRACE_FUNCTION("");

    // Load the phrase.
    lune::Phrase phrase;
    phrase.load(Infinit::User, Infinit::Network, "portal");

    // Connect to the server.
    Transfer::socket =
      new reactor::network::TCPSocket(*reactor::Scheduler::scheduler(),
                                      elle::String("127.0.0.1"),
                                      phrase.port);
    Transfer::serializer =
      new infinit::protocol::Serializer(*reactor::Scheduler::scheduler(),
                                        *socket);
    Transfer::channels =
      new infinit::protocol::ChanneledStream(*reactor::Scheduler::scheduler(),
                                             *serializer);
    Transfer::rpcs = new etoile::portal::RPC(*channels);

    if (!Transfer::rpcs->authenticate(phrase.pass))
      throw reactor::Exception("unable to authenticate to Etoile");
  }

  etoile::gear::Identifier
  Transfer::attach(etoile::gear::Identifier const& object,
                   elle::String const& path)
  {
    ELLE_TRACE_FUNCTION(object, path);

    boost::filesystem::path p(path);
    std::string way = p.parent_path().generic_string();
    etoile::path::Slab name = p.filename().generic_string();

    // Resolve parent directory.
    etoile::path::Chemin chemin(Transfer::rpcs->pathresolve(way));

    // Load parent directory.
    etoile::gear::Identifier directory(Transfer::rpcs->directoryload(chemin));

    Ward ward_directory(directory);

    // Retrieve the subject's permissions on the object.
    nucleus::neutron::Record record(
      Transfer::rpcs->accesslookup(directory, agent::Agent::Subject));

    // Check the record.
    if ((record == nucleus::neutron::Record::null()) ||
        ((record.permissions() & nucleus::neutron::permissions::write) !=
         nucleus::neutron::permissions::write))
      throw std::runtime_error("the subject does not have the permission");

    // Grant permissions for the user itself.
    // Set default permissions: read and write.
    nucleus::neutron::Permissions permissions =
      nucleus::neutron::permissions::read |
      nucleus::neutron::permissions::write;

    // Set the owner permissions.
    Transfer::rpcs->accessgrant(object, agent::Agent::Subject, permissions);

    // Grant read permission for 'everybody' group.
    switch (Transfer::descriptor->data().policy())
      {
      case horizon::Policy::accessible:
        {
          // grant the read permission to the 'everybody' group.
          Transfer::rpcs->accessgrant(
            object,
            Transfer::descriptor->meta().everybody_subject(),
            nucleus::neutron::permissions::read);

          break;
        }
      case horizon::Policy::editable:
        {
          // XXX
          assert(false && "not yet supported");

          break;
        }
      case horizon::Policy::confidential:
        {
          // Nothing else to do in this case, the file system object
          // remains private to its owner.

          break;
        }
      default:
        {
          throw std::runtime_error("invalid policy");
        }
      }

    // Add object to parent directory.
    Transfer::rpcs->directoryadd(directory, name, object);

    // Release the identifier tracking.
    ward_directory.release();

    return (directory);
  }

  static elle::Natural64 _progress(0);
  static elle::Natural64 _size(0);

  etoile::path::Chemin
  Transfer::from_setup()
  {
    ELLE_TRACE_FUNCTION("");

    // (1) Get the transfer size from the root directory.
    {
      // Resolve the path to the root directory.
      etoile::path::Chemin chemin(
        Transfer::rpcs->pathresolve(
          std::string(1, elle::system::path::separator)));

      // Load the root directory.
      etoile::gear::Identifier directory(
        Transfer::rpcs->directoryload(chemin));

      Ward ward_directory(directory);

      // Then, retrieve the size of the transfer.
      nucleus::neutron::Trait size(
        Transfer::rpcs->attributesget(directory,
                                      "infinit:transfer:size"));

      if (size == nucleus::neutron::Trait::null())
        throw std::runtime_error("no transfer size attribute present");

      // Set the size variable.
      _size = boost::lexical_cast<elle::Natural64>(size.value());
      ELLE_ASSERT_NEQ(_size, 0u);

      ELLE_DEBUG("the 'size' attribute is '%s'", _size);

      // Discard the directory since unchanged.
      Transfer::rpcs->directorydiscard(directory);

      ward_directory.release();
    }

    // The way to the progress-specific file. Note that this file does not
    // contain the progress in its data but in a specific attribute. This
    // has been done so as to speed up the process of updating the progress.
    //
    // Indeed, by setting an attribute, only the metadata of this file needs
    // to be retrieved while with the progress in the data, another block
    // would need to be retrieved.
    //
    // Note that the progress attribute could not be set in the root directory
    // because the root directory belongs to the user having transferred the
    // data. The user retrieving it has to create an object he owns so as to
    // set an attribute.
    elle::String root(elle::String(1, elle::system::path::separator) +
                      ".progress");

    // (2) Create the progress file.
    {
      // Create a file for the progress.
      etoile::gear::Identifier file(Transfer::rpcs->filecreate());

      Ward ward_file(file);

      // Attach the file to the hierarchy.
      etoile::gear::Identifier directory(Transfer::attach(file, root));

      Ward ward_directory(directory);

      // Set the initial attribute.
      Transfer::rpcs->attributesset(file,
                                    "infinit:transfer:progress",
                                    "0");

      // Set the progress to zero.
      _progress = 0;

      // Store the file.
      Transfer::rpcs->filestore(file);

      ward_file.release();

      // Store parent directory.
      Transfer::rpcs->directorystore(directory);

      ward_directory.release();
    }

    return (Transfer::rpcs->pathresolve(root));
  }

  void
  Transfer::from_progress(elle::Natural64 increment)
  {
    ELLE_TRACE_FUNCTION(increment);

    // The difference between the current progress and the last
    // one which has been pushed in the attributes. Once this
    // difference is reached, the attributes are updated.
    //
    // This is required so as to limit the number of updates while
    // ensuring a smooth progress.
    const elle::Real DIFFERENCE = 0.5;

    // Setup the progress update and keep the chemin which is
    // not going to change.
    static etoile::path::Chemin chemin(Transfer::from_setup());
    static elle::Natural64 stale(_progress);

    // Increment the progress counter.
    _progress += increment;

    // Compute the increment in terms of pourcentage of progress.
    ELLE_ASSERT_NEQ(_size, 0u);
    elle::Real difference = (_progress - stale) * 100 / _size;

    ELLE_DEBUG("difference %s", difference);

    // If the difference is large enough, update the progress in the root
    // directory's attribtues.
    if ((difference > DIFFERENCE) || (_progress == _size))
      {
        // Load the progress file.
        etoile::gear::Identifier identifier(
          Transfer::rpcs->fileload(chemin));

        Ward ward(identifier);

        elle::String string =
          boost::lexical_cast<elle::String>(_progress);

        // Update the progress attribute.
        Transfer::rpcs->attributesset(identifier,
                                      "infinit:transfer:progress",
                                      string);

        ELLE_DEBUG("update progress to '%s'", string);

        // Store the modifications.
        Transfer::rpcs->filestore(identifier);

        ward.release();

        // Update the stale progress which now is up-to-date.
        stale = _progress;
      }
  }

  void
  Transfer::from_traverse(std::string const& source,
                          elle::String const& target)
  {
    ELLE_TRACE_FUNCTION(source, target);

    // Before everything else, force the creation of the progress file.
    //
    // XXX[note that this call could be removed if etoile auto-publish
    //     blocks which have remained for quite some time main memory]
    Transfer::from_progress(0);

    // Resolve the directory.
    etoile::path::Chemin chemin(Transfer::rpcs->pathresolve(source));

    // Load the directory.
    etoile::gear::Identifier directory(Transfer::rpcs->directoryload(chemin));

    Ward ward_directory(directory);

    // Consult the directory.
    nucleus::neutron::Range<nucleus::neutron::Entry> entries(
      Transfer::rpcs->directoryconsult(
        directory,
        0, std::numeric_limits<nucleus::neutron::Index>::max()));

    // Go through the entries.
    for (auto entry: entries)
      {
        std::string _source(
          source + elle::system::path::separator + entry->name());

        ELLE_DEBUG("source %s", _source);

        // Resolve the child.
        etoile::path::Chemin chemin(Transfer::rpcs->pathresolve(_source));

        // Load the child.
        etoile::gear::Identifier child(Transfer::rpcs->objectload(chemin));

        Ward ward_child(child);

        // Retrieve information on the child.
        etoile::abstract::Object abstract(
          Transfer::rpcs->objectinformation(child));

        elle::String path(target + _source);

        switch (abstract.genre)
          {
          case nucleus::neutron::Genre::file:
            {
              // 1MB seems large enough for the performance to remain
              // good while ensuring a smooth progress i.e no jump from
              // 4% to 38% for reasonable large files.
              std::streamsize N = 1048576;
              nucleus::neutron::Offset offset(0);

              std::ofstream stream(path, std::ios::binary);

              ELLE_DEBUG("file %s", path.c_str());

              // Copy the file.
              while (offset < abstract.size)
                {
                  elle::Buffer data(
                    Transfer::rpcs->fileread(child, offset, N));

                  stream.write((const char*)data.contents(),
                               static_cast<std::streamsize>(data.size()));

                  offset += data.size();

                  // Set the progress.
                  Transfer::from_progress(data.size());
                }

              // Make sure the right amount has been copied.
              assert(offset == abstract.size);

              stream.close();

              // Discard the child.
              Transfer::rpcs->objectdiscard(child);

              ward_child.release();

              break;
            }
          case nucleus::neutron::Genre::directory:
            {
              ELLE_DEBUG("directory %s", path.c_str());

              // Create the directory.
              if (boost::filesystem::create_directory(path) == false)
                throw std::runtime_error("unable to create the directory");

              // Set the progress.
              Transfer::from_progress(1);

              // Recursively explore the Infinit network.
              Transfer::from_traverse(_source +
                                      elle::system::path::separator,
                                      target);

              // Discard the child.
              Transfer::rpcs->objectdiscard(child);

              ward_child.release();

              break;
            }
          case nucleus::neutron::Genre::link:
            {
              ELLE_DEBUG("link %s", path.c_str());

              // Resolve the link.
              std::string way(Transfer::rpcs->linkresolve(child));

              // Create the link.
              boost::filesystem::create_symlink(way, path);

              // Set the progress.
              Transfer::from_progress(way.length());

              // Discard the child.
              Transfer::rpcs->objectdiscard(child);

              ward_child.release();

              break;
            }
          }
      }

    // Discard the directory since no longer necessary.
    Transfer::rpcs->directorydiscard(directory);

    ward_directory.release();
  }

  void
  Transfer::from(elle::String const& target)
  {
    ELLE_TRACE_FUNCTION(target);

    // Connect to Etoile.
    Transfer::connect();

    // Traverse the Infinit network from the root.
    Transfer::from_traverse(std::string(1, elle::system::path::separator),
                            target);
  }

  void
  Transfer::to_update(elle::Natural64 const size)
  {
    ELLE_TRACE_FUNCTION(size);

    std::string root(1, elle::system::path::separator);

    // Resolve the root directory.
    etoile::path::Chemin chemin(Transfer::rpcs->pathresolve(root));

    // Load the directory.
    etoile::gear::Identifier directory(Transfer::rpcs->directoryload(chemin));

    Ward ward_directory(directory);

    // Set the attribute.
    Transfer::rpcs->attributesset(directory,
                                  "infinit:transfer:size", elle::sprint(size));

    // Store the directory.
    Transfer::rpcs->directorystore(directory);

    ward_directory.release();
  }

  elle::Natural64
  Transfer::to_create(elle::String const& source,
                      elle::String const& target)
  {
    ELLE_TRACE_FUNCTION(source, target);

    // Create file.
    etoile::gear::Identifier file(Transfer::rpcs->filecreate());

    Ward ward_file(file);

    // Attach the file to the hierarchy.
    etoile::gear::Identifier directory(Transfer::attach(file, target));

    Ward ward_directory(directory);

    nucleus::neutron::Offset offset(0);

    // Write the source file's content into the Infinit file freshly created.
    std::streamsize N = 5242880;
    std::ifstream stream(source, std::ios::binary);
    elle::Buffer buffer(N);

    while (stream.good())
      {
        buffer.size(N);

        stream.read((char*)buffer.mutable_contents(), buffer.size());

        buffer.size(stream.gcount());

        Transfer::rpcs->filewrite(file, offset, buffer);

        offset += buffer.size();
      }

    stream.close();

    // Store file.
    Transfer::rpcs->filestore(file);

    // Release the identifier tracking.
    ward_file.release();

    // Store parent directory.
    Transfer::rpcs->directorystore(directory);

    ward_directory.release();

    // Return the number of bytes composing the file having been copied.
    return (static_cast<elle::Natural64>(offset));
  }

  elle::Natural64
  Transfer::to_dig(elle::String const& path)
  {
    ELLE_TRACE_FUNCTION(path);

    // Create directory.
    etoile::gear::Identifier subdirectory(Transfer::rpcs->directorycreate());

    Ward ward_subdirectory(subdirectory);

    // Attach the directory to the hierarchy.
    etoile::gear::Identifier directory(Transfer::attach(subdirectory, path));

    Ward ward_directory(directory);

    // Store subdirectory.
    Transfer::rpcs->directorystore(subdirectory);

    // Release the identifier tracking.
    ward_subdirectory.release();

    // Store parent directory.
    Transfer::rpcs->directorystore(directory);

    ward_directory.release();

    // We consider that the directories do not account for the actual data but
    // for a single byte.
    return (1);
  }

  elle::Natural64
  Transfer::to_symlink(elle::String const& source,
                       elle::String const& target)
  {
    ELLE_TRACE_FUNCTION(source, target);

    // Create symlink.
    etoile::gear::Identifier link(Transfer::rpcs->linkcreate());

    Ward ward_link(link);

    // Attach the link to the hierarchy.
    etoile::gear::Identifier directory(Transfer::attach(link, target));

    Ward ward_directory(directory);

    std::string way(boost::filesystem::read_symlink(source).string());

    // bind the link.
    Transfer::rpcs->linkbind(link, way);

    // Store link.
    Transfer::rpcs->linkstore(link);

    // Release the identifier tracking.
    ward_link.release();

    // Store parent directory.
    Transfer::rpcs->directorystore(directory);

    ward_directory.release();

    return way.length();
  }

  void
  Transfer::to(elle::String const& source)
  {
    ELLE_TRACE_FUNCTION(source);

    elle::Natural64 size(0);

    // Connect to Etoile.
    Transfer::connect();

    boost::filesystem::path path(source);

    if (boost::filesystem::is_symlink(path) == true)
      {
        // Transfor a single link.
        elle::String root(path.parent_path().string());
        elle::String base(path.string().substr(root.length()));

        ELLE_DEBUG("root %s", root.c_str());
        ELLE_DEBUG("link %s", base.c_str());

        size += Transfer::to_symlink(source, base);
      }
    else if (boost::filesystem::is_directory(path) == true)
      {
        // Transfer a whole directory and its content.
        elle::String root(path.parent_path().string());
        elle::String base(path.string().substr(root.length()));

        ELLE_DEBUG("root %s", root.c_str());
        ELLE_DEBUG("base %s", base.c_str());

        boost::filesystem::recursive_directory_iterator iterator(source);
        boost::filesystem::recursive_directory_iterator end;

        size += Transfer::to_dig(base);

        for (; iterator != end; ++iterator)
          {
            ELLE_DEBUG("path %s", iterator->path().string().c_str());

            if (boost::filesystem::is_symlink(iterator->path()) == true)
              {
                elle::String link(
                  iterator->path().string().substr(root.length()));

                ELLE_DEBUG("link %s", link.c_str());

                size += Transfer::to_symlink(iterator->path().string(), link);
              }
            else if (boost::filesystem::is_regular_file(
                       iterator->path()) == true)
              {
                elle::String file(
                  iterator->path().string().substr(root.length()));

                ELLE_DEBUG("file %s", file.c_str());

                size += Transfer::to_create(iterator->path().string(), file);
              }
            else if (boost::filesystem::is_directory(iterator->path()) == true)
              {
                elle::String directory(
                  iterator->path().string().substr(root.length()));

                ELLE_DEBUG("directory %s", directory.c_str());

                size += Transfer::to_dig(directory);
              }
            else
              throw std::runtime_error("unknown object type");
          }
      }
    else if (boost::filesystem::is_regular_file(path) == true)
      {
        // Transfor a single file.
        elle::String root(path.parent_path().string());
        elle::String base(path.string().substr(root.length()));

        ELLE_DEBUG("root %s", root.c_str());
        ELLE_DEBUG("file %s", base.c_str());

        size += Transfer::to_create(source, base);
      }
    else
      throw std::runtime_error("unknown object type");

    Transfer::to_update(size);
  }

  void
  Transfer(elle::Natural32 argc,
       elle::Character* argv[])
  {
    Transfer::Operation operation;

    // XXX Infinit::Parser is not deleted in case of errors

    // set up the program.
    if (elle::concurrency::Program::Setup("Transfer") == elle::Status::Error)
      throw elle::Exception("unable to set up the program");

    // initialize the Lune library.
    if (lune::Lune::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Lune");

    // initialize Infinit.
    if (Infinit::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Infinit");

    // initialize the operation.
    operation = Transfer::OperationUnknown;

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

    // register the options.
    if (Infinit::Parser->Register(
          "From",
          'f',
          "from",
          "specifies that the file is copied from the Infinit network to "
          "the local file system",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "To",
          't',
          "to",
          "specifies that the file is copied from the local file system to "
          "the Infinit network",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Path",
          'p',
          "path",
          "the path where the data must be copied from/to",
          elle::utility::Parser::KindRequired) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    if (Infinit::Parser->Example(
          "-u fistouille -n slug --to --path ~/Downloads/") ==
        elle::Status::Error)
      throw elle::Exception("unable to register the example");

    if (Infinit::Parser->Example(
          "-u fistouille -n slug --from --path /tmp/XXX/") ==
        elle::Status::Error)
      throw elle::Exception("unable to register the example");

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

    // initialize the Agent library.
    if (agent::Agent::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Agent");

    // check the mutually exclusive options.
    if ((Infinit::Parser->Test("From") == true) &&
        (Infinit::Parser->Test("To") == true))
      {
        // display the usage.
        Infinit::Parser->Usage();

        throw elle::Exception("the from and to options are mutually exclusive");
      }

    // test the option.
    if (Infinit::Parser->Test("From") == true)
      operation = Transfer::OperationFrom;

    // test the option.
    if (Infinit::Parser->Test("To") == true)
      operation = Transfer::OperationTo;

    // FIXME: do not re-parse the descriptor every time.
    Transfer::descriptor =
      new lune::Descriptor(Infinit::User, Infinit::Network);

    elle::String path;

    // retrieve the path.
    if (Infinit::Parser->Value("Path",
                               path) == elle::Status::Error)
      throw elle::Exception("unable to retrieve the path value");

    path =
      boost::algorithm::trim_right_copy_if(
        boost::filesystem::absolute(path).string(),
        boost::is_any_of("/"));

    // trigger the operation.
    switch (operation)
      {
      case Transfer::OperationFrom:
        {
          Transfer::from(path);

          break;
        }
      case Transfer::OperationTo:
        {
          Transfer::to(path);

          break;
        }
      case Transfer::OperationUnknown:
      default:
        {
          // display the usage.
          Infinit::Parser->Usage();

          throw elle::Exception("please specify an operation to perform");
        }
      }

    // delete the parser.
    delete Infinit::Parser;
    Infinit::Parser = nullptr;

    // clean the Agent library.
    if (agent::Agent::Clean() == elle::Status::Error)
      throw elle::Exception("unable to clean Agent");

    // clean Infinit.
    if (Infinit::Clean() == elle::Status::Error)
      throw elle::Exception("unable to clean Infinit");

    // clean Lune
    if (lune::Lune::Clean() == elle::Status::Error)
      throw elle::Exception("unable to clean Lune");
  }

}

int                     main(int                                argc,
                             char**                             argv)
{
  return satellite_main("8transfer", [&] {
                          satellite::Transfer(argc, argv);
                        });
}
