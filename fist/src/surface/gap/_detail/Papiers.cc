#include <surface/gap/State.hh>

#include <common/common.hh>

#include <elle/os/path.hh>

#include <papier/Passport.hh>

#include <reactor/lockable.hh>

#include <boost/uuid/string_generator.hpp>

ELLE_LOG_COMPONENT("infinit.surface.gap.Device");

namespace surface
{
  namespace gap
  {
    bool
    State::has_device() const
    {
      ELLE_TRACE_METHOD("");
      if (this->_passport != nullptr)
        return true;
      return false;
    }

    void
    State::update_device(std::string const& name) const
    {
      ELLE_TRACE_METHOD(name);

      ELLE_DEBUG("update device name to %s", name);
      std::string passport_path =
        common::infinit::passport_path(this->home(), this->me().id).string();

      ELLE_ASSERT(this->_device != nullptr);

      auto res = this->_meta.update_device(this->_device->id, name);
      this->_device.reset(new Device(res));
      auto passport_string = res.passport;

      if (this->_passport != nullptr)
        ELLE_WARN("%s: a passport was already present: %s",
                  *this, *this->_passport);

      this->_passport.reset(new papier::Passport());
      if (this->_passport->Restore(passport_string) == elle::Status::Error)
        throw Exception(gap_wrong_passport, "Cannot load the passport");

      this->_passport->store(elle::io::Path(passport_path));
    }

    Device const&
    State::device() const
    {
      ELLE_ASSERT(this->_device != nullptr);
      return *this->_device;
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
