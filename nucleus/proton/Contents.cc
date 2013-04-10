#include <nucleus/proton/Contents.hh>
#include <nucleus/proton/Node.hh>
#include <nucleus/proton/Quill.hh>
#include <nucleus/proton/Seam.hh>
#include <nucleus/neutron/Component.hh>
#include <nucleus/neutron/Catalog.hh>
#include <nucleus/Exception.hh>
#include <nucleus/factory.hh>

#include <elle/log.hh>

ELLE_LOG_COMPONENT("infinit.nucleus.proton.Contents");

namespace nucleus
{
  namespace proton
  {
    /*----------.
    | Constants |
    `----------*/

    neutron::Component const Contents::Constants::component{
      neutron::ComponentContents};

    /*-------------.
    | Construction |
    `-------------*/

    Contents::Contents():
      proton::ContentHashBlock(),

      _node(nullptr),
      _code(nullptr)
    {
    }

    Contents::~Contents()
    {
      delete this->_node;
      delete this->_code;
    }

    /*--------.
    | Methods |
    `--------*/

    void
    Contents::encrypt(cryptography::SecretKey const& key)
    {
      ELLE_TRACE_METHOD(key);

      ELLE_ASSERT(this->_node != nullptr);

      // Delete the previous code: the contents may have been deserialized
      // then modified and re-encrypted.
      delete this->_code;
      this->_code = nullptr;
      this->_code = new cryptography::Code{
        key.encrypt(*this->_node)};
    }

    void
    Contents::decrypt(cryptography::SecretKey const& key)
    {
      ELLE_TRACE_METHOD(key);

      ELLE_ASSERT(this->_node == nullptr);
      ELLE_ASSERT(this->_code != nullptr);

      ELLE_TRACE("decrypting the code with the secret key");

      this->_node = factory::node().allocate<Node>(this->_nature);
      // XXX[below, the decrypt<T> decrypts an archive and then constructs
      //     a T through the deserialize constructor. everything is good
      //     except that an object is still instanciated through the
      //     default constructor and we do not want that. we could pass a
      //     lambda which, in this case, would call key.decrypt() and return
      //     a T which would be used at the construction by the factory.
      //     another way would be for decrypt to return a pointer but this
      //     is less generic.
      //     unfortunately, the problem here is that we do not know the final
      //     type. thus, we cannot call decrypt<T> because do not know T.
      //     for deserialization the final type, can have no choice but to do
      //     it directly in the factory. therefore, a specific nucleus factory
      //     could be the solution.
      //     for example, the block factory would take a single argument: the
      //     archive while the node factory would take a code and a key
      //     which would be decrypted.]
      key.decrypt(*this->_code, *this->_node);
    }

    Mode
    Contents::mode() const
    {
      return ((this->_node != nullptr) ? Mode::decrypted : Mode::encrypted);
    }

    Node const&
    Contents::node() const
    {
      ELLE_ASSERT(this->_node != nullptr);

      return (*this->_node);
    }

    Node&
    Contents::node()
    {
      ELLE_ASSERT(this->_node != nullptr);

      return (*this->_node);
    }

    Node*
    Contents::cede()
    {
      ELLE_ASSERT(this->_node != nullptr);

      Node* node = this->_node;
      this->_node = nullptr;

      return (node);
    }

    Footprint
    Contents::footprint() const
    {
      // If the node as been ceded, return 0. Otherwise, return
      // the node's footprint.
      if (this->_node == nullptr)
        return (0);

      return (this->_node->footprint());
    }

    /*---------.
    | Dumpable |
    `---------*/

    elle::Status
    Contents::Dump(elle::Natural32          margin) const
    {
      elle::String      alignment(margin, ' ');

      std::cout << alignment << "[Contents] " << this << std::endl;

      if (proton::ContentHashBlock::Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the underlying block");

      if (this->_node != nullptr)
        {
          std::cout << alignment << elle::io::Dumpable::Shift
                    << "[Node]" << std::endl;

          this->_node->Dump(margin + 4);
        }
      else
        std::cout << alignment << elle::io::Dumpable::Shift
                  << "[Node] none" << std::endl;

      if (this->_code != nullptr)
        {
          std::cout << alignment << elle::io::Dumpable::Shift
                    << "[Code] " << *this->_code << std::endl;
        }
      else
        std::cout << alignment << elle::io::Dumpable::Shift
                  << "[Code] none" << std::endl;

      return elle::Status::Ok;
    }

    /*----------.
    | Printable |
    `----------*/

    void
    Contents::print(std::ostream& stream) const
    {
      stream << this->_nature;

      if (this->_node != nullptr)
        stream << "(" << *this->_node << ")";
    }
  }
}
