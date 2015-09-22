#include <surface/gap/State.hh>

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
    State::update_device(boost::optional<std::string> name,
                         boost::optional<std::string> model,
                         boost::optional<std::string> os) const
    {
      ELLE_TRACE_METHOD(name);
      if (!name && !model && !os)
      {
        ELLE_DEBUG("%s: neither name, model nor OS are present", *this);
        return;
      }

      ELLE_DEBUG("update device name to %s, model to %s, os to %s",
                 name, model, os);
      std::string passport_path = this->local_configuration().passport_path();

      ELLE_ASSERT(this->_device != nullptr);

      auto res = this->_meta.update_device(this->_device->id, name, model, os);
      this->_device.reset(new Device(res));
      auto passport_string = res.passport.get();

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

    void
    State::set_device_id(std::string const& device_id)
    {
      if (this->logged_in_to_meta())
      {
        ELLE_WARN("%s: not able to change device id when already logged in",
                  *this);
        return;
      }
      boost::uuids::uuid old_device_id = this->_device_uuid;
      ELLE_LOG("%s: change device_id from %s to %s", *this,
               old_device_id, device_id);
      try
      {
        if (boost::lexical_cast<std::string>(old_device_id) == device_id)
          return;
        boost::uuids::uuid uuid =
          boost::lexical_cast<boost::uuids::uuid>(device_id);
        this->_device_uuid = uuid;
        infinit::metrics::Reporter::metric_device_id(
          boost::lexical_cast<std::string>(this->device_uuid()));
        std::string path = this->local_configuration().device_id_path();
        std::ofstream file(path);
        if (!file.good())
        {
          ELLE_ERR("%s: unable to create device.uuid at %s", *this, path);
          return;
        }
        file << this->device_uuid() << std::endl;
        if (this->metrics_reporter())
          this->metrics_reporter()->user_changed_device_id(
            boost::lexical_cast<std::string>(old_device_id));
      }
      catch (boost::bad_lexical_cast const&)
      {
        ELLE_ERR("%s: unable to set device_id, invalid id: %s",
                 *this, device_id);
      }
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
