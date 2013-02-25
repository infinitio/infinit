#ifndef ELLE_UTILITY_FACTORY_HXX
# define ELLE_UTILITY_FACTORY_HXX

# include <elle/Exception.hh>
# include <elle/log.hh>
# include <elle/finally.hh>

namespace elle
{
  namespace utility
  {
//
// ---------- Generatoid ------------------------------------------------------
//

    /*-------------.
    | Construction |
    `-------------*/

    template <typename P,
              typename... A>
    template <typename T>
    Factory<P, A...>::Generatoid<T>::Generatoid(P const& product):
      _product(product)
    {
    }

    /*--------.
    | Methods |
    `--------*/

    template <typename P,
              typename... A>
    template <typename T>
    void*
    Factory<P, A...>::Generatoid<T>::allocate(A... arguments) const
    {
      return (new T{arguments...});
    }

//
// ---------- Factory ---------------------------------------------------------
//

    /*-------------.
    | Construction |
    `-------------*/

    template <typename P,
              typename... A>
    Factory<P, A...>::Factory(Factory<P, A...>&& other):
      _container(std::move(other._container))
    {
    }

    template <typename P,
              typename... A>
    Factory<P, A...>::~Factory()
    {
      for (auto scoutor: this->_container)
        delete scoutor.second;

      this->_container.clear();
    }

    /*--------.
    | Methods |
    `--------*/

    template <typename P,
              typename... A>
    template <typename T>
    void
    Factory<P, A...>::record(P const& product)
    {
      ELLE_LOG_COMPONENT("elle.utility.Factory");
      ELLE_TRACE_METHOD(product);

      // Check if there is already such an product registerd.
      if (this->_container.find(product) != this->_container.end())
        throw Exception("unable to register an already registered product");

      // Create a generatoid.
      auto generatoid = new Factory<P, A...>::Generatoid<T>(product);

      ELLE_FINALLY_ACTION_DELETE(generatoid);

      // Insert the generator in the container.
      auto result =
        this->_container.insert(
          std::pair<P const, Factory<P, A...>::Functionoid*>(product,
                                                             generatoid));

      // Check if the insertion was successful.
      if (result.second == false)
        throw Exception("unable to insert the generatoid into the container");

      ELLE_FINALLY_ABORT(generatoid);
    }

    template <typename P,
              typename... A>
    template <typename T>
    T*
    Factory<P, A...>::allocate(P const& product,
                               A... arguments) const
    {
      ELLE_LOG_COMPONENT("elle.utility.Factory");
      ELLE_TRACE_METHOD(product);

      Factory<P, A...>::Scoutor scoutor;

      // Retrieve the associated generator.
      if ((scoutor = this->_container.find(product)) ==
          this->_container.end())
        throw Exception(elle::sprintf("unable to locate the generatoid for the "
                                      "given product '%s'", product));

      // Allocate and return the instance.
      return (reinterpret_cast<T*>(scoutor->second->allocate(arguments...)));
    }
  }
}

#endif
