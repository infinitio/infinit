#ifndef HOLE_IMPLEMENTATIONS_SLUG_HOST_HXX
# define HOLE_IMPLEMENTATIONS_SLUG_HOST_HXX

# include <hole/Passport.hh>

namespace hole
{
  namespace implementations
  {
    namespace slug
    {
      Passport const&
      Host::remote_passport() const
      {
        return *this->_remote_passport;
      }

      void
      Host::remote_passport(Passport const& p)
      {
        this->_remote_passport.reset(new Passport{p});
      }
    }
  }
}

#endif
