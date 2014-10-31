#include <surface/gap/User.hh>

namespace surface
{
  namespace gap
  {
    User::User(uint32_t id_,
               bool status_,
               std::string const& fullname_,
               std::string const& handle_,
               std::string const& meta_id_,
               bool deleted_,
               bool ghost_)
      : status(status_)
      , fullname(fullname_)
      , handle(handle_)
      , meta_id(meta_id_)
      , deleted(deleted_)
      , ghost(ghost_)
    {}

    User::~User() noexcept(true)
    {}

    void
    User::print(std::ostream& stream) const
    {
      stream << "User("
             << this->id << ", "
             << this->fullname << ", "
             << (this->status ? "on" : "off")
             << "line)";
    }

    Notification::Type User::type = NotificationType_NewSwagger;
  }
}
