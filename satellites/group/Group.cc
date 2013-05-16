#include <satellites/group/Group.hh>
#include <satellites/satellite.hh>

#include <elle/utility/Parser.hh>
#include <elle/io/Piece.hh>
#include <elle/io/Path.hh>
#include <elle/io/Unique.hh>
#include <elle/finally.hh>

#include <reactor/exception.hh>
#include <reactor/network/tcp-socket.hh>
#include <reactor/scheduler.hh>

#include <protocol/Serializer.hh>

#include <elle/serialize/PairSerializer.hxx>

#include <etoile/gear/Identifier.hh>
#include <etoile/portal/Manifest.hh>
#include <etoile/abstract/Group.hh>

#include <nucleus/neutron/Range.hh>
#include <nucleus/neutron/Fellow.hh>
#include <nucleus/neutron/Subject.hh>

#include <lune/Lune.hh>
#include <lune/Phrase.hh>

#include <hole/Hole.hh>
#include <hole/storage/Directory.hh>

#include <boost/foreach.hpp>
#include <limits>

#include <common/common.hh>

#include <Program.hh>
#include <Descriptor.hh>

#include <HoleFactory.hh>

# define GROUP_FINALLY_ACTION_DISCARD(_variable_)                       \
  ELLE_FINALLY_LAMBDA(                                                  \
    _variable_,                                                         \
    [] (etoile::gear::Identifier const& id)                             \
    {                                                                   \
      Group::rpcs->groupdiscard(id);                                    \
    });

namespace satellite
{
  reactor::network::TCPSocket* Group::socket = nullptr;
  infinit::protocol::Serializer* Group::serializer = nullptr;
  infinit::protocol::ChanneledStream* Group::channels = nullptr;
  etoile::portal::RPC* Group::rpcs = nullptr;

  void
  Group::display(nucleus::neutron::Fellow const& fellow)
  {
    switch (fellow.subject().type())
      {
      case nucleus::neutron::Subject::TypeUser:
        {
          elle::io::Unique unique;

          // convert the public key into a human-kind-of-readable string.
          if (fellow.subject().user().Save(unique) == elle::Status::Error)
            throw reactor::Exception("unable to save the public key's unique");

          std::cout << "User"
                    << " "
                    << unique
                    << std::endl;

          break;
        }
      case nucleus::neutron::Subject::TypeGroup:
        {
          elle::io::Unique unique;

          // convert the group's address into a human-kind-of-readable string.
          if (fellow.subject().group().Save(unique) == elle::Status::Error)
            throw reactor::Exception("unable to save the address' unique");

          std::cout << "Group"
                    << " "
                    << unique
                    << std::endl;

          break;
        }
      default:
        {
          throw reactor::Exception(elle::sprintf("unknown subject type '%u'",
                                                 fellow.subject().type()));
        }
      }
  }

  void
  Group::connect()
  {
    lune::Phrase phrase;

    // Load the phrase.
    phrase.load(Infinit::User, Infinit::Network, "portal");

    Group::socket =
      new reactor::network::TCPSocket(*reactor::Scheduler::scheduler(),
                                      elle::String("127.0.0.1"),
                                      phrase.port);
    Group::serializer =
      new infinit::protocol::Serializer(*reactor::Scheduler::scheduler(), *socket);
    Group::channels =
      new infinit::protocol::ChanneledStream(*reactor::Scheduler::scheduler(),
                                             *serializer);
    Group::rpcs = new etoile::portal::RPC(*channels);

    // Authenticate.
    if (!Group::rpcs->authenticate(phrase.pass))
      throw reactor::Exception("authentication failed");
  }

  void
  Group::information(typename nucleus::neutron::Group::Identity const& identity)
  {
    Group::connect();
    etoile::gear::Identifier identifier = Group::rpcs->groupload(identity);
    GROUP_FINALLY_ACTION_DISCARD(identifier);
    etoile::abstract::Group abstract = Group::rpcs->groupinformation(identifier);
    abstract.Dump();
  }

