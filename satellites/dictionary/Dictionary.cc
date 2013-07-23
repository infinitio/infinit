#include <satellites/dictionary/Dictionary.hh>

#include <Infinit.hh>

#include <elle/utility/Parser.hh>
#include <elle/io/Unique.hh>
#include <elle/concurrency/Program.hh>

#include <cryptography/PublicKey.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

#include <lune/Lune.hh>
#include <lune/Dictionary.hh>
#include <papier/Identity.hh>
#include <lune/Map.hh>

#include <nucleus/proton/Address.hh>

#include <etoile/Etoile.hh>

namespace satellite
{
//
// ---------- methods ---------------------------------------------------------
//

  ///
  /// this method adds an entry.
  ///
  elle::Status          Dictionary::Add(elle::String const& user,
                                        const Dictionary::Type& type,
                                        const elle::String&     name,
                                        const elle::io::Unique&     identifier)
  {
    lune::Dictionary    dictionary;

    //
    // check the arguments.
    //
    {
      papier::Identity    identity;

      // does the user identity exist.
      if (papier::Identity::exists(user) == false)
        throw elle::Exception("this user does not seem to exist");
    }

    //
    // retrieve the dictionary.
    //
    {
      // load the dictionary if it exists.
      if (lune::Dictionary::exists(user) == true)
        dictionary.load(user);
    }

    // perform the action depending on the type.
    switch (type)
      {
      case Dictionary::TypeUser:
        {
          cryptography::PublicKey       K;

          // restore the public key from the identifier.
          if (K.Restore(identifier) == elle::Status::Error)
            throw elle::Exception("unable to restore the public key");

          // add an entry.
          if (dictionary.users.Add(name, K) == elle::Status::Error)
            throw elle::Exception("unable to add the user entry to the dictionary");

          break;
        }
      case Dictionary::TypeGroup:
        {
          nucleus::proton::Address address;

          // restore the address from the identifier.
          if (address.Restore(identifier) == elle::Status::Error)
            throw elle::Exception("unable to restore the address");

          // add an entry.
          if (dictionary.groups.Add(name, address) == elle::Status::Error)
            throw elle::Exception("unable to add the group entry to the dictionary");

          break;
        }
      case Dictionary::TypeUnknown:
      default:
        {
          throw elle::Exception("please specify a type of entry to add");
        }
      }

    // store the dictionary file.
    dictionary.store(user);

    return elle::Status::Ok;
  }

  ///
  /// this method removes an entry.
  ///
  elle::Status          Dictionary::Remove(elle::String const& user,
                                           const Dictionary::Type& type,
                                           const elle::String&  name)
  {
    lune::Dictionary    dictionary;

    //
    // check the arguments.
    //
    {
      papier::Identity    identity;

      // does the user identity exist.
      if (papier::Identity::exists(user) == false)
        throw elle::Exception("this user does not seem to exist");
    }

    //
    // retrieve the dictionary.
    //
    {
      // load the dictionary if it exists.
      if (lune::Dictionary::exists(user) == true)
        dictionary.load(user);
    }

    // perform the action depending on the type.
    switch (type)
      {
      case Dictionary::TypeUser:
        {
          // remove an entry.
          if (dictionary.users.Remove(name) == elle::Status::Error)
            throw elle::Exception("unable to remove the user entry");

          break;
        }
      case Dictionary::TypeGroup:
        {
          // remove an entry.
          if (dictionary.groups.Remove(name) == elle::Status::Error)
            throw elle::Exception("unable to remove the group entry");

          break;
        }
      case Dictionary::TypeUnknown:
      default:
        {
          throw elle::Exception("please specify a type of entry to remove");
        }
      }

    // store the dictionary file.
    dictionary.store(user);

    return elle::Status::Ok;
  }

  ///
  /// this method shows all the entries.
  ///
  elle::Status          Dictionary::Show(elle::String const& user,
                                         const Dictionary::Type& type)
  {
    lune::Dictionary    dictionary;

    //
    // check the arguments.
    //
    {
      papier::Identity    identity;

      // does the user identity exist.
      if (papier::Identity::exists(user) == false)
        throw elle::Exception("this user does not seem to exist");
    }

    //
    // retrieve the dictionary.
    //
    {
      // load the dictionary if it exists.
      if (lune::Dictionary::exists(user) == true)
        dictionary.load(user);
    }

    // perform the action depending on the type.
    switch (type)
      {
      case Dictionary::TypeUser:
        {
          lune::Map<cryptography::PublicKey>::Scoutor scoutor;

          // go through the user dictionary.
          for (scoutor = dictionary.users.container.begin();
               scoutor != dictionary.users.container.end();
               scoutor++)
            {
              lune::Map<cryptography::PublicKey>::Entry* entry = *scoutor;

              std::cout << entry->name << " :: "
                        // XXX << entry->value
                        << std::endl;
            }

          break;
        }
      case Dictionary::TypeGroup:
        {
          lune::Map<nucleus::proton::Address>::Scoutor scoutor;

          // go through the group dictionary.
          for (scoutor = dictionary.groups.container.begin();
               scoutor != dictionary.groups.container.end();
               scoutor++)
            {
              lune::Map<nucleus::proton::Address>::Entry* entry = *scoutor;

              std::cout << entry->name << " :: "
                        // XXX << entry->value
                        << std::endl;
            }

          break;
        }
      case Dictionary::TypeUnknown:
      default:
        {
          throw elle::Exception("please specify a type of entry to show");
        }
      }

    return elle::Status::Ok;
  }

