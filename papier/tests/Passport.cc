#include <elle/test.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/TupleSerializer.hxx>
#include <elle/format/hexadecimal.hh>
#include <elle/format/base64.hh>

#include <cryptography/rsa/KeyPair.hh>

#include <papier/Passport.hh>
#include <papier/Authority.hh>

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

static
void
legacy()
{
  papier::Authority authority = papier::authority();

  // An old version of the passport that we are going to validate
  // in a newer version of the software.
  elle::String archive("00002400000061633437326138342d653335312d346464662d396635632d333538303762336237383034020000007063000000000100000000010000beddf34b8736ad7aa1c22c6031ebb240bb777710fb53bda8f5fe5986243796e1107dc74f8dfe617d0edacc27321c0944e788f780942cdd234166e7fa2f97307cd9596511c4be63d015a9ff2b3a97d9ee1edf86dc2bc9063f44d55e8228c1462eb9165aa4a3607b858a138747c24cf1e8ec6b3d356773c1589312b4b9298acedf538bcbd37e76afb9d9ea4fa35e086d2e600019c0d8bea15a723ed86792b016e2d6b7d17a211e3bfaef352c2d6e7bab07d44ec0839bdcef74404004a4632c68eb9b2790a9c8e6382de273b77c1771dc3a2cce5fffa6bbd0357c1ce84f92f0c304c20ae38ffbf86a5af1db2ea98b89972dda7539c46d9209315ab828ba048561db0000030000000100010000000000020000000000006b6af30e49360a31f38b0ae236eec692481639a03fb06042fcdc38bedd063c0f7eaa2bd16fc34f294a15d10722ab508cdd677945c0456f293a272bfc7150f555f1bf5cd13461c52cb474e03a89e712385538dabc23be1720396fa551b7f94e9423323dc6658704bf2939e068577b27976e33e3aa00cc00116d879758d62bd3ca6800ee69c5a3ba98814332816f9b650aa0ce397c2964e3152c7742f9f465af245d1a707278c6bb11f19086824f53c046d3c3c8152bbb0ebfcc5d57235bf6dd76549586d28f749b78738ec5a864db69bf8902533b0138b73a42c2eebe33909b2533ed782425ced42cda3d074422bff41c4123d9924d8ee8052ecfcee45bb270037a7529a45bc320d1e49c4e6bca41aa7a61c5cf9f90d718930a0b77e76cd17f6385e2b29af7c7c235ee8373004d8a35724e710307160510b91b147f9e2a4914129caf1446c80745361829248c7a061b012b0af7beae065076c7c45b98b9b8316ae00b4b554871a40571b10c1574cc7276da36d7d666a8f29d874f8d4046043b0223386740cee9a6f51f1e654d7c182ac8b645f87819a0a209dd8759b34123ffb404775d6864a9366775727a7af546024f24b6b0212c1ddde760b869aa7eda0b2ae3dcfd56c2e45dc6624fe948b95803f38e03a2d6f7d8c04fcf69d6992bb30fc591e31b803fde1398e8a1e87082c19d7910244b92cff95dcc54afa54e3b35d36b");

  elle::Buffer h = elle::format::hexadecimal::decode(archive);
  elle::Buffer b64 = elle::format::base64::encode(h);

  papier::Passport passport;
  elle::serialize::from_string<
    elle::serialize::InputBase64Archive>(b64.string()) >> passport;

  BOOST_CHECK_EQUAL(passport.validate(authority), true);
}

ELLE_TEST_SUITE()
{
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(hash), 0, 10);
  suite.add(BOOST_TEST_CASE(ordering), 0, 10);
  suite.add(BOOST_TEST_CASE(legacy), 0, 10);
}
