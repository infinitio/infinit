#include <satellites/access/Access.hh>

#include <elle/utility/Parser.hh>
#include <elle/io/Piece.hh>
#include <elle/io/Path.hh>

#include <cryptography/PublicKey.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

#include <reactor/network/tcp-socket.hh>

#include <protocol/Serializer.hh>

#include <etoile/gear/Identifier.hh>
#include <etoile/path/Chemin.hh>

#include <nucleus/neutron/Range.hh>
#include <nucleus/neutron/Record.hh>
#include <nucleus/neutron/Subject.hh>

#include <common/common.hh>

#include <lune/Lune.hh>
#include <lune/Phrase.hh>

#include <satellites/satellite.hh>

#include <reactor/scheduler.hh>

#include <limits>

#include <Program.hh>

namespace satellite
{
  reactor::network::TCPSocket* Access::socket = nullptr;
  infinit::protocol::Serializer* Access::serializer = nullptr;
  infinit::protocol::ChanneledStream* Access::channels = nullptr;
  etoile::RPC* Access::rpcs = nullptr;

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
      if (_clean && Access::socket != nullptr)
        Access::rpcs->objectdiscard(this->_id);
    }

    void release()
    {
      _clean = false;
    }

  private:
    etoile::gear::Identifier _id;
    bool _clean;
  };

  static
  void
  display(nucleus::neutron::Record const& record)
  {
    switch (record.subject().type())
      {
        case nucleus::neutron::Subject::TypeUser:
        {
          elle::io::Unique unique;
          // Convert the public key into a human-kind-of-readable string.
          if (record.subject().user().Save(unique) == elle::Status::Error)
            throw reactor::Exception("unable to save the public key's unique");
          std::cout << "User"
                    << " "
                    << unique
                    << " "
                    << std::dec
                    << static_cast<elle::Natural32>(record.permissions())
                    << std::endl;
          break;
        }
        case nucleus::neutron::Subject::TypeGroup:
        {
          elle::io::Unique unique;
          // Convert the group's address into a human-kind-of-readable string.
          if (record.subject().group().Save(unique) == elle::Status::Error)
            throw reactor::Exception("unable to save the address' unique");

          std::cout << "Group"
                    << " "
                    << unique
                    << " "
                    << std::dec << static_cast<elle::Natural32>(record.permissions()) << std::endl;

          break;
        }
        default:
        {
          break;
        }
      }
  }

  void
  Access::connect()
  {
    // Load the phrase.
    lune::Phrase        phrase;
    phrase.load(Infinit::User, Infinit::Network, "portal");

    // Connect to the server.
    Access::socket =
      new reactor::network::TCPSocket(*reactor::Scheduler::scheduler(),
                                      elle::String("127.0.0.1"),
                                      phrase.port);
    Access::serializer =
      new infinit::protocol::Serializer(*reactor::Scheduler::scheduler(), *socket);
    Access::channels =
      new infinit::protocol::ChanneledStream(*reactor::Scheduler::scheduler(),
                                             *serializer);
    Access::rpcs = new etoile::RPC(*channels);

    // if (!Access::rpcs->authenticate(phrase.pass))
    //   throw reactor::Exception("unable to authenticate to Etoile");
  }

  void
  Access::lookup(const std::string& path,
                 const nucleus::neutron::Subject& subject)
  {
    Access::connect();
    // Resolve the path.
    etoile::path::Chemin chemin(Access::rpcs->pathresolve(path));
    // Load the object.
    etoile::gear::Identifier identifier(Access::rpcs->objectload(chemin));
    Ward ward(identifier);
    // Lookup the access record.
    nucleus::neutron::Record record(
      Access::rpcs->accesslookup(identifier, subject));
    display(record);
  }

  void
  Access::consult(const std::string& path)
  {
    Access::connect();
    // Resolve the path.
    etoile::path::Chemin chemin(Access::rpcs->pathresolve(path));
    // Load the object.
    etoile::gear::Identifier identifier(Access::rpcs->objectload(chemin));
    Ward ward(identifier);
    // Consult the object access.
    nucleus::neutron::Range<nucleus::neutron::Record> range(
      Access::rpcs->accessconsult(
        identifier,
        std::numeric_limits<nucleus::neutron::Index>::min(),
        std::numeric_limits<nucleus::neutron::Size>::max()));
    // Dump the records.
    for (auto& record: range)
      display(*record);
  }

  void
  Access::grant(const std::string&  path,
                const nucleus::neutron::Subject&   subject,
                const nucleus::neutron::Permissions permissions)
  {
    Access::connect();
    // Resolve the path.
    etoile::path::Chemin chemin(Access::rpcs->pathresolve(path));
    // Load the object.
    etoile::gear::Identifier identifier(Access::rpcs->objectload(chemin));
    Ward ward(identifier);
    // Lookup the access record.
    Access::rpcs->accessgrant(identifier, subject, permissions);
    // store the object.
    Access::rpcs->objectstore(identifier);
    ward.release();
  }

  void
  Access::revoke(const std::string& path,
                 const nucleus::neutron::Subject&  subject)
  {
    Access::connect();
    // Resolve the path.
    etoile::path::Chemin chemin(Access::rpcs->pathresolve(path));
    // Load the object.
    etoile::gear::Identifier identifier(Access::rpcs->objectload(chemin));
    Ward ward(identifier);
    // Revoke the access for the given subject.
    Access::rpcs->accessrevoke(identifier, subject);
    // store the object.
    Access::rpcs->objectstore(identifier);
    ward.release();
  }

  static
  elle::Status
  Access(elle::Natural32 argc,
         elle::Character* argv[])
  {
    Access::Operation   operation;

    // XXX Infinit::Parser is not deleted in case of errors

    // set up the program.
    if (elle::concurrency::Program::Setup("Access") == elle::Status::Error)
      throw elle::Exception("unable to set up the program");

    // initialize the Lune library.
    if (lune::Lune::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Lune");

    // initialize Infinit.
    if (Infinit::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Infinit");

    // initialize the operation.
    operation = Access::OperationUnknown;

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
          "Lookup",
          'l',
          "lookup",
          "look up a specific access record",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Consult",
          'c',
          "consult",
          "consult the access records",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Grant",
          'g',
          "grant",
          "grant access to a user/group",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Revoke",
          'r',
          "revoke",
          "revoke an access for a user/group",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Type",
          't',
          "type",
          "indicate the type of the entity: user or group",
          elle::utility::Parser::KindRequired) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Path",
          'p',
          "path",
          "indicate the local absolute path to the target object "
          "i.e file, directory or link",
          elle::utility::Parser::KindRequired) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Identity",
          'i',
          "identity",
          "specify the user/group base64 identity",
          elle::utility::Parser::KindRequired) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Read",
          'R',
          "read",
          "indicate that the read permission must be granted",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Write",
          'W',
          "write",
          "indicate that the write permission must be granted",
          elle::utility::Parser::KindNone) == elle::Status::Error)
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
        return elle::Status::Ok;
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

    // check the mutually exclusive options.
    if ((Infinit::Parser->Test("Lookup") == true) &&
        (Infinit::Parser->Test("Consult") == true) &&
        (Infinit::Parser->Test("Grant") == true) &&
        (Infinit::Parser->Test("Revoke") == true))
      {
        // display the usage.
        Infinit::Parser->Usage();

        throw elle::Exception("the create, destroy and information options are "
               "mutually exclusive");
      }

    // test the option.
    if (Infinit::Parser->Test("Lookup") == true)
      operation = Access::OperationLookup;

    // test the option.
    if (Infinit::Parser->Test("Consult") == true)
      operation = Access::OperationConsult;

    // test the option.
    if (Infinit::Parser->Test("Grant") == true)
      operation = Access::OperationGrant;

    // test the option.
    if (Infinit::Parser->Test("Revoke") == true)
      operation = Access::OperationRevoke;

    // trigger the operation.
    switch (operation)
      {
      case Access::OperationLookup:
        {
          elle::String                  path;
          elle::String                  string;
          nucleus::neutron::Subject::Type        type;
          nucleus::neutron::Subject              subject;

          // retrieve the path.
          if (Infinit::Parser->Value("Path",
                                     path) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the path value");

          // retrieve the type.
          if (Infinit::Parser->Value("Type",
                                     string) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the type value");

          // convert the string into a subject type.
          if (nucleus::neutron::Subject::Convert(string, type) == elle::Status::Error)
            throw elle::Exception(elle::sprintf("unable to convert the string '%s' into a "
                   "valid subject type",
                   string.c_str()));

          // build a subject depending on the type.
          switch (type)
            {
            case nucleus::neutron::Subject::TypeUser:
              {
                cryptography::PublicKey         K;
                std::string                           res;

                // retrieve the identity which is supposed to
                // represent a user identity i.e a public key.
                if (Infinit::Parser->Value(
                      "Identity",
                      res) == elle::Status::Error)
                  throw elle::Exception("unable to retrieve the identity");

                if (K.Restore(res) == elle::Status::Error)
                  throw elle::Exception("unable to retrieve the user's public key "
                         "through the identity");

                // build the subject.
                if (subject.Create(K) == elle::Status::Error)
                  throw elle::Exception("unable to create the subject");

                break;
              }
            case nucleus::neutron::Subject::TypeGroup:
              {
                // XXX
                throw elle::Exception("not yet supported");

                break;
              }
            default:
              {
                throw elle::Exception(elle::sprintf("unsupported entity type '%u'", type));
              }
            }

          Access::lookup(path, subject);

          break;
        }
      case Access::OperationConsult:
        {
          elle::String                  path;

          // retrieve the path.
          if (Infinit::Parser->Value("Path",
                                     path) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the path value");

          // declare additional local variables.
          Access::consult(path);
          break;
        }
      case Access::OperationGrant:
        {
          elle::String                  path;
          elle::String                  string;
          nucleus::neutron::Subject::Type        type;
          nucleus::neutron::Subject              subject;
          nucleus::neutron::Permissions          permissions;

          // retrieve the path.
          if (Infinit::Parser->Value("Path",
                                     path) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the path value");

          // retrieve the type.
          if (Infinit::Parser->Value("Type",
                                     string) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the type value");

          // convert the string into a subject type.
          if (nucleus::neutron::Subject::Convert(string, type) == elle::Status::Error)
            throw elle::Exception(elle::sprintf("unable to convert the string '%s' into a "
                   "valid subject type",
                   string.c_str()));

          // build a subject depending on the type.
          switch (type)
            {
            case nucleus::neutron::Subject::TypeUser:
              {
                cryptography::PublicKey         K;
                std::string res;

                // retrieve the identity which is supposed to
                // represent a user identity i.e a public key.
                if (Infinit::Parser->Value(
                      "Identity",
                      res) == elle::Status::Error)
                  throw elle::Exception("unable to retrieve the identity");

                if (K.Restore(res) == elle::Status::Error)
                  throw elle::Exception("unable to retrieve the user's public key "
                         "through the identity");

                // build the subject.
                if (subject.Create(K) == elle::Status::Error)
                  throw elle::Exception("unable to create the subject");

                break;
              }
            case nucleus::neutron::Subject::TypeGroup:
              {
                // XXX
                throw elle::Exception("not yet supported");

                break;
              }
            default:
              {
                throw elle::Exception(elle::sprintf("unsupported entity type '%u'",
                       type));
              }
            }

          // initialize the permissions to none.
          permissions = nucleus::neutron::permissions::none;

          // grant the read permission, if requested.
          if (Infinit::Parser->Test("Read") == true)
            permissions |= nucleus::neutron::permissions::read;

          // grant the write permission, if requested.
          if (Infinit::Parser->Test("Write") == true)
            permissions |= nucleus::neutron::permissions::write;

          // declare additional local variables.
          Access::grant(path, subject, permissions);
          break;
        }
      case Access::OperationRevoke:
        {
          elle::String                  path;
          elle::String                  string;
          nucleus::neutron::Subject::Type        type;
          nucleus::neutron::Subject              subject;

          // retrieve the path.
          if (Infinit::Parser->Value("Path",
                                     path) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the path value");

          // retrieve the type.
          if (Infinit::Parser->Value("Type",
                                     string) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the type value");

          // convert the string into a subject type.
          if (nucleus::neutron::Subject::Convert(string, type) == elle::Status::Error)
            throw elle::Exception(elle::sprintf("unable to convert the string '%s' into a "
                   "valid subject type",
                   string.c_str()));

          // build a subject depending on the type.
          switch (type)
            {
            case nucleus::neutron::Subject::TypeUser:
              {
                cryptography::PublicKey         K;
                std::string res;

                // retrieve the identity which is supposed to
                // represent a user identity i.e a public key.
                if (Infinit::Parser->Value(
                      "Identity",
                      res) == elle::Status::Error)
                  throw elle::Exception("unable to retrieve the identity");

                if (K.Restore(res) == elle::Status::Error)
                  throw elle::Exception("unable to retrieve the user's public key "
                         "through the identity");

                // build the subject.
                if (subject.Create(K) == elle::Status::Error)
                  throw elle::Exception("unable to create the subject");

                break;
              }
            case nucleus::neutron::Subject::TypeGroup:
              {
                // XXX
                throw elle::Exception("not yet supported");

                break;
              }
            default:
              {
                throw elle::Exception(elle::sprintf("unsupported entity type '%u'",
                       type));
              }
            }

          Access::revoke(path, subject);
          break;
        }
      case Access::OperationUnknown:
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

    // clean Infinit.
    if (Infinit::Clean() == elle::Status::Error)
      throw elle::Exception("unable to clean Infinit");

    // clean Lune
    if (lune::Lune::Clean() == elle::Status::Error)
      throw elle::Exception("unable to clean Lune");

    return elle::Status::Ok;
  }
}

int main(int argc, char** argv)
{
  return satellite_main("8access", [&] {
                          satellite::Access(argc, argv);
                        });
}