  ///
  /// this method dump an entry.
  ///
  elle::Status          Dictionary::Dump(elle::String const& user,
                                         const Dictionary::Type& type,
                                         const elle::String&    name)
  {
    lune::Dictionary    dictionary;

    //
    // check the arguments.
    //
    {
      papier::Identity    identity;

      // does the user identity exist.
      if (papier::Identity::exists(user) == false)
        throw elle::Exception("this user does not seem to exist");
    }

    //
    // retrieve the dictionary.
    //
    {
      // load the dictionary if it exists.
      if (lune::Dictionary::exists(user) == true)
        dictionary.load(user);
    }

    // perform the action depending on the type.
    switch (type)
      {
      case Dictionary::TypeUser:
        {
          cryptography::PublicKey*      K;
          elle::io::Unique          unique;

          // retrieve the entry.
          if (dictionary.users.Lookup(name, K) != true)
            throw elle::Exception("unable to look up the user entry");

          // retrive the public key's unique.
          if (K->Save(unique) == elle::Status::Error)
            throw elle::Exception("unable to save the public key's unique");

          // dump the information.
          std::cout << name << " :: "
                    << unique << std::endl;

          break;
        }
      case Dictionary::TypeGroup:
        {
          nucleus::proton::Address* address;
          elle::io::Unique          unique;

          // retrieve the entry.
          if (dictionary.groups.Lookup(name, address) != true)
            throw elle::Exception("unable to look up the group entry");

          // retrive the address's unique.
          if (address->Save(unique) == elle::Status::Error)
            throw elle::Exception("unable to save the address's unique");

          // dump the information.
          std::cout << name << " :: "
                    << unique << std::endl;

          break;
        }
      case Dictionary::TypeUnknown:
      default:
        {
          throw elle::Exception("please specify a type of entry to dump");
        }
      }

    return elle::Status::Ok;
  }

//
// ---------- functions -------------------------------------------------------
//

