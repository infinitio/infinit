#define BOOST_TEST_MODULE Etoile
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

#include <cryptography/KeyPair.hh>

#include <reactor/network/Protocol.hh>
#include <reactor/scheduler.hh>

#include <nucleus/neutron/Object.hh>

#include <hole/storage/Memory.hh>
#include <hole/implementations/slug/Slug.hh>

#include <etoile/Etoile.hh>
#include <etoile/path/Chemin.hh>
#include <etoile/wall/Directory.hh>
#include <etoile/wall/File.hh>
#include <etoile/wall/Path.hh>

infinit::cryptography::KeyPair authority_keys =
  infinit::cryptography::KeyPair::generate(
  infinit::cryptography::Cryptosystem::rsa, 1024);
elle::Authority authority(authority_keys);

infinit::cryptography::KeyPair keys =
  infinit::cryptography::KeyPair::generate(
    infinit::cryptography::Cryptosystem::rsa, 1024);

static
void
test()
{
  nucleus::proton::Network network("test network");

  // Create root block.
  nucleus::neutron::Object root(network, keys.K(),
                                nucleus::neutron::Genre::directory);
  root.Seal(keys.k());
  auto root_addr = root.bind();

  hole::storage::Memory storage(network);
  elle::Passport passport("passport", "host", keys.K(), authority);

  std::unique_ptr<hole::Hole> slug(
    new hole::implementations::slug::Slug(storage,
                                          passport,
                                          authority,
                                          reactor::network::Protocol::tcp));
  slug->push(root_addr, root);

  etoile::Etoile etoile(keys, slug.get(), root_addr);

  auto root_chemin = etoile::wall::Path::resolve(etoile, "/");
  auto root_actor = etoile::wall::Directory::load(root_chemin);

  auto file_actor = etoile::wall::File::create();
  etoile::wall::Directory::add(root_actor, "test_file", file_actor);
  etoile::wall::File::write(file_actor, 0,
                            elle::ConstWeakBuffer("Hello world.", 12));
  etoile::wall::File::store(file_actor);
  etoile::wall::Directory::store(root_actor);
}

BOOST_AUTO_TEST_CASE(test_etoile)
{
  reactor::Scheduler sched;
  reactor::Thread t(sched, "main", [&] { test(); });
  sched.run();
}
