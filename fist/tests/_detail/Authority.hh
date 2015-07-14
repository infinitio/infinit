#ifndef FIST_SURFACE_GAP_TESTS_AUTHORITY_HH
# define FIST_SURFACE_GAP_TESTS_AUTHORITY_HH

#include <cryptography/rsa/KeyPair.hh>

#include <papier/Authority.hh>

namespace tests
{
  class KeyPair:
    public infinit::cryptography::rsa::KeyPair
  {
  public:
    KeyPair()
      : infinit::cryptography::rsa::KeyPair(
        infinit::cryptography::rsa::keypair::generate(1024))
    {}
  };

  extern KeyPair authority_keys;

  extern papier::Authority authority;
}

#endif
