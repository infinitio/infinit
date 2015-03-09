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
               bool swagger_,
               bool deleted_,
               bool ghost_,
               boost::optional<std::string> phone_number_,
               boost::optional<std::string> ghost_code_,
               boost::optional<std::string> ghost_invitation_url_)
      : id(id_)
      , status(status_)
      , fullname(fullname_)
      , handle(handle_)
      , meta_id(meta_id_)
      , swagger(swagger_)
      , deleted(deleted_)
      , ghost(ghost_)
      , phone_number(phone_number_ ? phone_number_.get() : "")
      , ghost_code(ghost_code_ ? ghost_code_.get() : "")
      , ghost_invitation_url(ghost_invitation_url_ ? ghost_invitation_url_.get()
                                                   : "")
    {}

    User::~User() noexcept(true)
    {}

    void
    User::print(std::ostream& stream) const
    {
      stream << "User("
             << this->id << ", "
             << this->fullname << ", "
             << (this->deleted ? "(deleted)" :
                  (this->ghost ? "(ghost)" : "(normal)"))
             << ", "
             << (this->status ? "on" : "off") << "line, "
             << (this->swagger ? "is swagger)" : ")");
    }

    Notification::Type User::type = NotificationType_NewSwagger;
  }
}
