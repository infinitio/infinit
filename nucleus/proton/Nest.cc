#include <nucleus/proton/Nest.hh>

namespace nucleus
{
  namespace proton
  {
    /*-------------.
    | Construction |
    `-------------*/

    Nest::Nest(Limits const& limits,
               Network const& network,
               cryptography::PublicKey const& agent_K):
      _limits(limits),
      _network(network),
      _agent_K(agent_K)
    {
    }

    namespace nest
    {
      /*----------.
      | Functions |
      `----------*/

      None&
      none()
      {
        // XXX[update the numbers below]
        static Limits limits{
          limits::Porcupine{},
          limits::Node{1048576, 0.5, 0.2},
          limits::Node{1048576, 0.5, 0.2}};
        static Network network{"none"};
        static cryptography::KeyPair keypair =
          cryptography::KeyPair::generate(cryptography::Cryptosystem::rsa,
                                          1024);
        static None none{limits, network, keypair.K()};

        return (none);
      }
    }
  }
}
