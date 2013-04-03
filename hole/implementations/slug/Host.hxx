#pragma once
#ifndef HOST_O9N6CLMS
#define HOST_O9N6CLMS

namespace hole
{
  namespace implementations
  {
    namespace slug
    {
      elle::Passport const&
      Host::remote_passport() const
      {
          return *this->_remote_passport;
      }

      void
      Host::remote_passport(elle::Passport const& p)
      {
          this->_remote_passport.reset(new elle::Passport{p});
      }
    }
  }
}

#endif /* end of include guard: HOST_O9N6CLMS */
