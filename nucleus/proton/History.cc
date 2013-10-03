#include <nucleus/proton/History.hh>
#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Network.hh>

#include <elle/Buffer.hh>

namespace nucleus
{
  namespace proton
  {

    /*--------.
    | Methods |
    `--------*/

    elle::Status        History::Register(const Revision&        revision)
    {
      // store the revision in the history's vector.
      this->_container.push_back(revision);

      return elle::Status::Ok;
    }

    elle::Status        History::Select(const Revision::Type     index,
                                        Revision&                revision) const
    {
      // check if the index is out of bound.
      if (index >= this->_container.size())
        throw Exception("the revision index is out of bound");

      // return the revision.
      revision = this->_container[index];

      return elle::Status::Ok;
    }

    elle::Status        History::Size(Revision::Type&            size) const
    {
      // return the size.
      size = this->_container.size();

      return elle::Status::Ok;
    }

    /*----------.
    | Operators |
    `----------*/

    elle::Boolean
    History::operator ==(History const& other) const
    {
      Revision::Type size;
      Revision::Type i;

      // check the address as this may actually be the same object.
      if (this == &other)
        return true;

      // check the containers' size.
      if (this->_container.size() != other._container.size())
        return false;

      // retrieve the size.
      size = this->_container.size();

      // go through the container and compare.
      for (i = 0; i < size; i++)
        {
          // compare the containers.
          if (this->_container[i] != other._container[i])
            return false;
        }

      return true;
    }

    /*-----------.
    | Interfaces |
    `-----------*/

    elle::Status        History::Dump(elle::Natural32           margin) const
    {
      elle::String      alignment(margin, ' ');
      Revision::Type     i;

      // display the name.
      std::cout << alignment << "[History]" << std::endl;

      // go through the container.
      for (i = 0; i < this->_container.size(); i++)
        {
          Revision       revision;

          // display the entry.
          std::cout << alignment << elle::io::Dumpable::Shift
                    << "[Entry]" << std::endl;

          // display the index.
          std::cout << alignment << elle::io::Dumpable::Shift
                    << "[Index] " << i << std::endl;

          // retrieve the revision.
          revision = this->_container[i];

          // dump the revision.
          if (revision.Dump(margin + 4) == elle::Status::Error)
            throw Exception("unable to dump the revision");
        }

      return elle::Status::Ok;
    }

    void
    History::print(std::ostream& stream) const
    {
      stream << "history("
             << "#" << this->_container.size()
             << ")";
    }

  }
}
