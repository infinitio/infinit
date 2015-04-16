#ifndef FIST_SURFACE_GAP_TESTS_AUTHORITY_HH
# define FIST_SURFACE_GAP_TESTS_AUTHORITY_HH

#include <cryptography/KeyPair.hh>

#include <papier/Authority.hh>

namespace tests
{
  class KeyPair:
    public infinit::cryptography::KeyPair
  {
  public:
    KeyPair()
      : infinit::cryptography::KeyPair(
        infinit::cryptography::KeyPair::generate(
          infinit::cryptography::Cryptosystem::rsa, 1024))
    {}
  };

  extern KeyPair authority_keys;

  extern papier::Authority authority;
}

#endif
