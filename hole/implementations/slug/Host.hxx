#ifndef HOLE_IMPLEMENTATIONS_SLUG_HOST_HXX
# define HOLE_IMPLEMENTATIONS_SLUG_HOST_HXX

# include <hole/Passport.hh>

namespace hole
{
  namespace implementations
  {
    namespace slug
    {
      elle::Passport*
      Host::remote_passport() const
      {
        return this->_remote_passport.get();
      }

      void
      Host::remote_passport(elle::Passport const& p)
      {
        this->_remote_passport.reset(new elle::Passport{p});
      }

      void
      Host::remote_passport_reset()
      {
        this->_remote_passport.reset(nullptr);
      }
    }
  }
}

#endif
