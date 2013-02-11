#include "../State.hh"

#include <common/common.hh>

#include <elle/os/path.hh>
#include <elle/Passport.hh>

ELLE_LOG_COMPONENT("infinit.surface.gap.State");

#include "impl.hh"

namespace surface
{
  namespace gap
  {

    bool
    State::has_device() const
    {
      ELLE_ASSERT(this->_self->_me.id().size() > 0 && "not properly initialized");
      ELLE_DEBUG("Check for '%s' device existence at '%s'",
                 this->_self->_me.id(),
                 common::infinit::passport_path(this->_self->_me.id()));
      return elle::os::path::exists(common::infinit::passport_path(this->_self->_me.id()));
    }

    std::string
    State::device_id()
    {
      if (this->_self->device_id().empty() == true)
        {
          elle::Passport passport;
          passport.load(elle::io::Path{common::infinit::passport_path(this->_self->_me.id())});
          this->_self->device_id(passport.id());
          this->_self->device_name(passport.name());
        }
      return this->_self->device_id();
    }

    std::string
    State::device_name()
    {
      if (this->_self->device_name().empty() == true)
        {

          elle::Passport passport;
          passport.load(elle::io::Path{common::infinit::passport_path(this->_self->_me.id())});
          this->_self->device_id(passport.id());
          this->_self->device_name(passport.name());
        }
      return this->_self->device_name();
    }

    void
    State::update_device(std::string const& name, bool force_create)
    {
      std::string passport_path = common::infinit::passport_path(this->_self->_me.id());

      _self->device_name(name);
      ELLE_DEBUG("Device to update: '%s'", _self->device_name());

      std::string passport_string;
      if (force_create || !this->has_device())
        {
          auto res = this->_meta->create_device(name);
          passport_string = res.passport;
          this->_self->device_id(res.created_device_id);
          ELLE_DEBUG("Created device id: %s", this->_self->device_id());
        }
      else
        {
          ELLE_DEBUG("Loading passport from '%s'.", passport_path);
          elle::Passport passport;
          passport.load(elle::io::Path{passport_path});

          ELLE_DEBUG("Passport id: %s", passport.id());
          auto res = this->_meta->update_device(passport.id(), name);
          this->_self->device_id(res.updated_device_id);
          passport_string = res.passport;
        }

      elle::Passport passport;
      if (passport.Restore(passport_string) == elle::Status::Error)
        throw Exception(gap_wrong_passport, "Cannot load the passport");

      passport.store(elle::io::Path(passport_path));
    }

  }
}
