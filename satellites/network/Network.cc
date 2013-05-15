#include <satellites/network/Network.hh>
#include <satellites/satellite.hh>

#include <Infinit.hh>

#include <etoile/Etoile.hh>
#include <etoile/nest/Nest.hh>
#include <etoile/automaton/Access.hh>

#include <hole/Authority.hh>
#include <hole/Hole.hh>
#include <hole/Openness.hh>
#include <hole/storage/Directory.hh>

#include <elle/io/Console.hh>
#include <elle/io/Directory.hh>
#include <elle/io/Piece.hh>
#include <elle/io/Unique.hh>
#include <elle/utility/Parser.hh>

#include <cryptography/oneway.hh>
#include <cryptography/random.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

#include <lune/Lune.hh>
#include <lune/Descriptor.hh>
#include <lune/Identity.hh>

#include <nucleus/proton/Network.hh>
#include <nucleus/proton/MutableBlock.hh>
#include <nucleus/proton/ImmutableBlock.hh>
#include <nucleus/proton/Porcupine.hh>
#include <nucleus/proton/Door.hh>
#include <nucleus/proton/Address.hh>
#include <nucleus/neutron/Object.hh>
#include <nucleus/neutron/Genre.hh>
#include <nucleus/neutron/Access.hh>

#include <horizon/Policy.hh>

#include <Infinit.hh>
#include <Program.hh>

namespace satellite
{
//
// ---------- methods ---------------------------------------------------------
//

