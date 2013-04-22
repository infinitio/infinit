#include "../State.hh"

#include <common/common.hh>

#include <elle/os/path.hh>
#include <hole/Passport.hh>

ELLE_LOG_COMPONENT("infinit.surface.gap.State");

namespace surface
{
  namespace gap
  {

    bool
    State::has_device() const
    {
      ELLE_ASSERT(this->_me.id.size() > 0 && "not properly initialized");
      ELLE_DEBUG("Check for '%s' device existence at '%s'",
                 this->_me.id,
                 common::infinit::passport_path(this->_me.id));
      return elle::os::path::exists(common::infinit::passport_path(this->_me.id));
    }

    std::string const&
    State::device_id()
    {
      if (this->_device.id.size() == 0)
      {
        elle::Passport passport;
        passport.load(elle::io::Path{common::infinit::passport_path(this->_me.id)});
        this->_device.id   = passport.id();
        this->_device.name = passport.name();
      }
      return this->_device.id;
    }

    std::string const&
    State::device_name()
    {
      if (this->_device.name.size() == 0)
      {
        elle::Passport passport;
        passport.load(elle::io::Path{common::infinit::passport_path(this->_me.id)});
        this->_device.id = passport.id();
        this->_device.name = passport.name();
      }
      return this->_device.name;
    }

    void
    State::update_device(std::string const& name, bool force_create)
    {
      ELLE_TRACE_FUNCTION(name, force_create);

     ELLE_DEBUG("update device %s to %s", this->_device.name, name);
      std::string passport_path = common::infinit::passport_path(this->_me.id);

      this->_device.name = name;

      std::string passport_string;
      if (force_create || !this->has_device())
      {
        auto res = this->_meta.create_device(name);
        passport_string = res.passport;
        this->_device.id = res.id;
        ELLE_DEBUG("Created device id: %s", this->_device.id);
      }
      else
      {
        ELLE_DEBUG("Loading passport from '%s'.", passport_path);
        elle::Passport passport;
        passport.load(elle::io::Path{passport_path});

        ELLE_DEBUG("Passport id: %s", passport.id());
        auto res = this->_meta.update_device(passport.id(), name);
        this->_device.id = res.id;
        passport_string = res.passport;
      }

      elle::Passport passport;
      if (passport.Restore(passport_string) == elle::Status::Error)
        throw Exception(gap_wrong_passport, "Cannot load the passport");

      passport.store(elle::io::Path(passport_path));
    }

  }
}