  void
  Group::create(elle::String const& description)
  {
    Group::connect();
    auto group = Group::rpcs->groupcreate(description);
    auto identity = group.first;
    etoile::gear::Identifier identifier = group.second;
    Group::rpcs->groupstore(identifier);
    elle::io::Unique unique;
    if (identity.Save(unique) == elle::Status::Error)
      throw reactor::Exception("unable to save the identity");
    std::cout << unique << std::endl;
  }

  void
  Group::add(typename nucleus::neutron::Group::Identity const& identity,
             nucleus::neutron::Subject const& subject)
  {
    Group::connect();
    etoile::gear::Identifier identifier = Group::rpcs->groupload(identity);
    GROUP_FINALLY_ACTION_DISCARD(identifier);
    Group::rpcs->groupadd(identifier, subject);
    Group::rpcs->groupstore(identifier);
    ELLE_FINALLY_ABORT(identifier);;
  }

  void
  Group::lookup(typename nucleus::neutron::Group::Identity const& identity,
                nucleus::neutron::Subject const& subject)
  {
    Group::connect();
    etoile::gear::Identifier identifier = Group::rpcs->groupload(identity);
    GROUP_FINALLY_ACTION_DISCARD(identifier);
    nucleus::neutron::Fellow fellow(
      Group::rpcs->grouplookup(identifier, subject));
    Group::display(fellow);
  }

  void
  Group::consult(typename nucleus::neutron::Group::Identity const& identity)
  {
    nucleus::neutron::Index index(0);
    nucleus::neutron::Size size(std::numeric_limits<nucleus::neutron::Size>::max());

    Group::connect();
    etoile::gear::Identifier identifier = Group::rpcs->groupload(identity);
    GROUP_FINALLY_ACTION_DISCARD(identifier);
    nucleus::neutron::Range<nucleus::neutron::Fellow> fellows =
      Group::rpcs->groupconsult(identifier, index, size);
    for (auto& fellow: fellows)
      Group::display(*fellow);
  }

  void
  Group::remove(typename nucleus::neutron::Group::Identity const& identity,
                nucleus::neutron::Subject const& subject)
  {
    Group::connect();
    etoile::gear::Identifier identifier = Group::rpcs->groupload(identity);
    GROUP_FINALLY_ACTION_DISCARD(identifier);
    Group::rpcs->groupremove(identifier, subject);
    Group::rpcs->groupstore(identifier);
    ELLE_FINALLY_ABORT(identifier);;
  }

  void
  Group::destroy(typename nucleus::neutron::Group::Identity const& identity)
  {
    Group::connect();
    etoile::gear::Identifier identifier = Group::rpcs->groupload(identity);
    GROUP_FINALLY_ACTION_DISCARD(identifier);
    Group::rpcs->groupdestroy(identifier);
    ELLE_FINALLY_ABORT(identifier);;
  }

//
// ---------- functions -------------------------------------------------------
//

  ///
  /// the main function.
  ///
  elle::Status
  Group(elle::Natural32 argc,
        elle::Character* argv[])
  {
    Group::Operation   operation;

    // XXX Infinit::Parser is not deleted in case of errors

    // set up the program.
    if (elle::concurrency::Program::Setup("Group") == elle::Status::Error)
      throw elle::Exception("unable to set up the program");

    // initialize the Lune library.
    if (lune::Lune::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Lune");

    // initialize Infinit.
    if (Infinit::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Infinit");

    // initialize the operation.
    operation = Group::OperationUnknown;

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
          "Information",
          'x',
          "information",
          "display information on a group",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Create",
          'c',
          "create",
          "create a group",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Description",
          'd',
          "description",
          "specify the group description on creation",
          elle::utility::Parser::KindRequired) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Lookup",
          'l',
          "lookup",
          "look up a specific group record",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Consult",
          'o',
          "consult",
          "consult the group records",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Add",
          'a',
          "add",
          "add a user/group to a group",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Remove",
          'r',
          "remove",
          "remove a user/group from a group",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Destroy",
          'y',
          "destroy",
          "destroy the given group",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Group",
          'g',
          "group",
          "indicate the group base64 identity on which to operate",
          elle::utility::Parser::KindOptional) == elle::Status::Error)
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
          "Identity",
          'i',
          "identity",
          "specify the user/group base64 identity to add/remove/lookup in the group",
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