  ///
  /// this method creates a new network by using the user 'name' as the
  /// initial user.
  ///
  elle::Status          Network::Create(const elle::String&     name,
                                        const hole::Model&      model,
                                        hole::Openness const& openness,
                                        horizon::Policy const& policy,
                                        const elle::String&     administrator)
  {
    elle::String identifier(name);
    lune::Identity identity;

    //
    // test the arguments.
    //
    {
      // does the network already exist.
      if (lune::Descriptor::exists(administrator, identifier) == true)
        throw elle::Exception("this network seems to already exist");

      // check the model.
      if (model == hole::Model::Null)
        throw elle::Exception("please specify the model of the network");

      // does the administrator user exist.
      if (lune::Identity::exists(administrator) == false)
        throw elle::Exception("the administrator user does not seem to exist");
    }

    // Retrieve the authority.
    elle::Authority authority(elle::io::Path{lune::Lune::Authority});
    {
      elle::String              prompt;
      elle::String              pass;

      // prompt the user for the passphrase.
      prompt = "Enter passphrase for the authority: ";

      if (elle::io::Console::Input(
            pass,
            prompt,
            elle::io::Console::OptionPassword) == elle::Status::Error)
        throw elle::Exception("unable to read the input");

      // decrypt the authority.
      if (authority.Decrypt(pass) == elle::Status::Error)
        throw elle::Exception("unable to decrypt the authority");
    }

    //
    // retrieve the administrator's identity.
    //
    {
      elle::String              prompt;
      elle::String              pass;

      // prompt the user for the passphrase.
      prompt = "Enter passphrase for keypair '" + administrator + "': ";

      if (elle::io::Console::Input(
            pass,
            prompt,
            elle::io::Console::OptionPassword) == elle::Status::Error)
        throw elle::Exception("unable to read the input");

      // load the identity.
      identity.load(administrator);

      // decrypt the authority.
      if (identity.Decrypt(pass) == elle::Status::Error)
        throw elle::Exception("unable to decrypt the identity");
    }

    nucleus::proton::Network network(identifier);

    //
    // create an "everybody" group.
    //
    nucleus::neutron::Group group(network,
                                  identity.pair().K(),
                                  "everybody");

    elle::io::Path shelter_path(lune::Lune::Shelter);
    shelter_path.Complete(elle::io::Piece{"%USER%", administrator},
                          elle::io::Piece{"%NETWORK%", identifier});
    hole::storage::Directory storage(network, shelter_path.string());

    group.seal(identity.pair().k());

    nucleus::proton::Address group_address(group.bind());

    storage.store(group_address, group);

    // XXX[we should use a null nest in this case because no block should be loaded/unloded]
    etoile::nest::Nest nest(ACCESS_SECRET_KEY_LENGTH,
                            nucleus::proton::Limits(nucleus::proton::limits::Porcupine{},
                                                    nucleus::proton::limits::Node{1024, 0.5, 0.2},
                                                    nucleus::proton::limits::Node{1024, 0.5, 0.2}),
                            network,
                            identity.pair().K());
    nucleus::proton::Porcupine<nucleus::neutron::Access> access(nest);
    nucleus::proton::Radix* access_radix = nullptr;

    // depending on the policy.
    switch (policy)
      {
      case horizon::Policy::accessible:
      case horizon::Policy::editable:
        {
          // Create an access block and add the 'everybody' group
          // to it.
          nucleus::neutron::Subject subject;
          nucleus::neutron::Permissions permissions;

          switch (policy)
            {
            case horizon::Policy::accessible:
              {
                permissions = nucleus::neutron::permissions::read;

                break;
              }
            case horizon::Policy::editable:
              {
                permissions = nucleus::neutron::permissions::write;

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
                throw elle::Exception("invalid policy");
              }
            }

          if (subject.Create(group_address) == elle::Status::Error)
            throw elle::Exception("unable to create the group subject");

          nucleus::proton::Door<nucleus::neutron::Access> door =
            access.lookup(subject);

          door.open();

          // Note that a null token is provided because the root directory
          // contains no data.
          door().insert(new nucleus::neutron::Record{subject, permissions});

          door.close();

          access.update(subject);

          // XXX
          static cryptography::SecretKey secret_key(cryptography::cipher::Algorithm::aes256,
                                                    ACCESS_SECRET_KEY);

          access_radix = new nucleus::proton::Radix{access.seal(secret_key)};

          break;
        }
      case horizon::Policy::confidential:
        {
          access_radix = new nucleus::proton::Radix{};

          break;
        }
      }

    cryptography::Digest fingerprint =
      nucleus::neutron::access::fingerprint(access);

    ELLE_ASSERT(access_radix != nullptr);
    ELLE_ASSERT(access_radix->strategy() == nucleus::proton::Strategy::value);

    //
    // create the root directory.
    //
    nucleus::neutron::Object directory(network,
                                       identity.pair().K(),
                                       nucleus::neutron::Genre::directory);

    if (directory.Update(directory.author(),
                         directory.contents(),
                         directory.size(),
                         *access_radix,
                         directory.owner_token()) == elle::Status::Error)
      throw elle::Exception("unable to update the directory");

    // seal the directory.
    if (directory.Seal(identity.pair().k(), fingerprint) == elle::Status::Error)
      throw elle::Exception("unable to seal the object");

    nucleus::proton::Address directory_address(directory.bind());

    storage.store(directory_address, directory);

    //
    // create the network's descriptor.
    //
    {
      // Create the meta section.
      cryptography::Signature meta_signature =
        authority.k().sign(
          lune::descriptor::meta::hash(identifier,
                                       identity.pair().K(),
                                       model,
                                       directory_address,
                                       group_address,
                                       false,
                                       1048576));
      lune::descriptor::Meta meta_section(identifier,
                                          identity.pair().K(),
                                          model,
                                          directory_address,
                                          group_address,
                                          false,
                                          1048576,
                                          meta_signature);

      // Create the data section.
      cryptography::Signature data_signature =
        identity.pair().k().sign(
          lune::descriptor::data::hash(name,
                                       openness,
                                       policy,
                                       Infinit::version,
                                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0, 0));
      lune::descriptor::Data data_section(name,
                                          openness,
                                          policy,
                                          Infinit::version,
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                          0, 0, 0, 0, 0, 0, 0, 0, 0,
                                          data_signature);

      // Create the descriptor from both sections and store it.
      lune::Descriptor descriptor(std::move(meta_section),
                                  std::move(data_section));

      descriptor.store(identity);
    }

    return elle::Status::Ok;
  }

