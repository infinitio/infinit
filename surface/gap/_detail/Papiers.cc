#include <surface/gap/State.hh>

#include <common/common.hh>

#include <elle/os/path.hh>

#include <papier/Passport.hh>

#include <reactor/lockable.hh>

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
    State::device() const
    {
      ELLE_TRACE_METHOD("");

      reactor::Lock l{this->_device_mutex};

      if (!this->has_device())
        this->update_device("XXX");
      else if (this->_device == nullptr)
      {
        if (this->_passport != nullptr)
          ELLE_WARN("%s: a passport was already present: %s",
                    *this, *this->_passport);
        this->_passport.reset(new papier::Passport());

        this->_passport->load(
          elle::io::Path{common::infinit::passport_path(this->me().id)});

        this->_device.reset(
          new Device{this->_passport->id(), this->_passport->name()});

      }
      ELLE_ASSERT(this->_device != nullptr);
      return *this->_device;
    }

    void
    State::update_device(std::string const& name, bool force_create) const
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

      if (this->_passport != nullptr)
        ELLE_WARN("%s: a passport was already present: %s",
                  *this, *this->_passport);

      this->_passport.reset(new papier::Passport());

      if (this->_passport->Restore(passport_string) == elle::Status::Error)
        throw Exception(gap_wrong_passport, "Cannot load the passport");

      this->_passport->store(elle::io::Path(passport_path));
    }

    papier::Passport const&
    State::passport() const
    {
      reactor::Lock l{this->_passport_mutex};
      if (this->_passport == nullptr)
      {
        if (!this->has_device())
          this->device();
      }

      return *this->_passport;
    }
  }
}
