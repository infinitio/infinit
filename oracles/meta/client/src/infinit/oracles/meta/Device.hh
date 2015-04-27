#ifndef INFINIT_ORACLES_META_CLIENT_DEVICE_HH
# define INFINIT_ORACLES_META_CLIENT_DEVICE_HH

# include <elle/Printable.hh>
# include <elle/UUID.hh>

# include <das/model.hh>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      struct Device
        : public elle::Printable
      {
      /*-------------.
      | Construction |
      `-------------*/
      public:
        Device() = default;
        Device(elle::serialization::SerializerIn& s);
        elle::UUID id;
        das::Variable<std::string> name;
        boost::optional<std::string> model;
        boost::optional<std::string> os;
        boost::optional<std::string> passport;
        boost::optional<boost::posix_time::ptime> last_sync;

      /*--------------.
      | Serialization |
      `--------------*/
      public:
        void
        serialize(elle::serialization::Serializer& s);

      /*----------.
      | Printable |
      `----------*/
      protected:
        virtual
        void
        print(std::ostream& stream) const override;
      };
    }
  }
}

DAS_MODEL_FIELD(infinit::oracles::meta::Device, id);
DAS_MODEL_FIELD(infinit::oracles::meta::Device, model);
DAS_MODEL_FIELD(infinit::oracles::meta::Device, name);
DAS_MODEL_FIELD(infinit::oracles::meta::Device, os);
DAS_MODEL_FIELD(infinit::oracles::meta::Device, passport);
DAS_MODEL_FIELD(infinit::oracles::meta::Device, last_sync);

#endif
