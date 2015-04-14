#include <boost/uuid/string_generator.hpp>

#include <elle/os/path.hh>

#include <reactor/lockable.hh>

#include <papier/Passport.hh>

#include <surface/gap/State.hh>

namespace surface
{
  namespace gap
  {
    State::Device const&
    State::device() const
    {
      static elle::UUID nil_uuid;
      ELLE_ASSERT_NEQ(this->_device_uuid, nil_uuid);
      return this->_devices.at(this->_device_uuid);
    }

    papier::Passport const&
    State::passport() const
    {
      ELLE_ASSERT(this->_passport != nullptr);
      return *this->_passport;
    }

    /// Get the local identity of the logged user.
    papier::Identity const&
    State::identity() const
    {
      ELLE_ASSERT(this->_identity != nullptr);
      return *this->_identity;
    }

  }
}
