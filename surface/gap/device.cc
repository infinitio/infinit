#include "State.hh"

#include <common/common.hh>

#include <elle/os/path.hh>

#include <papier/Passport.hh>

ELLE_LOG_COMPONENT("infinit.surface.gap.Device");

namespace surface
{
  namespace gap
  {
    bool
    State::has_device() const
    {
      ELLE_TRACE_METHOD("");
      if (this->_device != nullptr)
        return true;

      ELLE_ASSERT(this->me().id.size() > 0 && "not properly initialized");
      ELLE_DEBUG("Check for '%s' device existence at '%s'",
                 this->me().id,
                 common::infinit::passport_path(this->me().id));
      if (elle::os::path::exists(common::infinit::passport_path(this->me().id)))
      {
        papier::Passport passport;
        passport.load(
          elle::io::Path{common::infinit::passport_path(this->me().id)});
        auto it = std::find(this->me().devices.begin(),
                            this->me().devices.end(),
                            passport.id());
        if (it != this->me().devices.end())
          return true;
      }
      return false;
    }

    Device const&
    State::device()
    {
      ELLE_TRACE_METHOD("");

      if (!this->has_device())
        this->update_device("XXX");
      else if (this->_device == nullptr)
      {
        papier::Passport passport;
        passport.load(
          elle::io::Path{common::infinit::passport_path(this->me().id)});
        this->_device.reset(new Device{passport.id(), passport.name()});
      }
      ELLE_ASSERT(this->_device != nullptr);
      return *this->_device;
    }

    std::string const&
    State::device_id()
    {
      ELLE_TRACE_METHOD("");

      return this->device().id;
    }

    std::string const&
    State::device_name()
    {
      ELLE_TRACE_METHOD("");

      return this->device().name;
    }

    void
    State::update_device(std::string const& name, bool force_create)
    {
      ELLE_TRACE_METHOD(name, force_create);

      ELLE_DEBUG("update device name to %s", name);
      std::string passport_path = common::infinit::passport_path(this->me().id);

      std::string passport_string;
      if (force_create || !this->has_device())
      {
        auto res = this->_meta.create_device(name);
        passport_string = res.passport;
        this->_device.reset(new Device{res.id, name});
        ELLE_DEBUG("Created device id: %s", this->_device->id);
      }
      else
      {
        ELLE_DEBUG("Loading passport from '%s'.", passport_path);
        papier::Passport passport;
        passport.load(elle::io::Path{passport_path});

        ELLE_DEBUG("Passport id: %s", passport.id());
        auto res = this->_meta.update_device(passport.id(), name);
        this->_device.reset(new Device{res.id, name});
        passport_string = res.passport;
      }

      papier::Passport passport;
      if (passport.Restore(passport_string) == elle::Status::Error)
        throw Exception(gap_wrong_passport, "Cannot load the passport");

      passport.store(elle::io::Path(passport_path));
    }
  }
}