  ///
  /// this method destroys an existing network.
  ///
  elle::Status          Network::Destroy(const elle::String& administrator,
                                         const elle::String&    name)
  {
    elle::String identifier(name);

    //
    // remove the descriptor.
    //
    {
      elle::io::Path        path;

      // does the network exist.
      if (lune::Descriptor::exists(administrator, identifier) == true)
        lune::Descriptor::erase(administrator, identifier);
    }

    //
    // destroy the shelter.
    //
    {
      elle::io::Path        path;

      // create the shelter path.
      if (path.Create(lune::Lune::Shelter) == elle::Status::Error)
        throw elle::Exception("unable to create the path");

      // complete the path with the network name.
      if (path.Complete(elle::io::Piece("%USER%", administrator),
                        elle::io::Piece("%NETWORK%", identifier)) == elle::Status::Error)
        throw elle::Exception("unable to complete the path");

      // if the shelter exists, clear it and remove it.
      if (elle::io::Directory::Exist(path) == true)
        {
          // clear the shelter content.
          if (elle::io::Directory::Clear(path) == elle::Status::Error)
            throw elle::Exception("unable to clear the directory");

          // remove the directory.
          if (elle::io::Directory::Remove(path) == elle::Status::Error)
            throw elle::Exception("unable to remove the directory");
        }
    }

    //
    // remove the network directory.
    //
    {
      elle::io::Path        path;

      // create the network path.
      if (path.Create(lune::Lune::Network) == elle::Status::Error)
        throw elle::Exception("unable to create the path");

      // complete the path with the network name.
      if (path.Complete(elle::io::Piece("%USER%", administrator),
                        elle::io::Piece("%NETWORK%", identifier)) == elle::Status::Error)
        throw elle::Exception("unable to complete the path");

      // if the network exists, clear it and remove it.
      if (elle::io::Directory::Exist(path) == true)
        {
          // clear the network directory content.
          if (elle::io::Directory::Clear(path) == elle::Status::Error)
            throw elle::Exception("unable to clear the directory");

          // remove the directory.
          if (elle::io::Directory::Remove(path) == elle::Status::Error)
            throw elle::Exception("unable to remove the directory");
        }
    }

    return elle::Status::Ok;
  }

  ///
  /// this method retrieves and displays information on the given network.
  ///
  elle::Status          Network::Information(const elle::String& administrator,
                                             const elle::String& name)
  {
    elle::String identifier(name);

    //
    // test the arguments.
    //
    {
      // does the network exist.
      if (lune::Descriptor::exists(administrator, identifier) == false)
        throw elle::Exception("this network does not seem to exist");
    }

    lune::Descriptor descriptor(administrator, identifier);

    // validate the descriptor.
    descriptor.validate(Infinit::authority());

    std::cout << descriptor << std::endl;

    return elle::Status::Ok;
  }

//
// ---------- functions -------------------------------------------------------
//

  ///
  /// the main function.
  ///
  elle::Status
  Network(elle::Natural32 argc,
          elle::Character* argv[])
  {
    Network::Operation  operation;

    // XXX Infinit::Parser is not deleted in case of errors

    // set up the program.
    if (elle::concurrency::Program::Setup("Network") == elle::Status::Error)
      throw elle::Exception("unable to set up the program");

    // initialize the Lune library.
    if (lune::Lune::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Lune");

    // initialize Infinit.
    if (Infinit::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Infinit");

    // initialize the operation.
    operation = Network::OperationUnknown;

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
          "Create",
          'c',
          "create",
          "create a new network",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Destroy",
          'd',
          "destroy",
          "destroy an existing network",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Information",
          'x',
          "information",
          "display information regarding a network",
          elle::utility::Parser::KindNone) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Name",
          'n',
          "name",
          "specify the network name",
          elle::utility::Parser::KindRequired) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Model",
          'm',
          "model",
          "specify the network model: Local, Remote, Kool etc.",
          elle::utility::Parser::KindRequired) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    // register the options.
    if (Infinit::Parser->Register(
          "Administrator",
          'a',
          "administrator",
          "specify the network administrator",
          elle::utility::Parser::KindRequired) == elle::Status::Error)
      throw elle::Exception("unable to register the option");

