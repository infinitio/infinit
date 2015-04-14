#ifndef INFINIT_ORACLES_DEVICE_HH
# define INFINIT_ORACLES_DEVICE_HH

# include <boost/optional.hpp>
# include <boost/date_time/posix_time/posix_time.hpp>

# include <elle/Printable.hh>
# include <elle/UUID.hh>
# include <elle/serialization/fwd.hh>
# include <elle/serialization/json.hh>
# include <elle/serialization/Serializer.hh>

namespace infinit
{
  namespace oracles
  {
    struct Device
      : public elle::Printable
    {
    public:
      typedef elle::UUID Id;
      Device() = default;
      Device(elle::serialization::SerializerIn& s);
      elle::UUID id;
      std::string name;
      boost::optional<std::string> os;
      boost::optional<std::string> passport;
      boost::optional<boost::posix_time::ptime> last_sync;
    private:
      boost::optional<bool> _connected;

    public:
      bool
      connected() const;

      void
      serialize(elle::serialization::Serializer& s);
      virtual
      void
      print(std::ostream& stream) const override;

      bool
      operator ==(Device const&) const;
      bool
      operator !=(Device const&) const;
      bool
      operator <(Device const&) const;
    };
  }
}

#endif