    Descriptor descriptor(Infinit::User, Infinit::Network);

    // check the mutually exclusive options.
    if ((Infinit::Parser->Test("Information") == true) &&
        (Infinit::Parser->Test("Create") == true) &&
        (Infinit::Parser->Test("Add") == true) &&
        (Infinit::Parser->Test("Lookup") == true) &&
        (Infinit::Parser->Test("Consult") == true) &&
        (Infinit::Parser->Test("Remove") == true) &&
        (Infinit::Parser->Test("Destroy") == true))
      {
        // display the usage.
        Infinit::Parser->Usage();

        throw elle::Exception("the information, create, add, lookup, consult, remove and "
               "destroy options are mutually exclusive");
      }

    // test the options.
    if (Infinit::Parser->Test("Information") == true)
      operation = Group::OperationInformation;

    if (Infinit::Parser->Test("Create") == true)
      operation = Group::OperationCreate;

    if (Infinit::Parser->Test("Add") == true)
      operation = Group::OperationAdd;

    if (Infinit::Parser->Test("Lookup") == true)
      operation = Group::OperationLookup;

    if (Infinit::Parser->Test("Consult") == true)
      operation = Group::OperationConsult;

    if (Infinit::Parser->Test("Remove") == true)
      operation = Group::OperationRemove;

    if (Infinit::Parser->Test("Destroy") == true)
      operation = Group::OperationDestroy;