    if (Infinit::Parser->Example(
          "-c -n test -m slug -a fistouille") == elle::Status::Error)
      throw elle::Exception("unable to register the example");

    if (Infinit::Parser->Example(
          "-x -n test -a fistouille") == elle::Status::Error)
      throw elle::Exception("unable to register the example");

    if (Infinit::Parser->Example(
          "-d -n test -a fistouille") == elle::Status::Error)
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
        return elle::Status::Ok;
      }

    // check the mutually exclusive options.
    if ((Infinit::Parser->Test("Create") == true) &&
        (Infinit::Parser->Test("Destroy") == true) &&
        (Infinit::Parser->Test("Information") == true))
      {
        // display the usage.
        Infinit::Parser->Usage();

        throw elle::Exception("the create, destroy and information options are "
               "mutually exclusive");
      }

    // test the option.
    if (Infinit::Parser->Test("Create") == true)
      operation = Network::OperationCreate;

    // test the option.
    if (Infinit::Parser->Test("Destroy") == true)
      operation = Network::OperationDestroy;

    // test the option.
    if (Infinit::Parser->Test("Information") == true)
      operation = Network::OperationInformation;

    // trigger the operation.
    switch (operation)
      {
      case Network::OperationCreate:
        {
          elle::String          name;
          elle::String          string;
          hole::Model           model;
          elle::String          administrator;

          // retrieve the name.
          if (Infinit::Parser->Value("Name", name) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the name value");

          // retrieve the model.
          if (Infinit::Parser->Value("Model", string) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the model value");

          // build the model.
          if (model.Create(string) == elle::Status::Error)
            throw elle::Exception("unable to create the model");

          // retrieve the administrator.
          if (Infinit::Parser->Value("Administrator",
                                     administrator) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the administrator value");

          // create the network.
          if (Network::Create(name,
                              model,
                              hole::Openness::closed, // XXX[make an option]
                              horizon::Policy::accessible, // XXX[make an option]
                              administrator) == elle::Status::Error)
            throw elle::Exception("unable to create the network");

          // display a message.
          std::cout << "The network has been created successfully!"
                    << std::endl;

          break;
        }
      case Network::OperationDestroy:
        {
          elle::String          administrator;
          elle::String          name;

          // retrieve the administrator.
          if (Infinit::Parser->Value("Administrator",
                                     administrator) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the administrator value");

          // retrieve the name.
          if (Infinit::Parser->Value("Name", name) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the name value");

          // destroy the network.
          if (Network::Destroy(administrator, name) == elle::Status::Error)
            throw elle::Exception("unable to destroy the network");

          // display a message.
          std::cout << "The network has been destroyed successfully!"
                    << std::endl;

          break;
        }
      case Network::OperationInformation:
        {
          elle::String          administrator;
          elle::String          name;

          // retrieve the administrator.
          if (Infinit::Parser->Value("Administrator",
                                     administrator) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the administrator value");

          // retrieve the name.
          if (Infinit::Parser->Value("Name", name) == elle::Status::Error)
            throw elle::Exception("unable to retrieve the name value");

          // get information on the network.
          if (Network::Information(administrator, name) == elle::Status::Error)
            throw elle::Exception("unable to retrieve information on the network");

          break;
        }
      case Network::OperationUnknown:
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

int
main(int argc, char** argv)
{
  return satellite_main("8network", std::bind(satellite::Network, argc, argv));
}
