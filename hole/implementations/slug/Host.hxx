#ifndef HOLE_IMPLEMENTATIONS_SLUG_HOST_HXX
# define HOLE_IMPLEMENTATIONS_SLUG_HOST_HXX

# include <papier/Passport.hh>

namespace hole
{
  namespace implementations
  {
    namespace slug
    {
      papier::Passport*
      Host::remote_passport() const
      {
        return this->_remote_passport.get();
      }

      void
      Host::remote_passport(papier::Passport const& p)
      {
        this->_remote_passport.reset(new papier::Passport{p});
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
