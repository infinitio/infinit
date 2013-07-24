#define BOOST_TEST_DYN_LINK
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

#include <elle/attribute.hh>
#include <elle/cast.hh>

#include <cryptography/KeyPair.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

#include <hole/implementations/slug/Implementation.hh>
#include <hole/storage/Directory.hh>

#include <nucleus/neutron/Access.hh>
#include <nucleus/neutron/Genre.hh>
#include <nucleus/neutron/Object.hh>

/*-------------------.
| TemporaryDirectory |
`-------------------*/

class TemporaryDirectory
{
public:
  TemporaryDirectory()
  {
    do
      {
        this->_path = this->_tmpdir / boost::filesystem::unique_path();
      }
    while (!boost::filesystem::create_directory(this->_path));
  }

  ~TemporaryDirectory()
  {
    boost::filesystem::remove_all(this->_path);
  }

private:
  static boost::filesystem::path _tmpdir;
  ELLE_ATTRIBUTE_R(boost::filesystem::path, path);
};

boost::filesystem::path TemporaryDirectory::_tmpdir(
  boost::filesystem::temp_directory_path());

static std::vector<elle::network::Locus> members()
{
  std::vector<elle::network::Locus> res;
  res.push_back(elle::network::Locus("127.0.0.1", 12345));
  return res;
}

cryptography::KeyPair keys =
  cryptography::KeyPair::generate(cryptography::Cryptosystem::rsa, 1024);
papier::Authority authority(keys);
cryptography::KeyPair user_keys =
  cryptography::KeyPair::generate(cryptography::Cryptosystem::rsa, 1024);
papier::Passport passport("n'importe quoi", "n'importe quoi",
                        user_keys.K(), authority);

class Slug: public hole::implementations::slug::Implementation
{
public:
  Slug(nucleus::proton::Network const& network,
       papier::Passport const& passport = ::passport,
       papier::Authority const& authority = ::authority,
       std::vector<elle::network::Locus> const& members = ::members())
    : hole::implementations::slug::Implementation(
      _storage, passport, authority,
      reactor::network::Protocol::udt, members, 12345,
      boost::posix_time::seconds(1))
    , _tmp()
    , _storage(network, _tmp.path().native())
  {}

private:
  TemporaryDirectory _tmp;
  hole::storage::Directory _storage;
};

void
test()
{
  nucleus::proton::Network network("namespace");

  Slug s1(network);
  s1.join();
  Slug s2(network);
  s2.join();

  cryptography::KeyPair user_keys =
    cryptography::KeyPair::generate(cryptography::Cryptosystem::rsa, 1024);

  nucleus::neutron::Object block(network, user_keys.K(),
                                 nucleus::neutron::Genre::file);
  block.Update(nucleus::neutron::Author(),
               nucleus::proton::Address::null(),
               42,
               nucleus::proton::Address::null(),
               nucleus::neutron::Token::null());
  block.Seal(user_keys.k(), nullptr);

  auto address = block.bind();
  s1.push(address, block);

  auto retreived = elle::cast<nucleus::neutron::Object>::runtime(
    s2.pull(address, nucleus::proton::Revision::Last));
  BOOST_CHECK(retreived);
  BOOST_CHECK_EQUAL(retreived->size(), 42);

  elle::concurrency::scheduler().terminate();
}

// At some point all slug instances used to share the same Machine instance (the
// last allocated). Check that by instantiating two slug and NOT connecting
// them, they don't see each other's blocks.
void
test_separate_missing()
{
  Slug s1(network, passport, authority, std::vector<elle::network::Locus>());
  s1.join();
  Slug s2(network, passport, authority, std::vector<elle::network::Locus>());
  s2.join();

  nucleus::proton::Network network("namespace");

  cryptography::KeyPair user_keys =
    cryptography::KeyPair::generate(cryptography::Cryptosystem::rsa, 1024);

  nucleus::neutron::Object block(network, user_keys.K(),
                                 nucleus::neutron::Genre::file);
  block.Seal(user_keys.k(), nullptr);

  auto address = block.bind();
  s1.push(address, block);

  BOOST_CHECK_THROW(s2.pull(address, nucleus::proton::Revision::Last),
                    reactor::Exception);
  elle::concurrency::scheduler().terminate();
}

void
sched_wrapper(std::function<void()> const& f)
{
  auto& sched = elle::concurrency::scheduler();
  reactor::Thread test(sched, "test", f);
  sched.run();
}

#define TEST_CASE(Test)                                 \
  BOOST_TEST_CASE(std::bind(sched_wrapper, Test))       \

bool test_suite()
{
  boost::unit_test::test_suite* slug = BOOST_TEST_SUITE(
    "infinit::hole::implementations::slug");
  //slug->add(TEST_CASE(test));
  slug->add(TEST_CASE(test_separate_missing));

  boost::unit_test::framework::master_test_suite().add(slug);

  return true;
}

int
main(int argc, char** argv)
{
  return ::boost::unit_test::unit_test_main(test_suite, argc, argv);
}