    // trigger the operation.
    switch (operation)
      {
      case Group::OperationInformation:
        {
          elle::String string;
          typename nucleus::neutron::Group::Identity group;

          // retrieve the group.
          if (Infinit::Parser->Value("Group",
                                     string,
                                     elle::String()) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the group identity");

          // if no group is provided, use the "everybody" group of the network.
          if (string.empty() == false)
            {
              // convert the string into a group identity.
              if (group.Restore(string) == elle::Status::Error)
                throw elle::Exception("unable to convert the string into a group identity");
            }
          else
            {

              group = descriptor.meta().everybody_identity();
            }

          Group::information(group);

          break;
        }
      case Group::OperationCreate:
        {
          elle::String description;

          // retrieve the description.
          if (Infinit::Parser->Value("Description",
                                     description) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the description");

          Group::create(description);

          break;
        }
      case Group::OperationAdd:
        {
          elle::String string;
          typename nucleus::neutron::Group::Identity group;
          nucleus::neutron::Subject::Type type;
          nucleus::neutron::Subject subject;

          // retrieve the group.
          if (Infinit::Parser->Value("Group",
                                     string,
                                     elle::String()) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the group identity");

          // if no group is provided, use the "everybody" group of the network.
          if (string.empty() == false)
            {
              // convert the string into a group identity.
              if (group.Restore(string) == elle::Status::Error)
                throw elle::Exception("unable to convert the string into a group identity");
            }
          else
            {
              group = descriptor.meta().everybody_identity();
            }

          // retrieve the type.
          if (Infinit::Parser->Value("Type", string) == elle::Status::Error)
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
                typename nucleus::neutron::User::Identity K;

                // retrieve the identifier which is supposed to
                // represent a user identity i.e a public key.
                if (Infinit::Parser->Value("Identity",
                                           string) == elle::Status::Error)
                  throw elle::Exception("unable to retrieve the identifier");

                if (K.Restore(string) == elle::Status::Error)
                  throw elle::Exception("unable to retrieve the user's public key "
                         "through the identifier");

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
                throw elle::Exception(
                    elle::sprintf("unsupported entity type '%u'", type));
              }
            }

          Group::add(group, subject);

          break;
        }
      case Group::OperationLookup:
        {
          elle::String string;
          typename nucleus::neutron::Group::Identity group;
          nucleus::neutron::Subject::Type type;
          nucleus::neutron::Subject subject;

          // retrieve the group.
          if (Infinit::Parser->Value("Group",
                                     string,
                                     elle::String()) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the group identity");

          // if no group is provided, use the "everybody" group of the network.
          if (string.empty() == false)
            {
              // convert the string into a group identity.
              if (group.Restore(string) == elle::Status::Error)
                throw elle::Exception("unable to convert the string into a group identity");
            }
          else
            {
              group = descriptor.meta().everybody_identity();
            }

          // retrieve the type.
          if (Infinit::Parser->Value("Type", string) == elle::Status::Error)
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
                typename nucleus::neutron::User::Identity K;

                // retrieve the identifier which is supposed to
                // represent a user identity i.e a public key.
                if (Infinit::Parser->Value("Identity",
                                           string) == elle::Status::Error)
                  throw elle::Exception("unable to retrieve the identifier");

                if (K.Restore(string) == elle::Status::Error)
                  throw elle::Exception("unable to retrieve the user's public key "
                         "through the identifier");

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

          Group::lookup(group, subject);

          break;
        }
      case Group::OperationConsult:
        {
          elle::String string;
          typename nucleus::neutron::Group::Identity group;

          // retrieve the group.
          if (Infinit::Parser->Value("Group",
                                     string,
                                     elle::String()) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the group identity");

          // if no group is provided, use the "everybody" group of the network.
          if (string.empty() == false)
            {
              // convert the string into a group identity.
              if (group.Restore(string) == elle::Status::Error)
                throw elle::Exception("unable to convert the string into a group identity");
            }
          else
            {
              group = descriptor.meta().everybody_identity();
            }

          Group::consult(group);

          break;
        }
      case Group::OperationRemove:
        {
          elle::String string;
          typename nucleus::neutron::Group::Identity group;
          nucleus::neutron::Subject::Type type;
          nucleus::neutron::Subject subject;

          // retrieve the group.
          if (Infinit::Parser->Value("Group",
                                     string,
                                     elle::String()) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the group identity");

          // if no group is provided, use the "everybody" group of the network.
          if (string.empty() == false)
            {
              // convert the string into a group identity.
              if (group.Restore(string) == elle::Status::Error)
                throw elle::Exception("unable to convert the string into a group identity");
            }
          else
            {
              group = descriptor.meta().everybody_identity();
            }

          // retrieve the type.
          if (Infinit::Parser->Value("Type", string) == elle::Status::Error)
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
                typename nucleus::neutron::User::Identity K;

                // retrieve the identifier which is supposed to
                // represent a user identity i.e a public key.
                if (Infinit::Parser->Value("Identity",
                                           string) == elle::Status::Error)
                  throw elle::Exception("unable to retrieve the identifier");

                if (K.Restore(string) == elle::Status::Error)
                  throw elle::Exception("unable to retrieve the user's public key "
                         "through the identifier");

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

          Group::remove(group, subject);

          break;
        }
      case Group::OperationDestroy:
        {
          elle::String string;
          typename nucleus::neutron::Group::Identity group;

          // retrieve the group.
          if (Infinit::Parser->Value("Group",
                                     string,
                                     elle::String()) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the group identity");

          // if no group is provided, use the "everybody" group of the network.
          if (string.empty() == false)
            {
              // convert the string into a group identity.
              if (group.Restore(string) == elle::Status::Error)
                throw elle::Exception("unable to convert the string into a group identity");
            }
          else
            {
              group = descriptor.meta().everybody_identity();
            }

          Group::destroy(group);

          break;
        }
      case Group::OperationUnknown:
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
  return satellite_main("8group", [&] {
                          satellite::Group(argc, argv);
                        });
}