  ///
  /// the main function.
  ///
  elle::Status          Main(elle::Natural32                    argc,
                             elle::Character*                   argv[])
  {
    Dictionary::Operation       operation;

    // XXX Infinit::Parser is not deleted in case of errors

    // set up the program.
    if (elle::concurrency::Program::Setup("Dictionary") == elle::Status::Error)
      throw elle::Exception("unable to set up the program");

    // initialize the Lune library.
    if (lune::Lune::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Lune");

    // initialize Infinit.
    if (Infinit::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Infinit");

    // initialize the Etoile library.
    if (etoile::Etoile::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Etoile");

    // initialize the operation.
    operation = Dictionary::OperationUnknown;

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

    // register the options.
    if (Infinit::Parser->Register(
          "Add",
          'a',
          "add",
          "add a dictionary record",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Remove",
          'r',
          "remove",
          "remove a dictionary record",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Show",
          's',
          "show",
          "show all the dictionary records",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Dump",
          'd',
          "dump",
          "dump a specific dictionary record",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "User",
          'u',
          "user",
          "indicate the name of the user",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Type",
          't',
          "type",
          "indicate the type of the record: user or group",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Name",
          'n',
          "name",
          "specify the local UNIX/Windows/etc. name of the user/group entry",
          elle::utility::Parser::KindRequired) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Identifier",
          'i',
          "identifier",
          "specify the user/group Infinit base64 identifier",
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

    // check the mutually exclusive options.
    if ((Infinit::Parser->Test("Add") == true) &&
        (Infinit::Parser->Test("Remove") == true) &&
        (Infinit::Parser->Test("Dump") == true) &&
        (Infinit::Parser->Test("Show") == true))
      {
        // display the usage.
        Infinit::Parser->Usage();

        throw elle::Exception("the add, remove, dump and show options are "
               "mutually exclusive");
      }

    // test the option.
    if (Infinit::Parser->Test("Add") == true)
      operation = Dictionary::OperationAdd;

    // test the option.
    if (Infinit::Parser->Test("Remove") == true)
      operation = Dictionary::OperationRemove;

    // test the option.
    if (Infinit::Parser->Test("Dump") == true)
      operation = Dictionary::OperationDump;

    // test the option.
    if (Infinit::Parser->Test("Show") == true)
      operation = Dictionary::OperationShow;

    // trigger a command.
    switch (operation)
      {
      case Dictionary::OperationAdd:
        {
          elle::String user;
          elle::String t;
          Dictionary::Type      type;
          elle::String          name;
          elle::io::Unique          identifier;

          // retrieve the user.
          if (Infinit::Parser->Value("User", user) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the user name");

          // retrieve the type.
          if (Infinit::Parser->Value("Type", t) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the type value");

          if (t == "user")
            type = Dictionary::TypeUser;
          else if (t == "group")
            type = Dictionary::TypeGroup;
          else
            throw elle::Exception("unknown type '%s'",
                   t.c_str());

          // retrieve the name.
          if (Infinit::Parser->Value("Name", name) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the name value");

          // retrieve the identifier.
          if (Infinit::Parser->Value("Identifier",
                                     identifier) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the identifier value");

          // add a record.
          if (Dictionary::Add(user,
                              type,
                              name,
                              identifier) == elle::Status::Error)
            throw elle::Exception("unable to add a mapping");

          // display a message.
          std::cout << "The record has been added successfully!"
                    << std::endl;

          break;
        }
      case Dictionary::OperationRemove:
        {
          elle::String user;
          elle::String t;
          Dictionary::Type      type;
          elle::String          name;

          // retrieve the user.
          if (Infinit::Parser->Value("User", user) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the user name");

          // retrieve the type.
          if (Infinit::Parser->Value("Type", t) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the type value");

          if (t == "user")
            type = Dictionary::TypeUser;
          else if (t == "group")
            type = Dictionary::TypeGroup;
          else
            throw elle::Exception("unknown type '%s'",
                   t.c_str());

          // retrieve the name.
          if (Infinit::Parser->Value("Name", name) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the name value");

          // remove a record.
          if (Dictionary::Remove(user,
                                 type,
                                 name) == elle::Status::Error)
            throw elle::Exception("unable to remove the mapping");

          // display a message.
          std::cout << "The record has been removed successfully!"
                    << std::endl;

          break;
        }
      case Dictionary::OperationDump:
        {
          elle::String user;
          elle::String t;
          Dictionary::Type      type;
          elle::String          name;

          // retrieve the user.
          if (Infinit::Parser->Value("User", user) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the user name");

          // retrieve the type.
          if (Infinit::Parser->Value("Type", t) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the type value");

          if (t == "user")
            type = Dictionary::TypeUser;
          else if (t == "group")
            type = Dictionary::TypeGroup;
          else
            throw elle::Exception("unknown type '%s'",
                   t.c_str());

          // retrieve the name.
          if (Infinit::Parser->Value("Name", name) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the name value");

          // dump the record.
          if (Dictionary::Dump(user,
                               type,
                               name) == elle::Status::Error)
            throw elle::Exception("unable to dump the mapping");

          break;
        }
      case Dictionary::OperationShow:
        {
          elle::String user;
          elle::String t;
          Dictionary::Type      type;

          // retrieve the user.
          if (Infinit::Parser->Value("User", user) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the user name");

          // retrieve the type.
          if (Infinit::Parser->Value("Type", t) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the type value");

          if (t == "user")
            type = Dictionary::TypeUser;
          else if (t == "group")
            type = Dictionary::TypeGroup;
          else
            throw elle::Exception("unknown type '%s'",
                   t.c_str());

          // show the records.
          if (Dictionary::Show(user, type) == elle::Status::Error)
            throw elle::Exception("unable to show the mappings");

          break;
        }
      case Dictionary::OperationUnknown:
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

    // clean the Etoile.
    if (etoile::Etoile::Clean() == elle::Status::Error)
      throw elle::Exception("unable to clean Etoile");

    // clean Infinit.
    if (Infinit::Clean() == elle::Status::Error)
      throw elle::Exception("unable to clean Infinit");

    // clean Lune
    if (lune::Lune::Clean() == elle::Status::Error)
      throw elle::Exception("unable to clean Lune");

    return elle::Status::Ok;
  }

}

//
// ---------- main ------------------------------------------------------------
//

///
/// this is the program entry point.
///
int                     main(int                                argc,
                             char**                             argv)
{
  try
    {
      if (satellite::Main(argc, argv) == elle::Status::Error)
        return (1);
    }
  catch (std::exception& e)
    {
      std::cout << "The program has been terminated following "
                << "a fatal error (" << e.what() << ")." << std::endl;

      return (1);
    }

  return (0);
}
