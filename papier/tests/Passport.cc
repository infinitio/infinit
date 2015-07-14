#include <elle/test.hh>

#include <cryptography/rsa/KeyPair.hh>

#include <papier/Passport.hh>

class KeyPair:
  public infinit::cryptography::rsa::KeyPair
{
public:
  KeyPair()
    : infinit::cryptography::rsa::KeyPair(
      infinit::cryptography::rsa::keypair::generate(
        1024))
  {}
};

KeyPair authority_keys;
papier::Authority authority(authority_keys);

static
void
hash()
{
  KeyPair keys1;
  papier::Passport p11("device_id_1", "device_name_1", keys1.K(), authority);
  papier::Passport p21("device_id_2", "device_name_2", keys1.K(), authority);
  KeyPair keys2;
  papier::Passport p12("device_id_1", "device_name_1", keys2.K(), authority);
  papier::Passport p22("device_id_2", "device_name_2", keys2.K(), authority);

  std::hash<papier::Passport> hash;

  BOOST_CHECK_EQUAL(hash(p11), hash(p11));
  BOOST_CHECK_NE   (hash(p11), hash(p12));
  BOOST_CHECK_NE   (hash(p11), hash(p21));
  BOOST_CHECK_NE   (hash(p11), hash(p22));
  BOOST_CHECK_NE   (hash(p12), hash(p11));
  BOOST_CHECK_EQUAL(hash(p12), hash(p12));
  BOOST_CHECK_NE   (hash(p12), hash(p21));
  BOOST_CHECK_NE   (hash(p12), hash(p22));
  BOOST_CHECK_NE   (hash(p21), hash(p11));
  BOOST_CHECK_NE   (hash(p21), hash(p12));
  BOOST_CHECK_EQUAL(hash(p21), hash(p21));
  BOOST_CHECK_NE   (hash(p21), hash(p22));
  BOOST_CHECK_NE   (hash(p22), hash(p11));
  BOOST_CHECK_NE   (hash(p22), hash(p12));
  BOOST_CHECK_NE   (hash(p22), hash(p21));
  BOOST_CHECK_EQUAL(hash(p22), hash(p22));
}

static
void
ordering()
{
  KeyPair keys1;
  papier::Passport p11("device_id_1", "device_name_1", keys1.K(), authority);
  papier::Passport p21("device_id_2", "device_name_2", keys1.K(), authority);
  KeyPair keys2;
  papier::Passport p12("device_id_1", "device_name_1", keys2.K(), authority);
  papier::Passport p22("device_id_2", "device_name_1", keys2.K(), authority);

  BOOST_CHECK(p11 < p21);
  BOOST_CHECK(!(p21 < p11));
  BOOST_CHECK((p11 < p12) != (p12 < p11));
  BOOST_CHECK((p11 < p22) != (p22 < p11));
  BOOST_CHECK((p12 < p21) != (p21 < p12));
}

ELLE_TEST_SUITE()
{
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(hash), 0, 10);
  suite.add(BOOST_TEST_CASE(ordering), 0, 10);
}
