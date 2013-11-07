#ifndef INFINIT_ORACLES_TROPHONIUS_SERVER_META_HH
# define INFINIT_ORACLES_TROPHONIUS_SERVER_META_HH

# include <infinit/oracles/trophonius/server/Client.hh>

namespace infinit
{
  namespace oracles
  {
    namespace trophonius
    {
      namespace server
      {
        class Meta:
          public Client
        {
        public:
          Meta(Trophonius& trophonius,
               std::unique_ptr<reactor::network::TCPSocket>&& socket);

          void
          _handle() override;

        /*----------.
        | Printable |
        `----------*/
        public:
          void
          print(std::ostream& stream) const override;
        };
      }
    }
  }
}

#endif
