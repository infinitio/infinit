#include <nucleus/proton/MutableBlock.hh>
#include <nucleus/proton/History.hh>
#include <nucleus/proton/Revision.hh>
#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Network.hh>

#include <boost/lexical_cast.hpp>

namespace nucleus
{
  namespace proton
  {

//
// ---------- constructs & destructors ----------------------------------------
//

    MutableBlock::MutableBlock():
      Block()
    {
    }

    MutableBlock::MutableBlock(Network const network,
                               Family const family,
                               neutron::Component const component,
                               cryptography::PublicKey const& creator_K):
      Block(network, family, component, creator_K),

      _revision(Revision::First)
    {
    }

    ELLE_SERIALIZE_CONSTRUCT_DEFINE(MutableBlock, Block)
    {
    }

//
// ---------- methods ---------------------------------------------------------
//

    elle::Boolean
    MutableBlock::derives(MutableBlock const& other) const
    {
      return (this->_revision > other._revision);
    }

//
// ---------- dumpable --------------------------------------------------------
//

    elle::Status
    MutableBlock::Dump(const elle::Natural32 margin) const
    {
      elle::String      alignment(margin, ' ');

      std::cout << alignment << "[MutableBlock]" << std::endl;

      if (Block::Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the parent");

      if (this->_revision.Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the revision");

      if (this->_base.Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the revision");

      return elle::Status::Ok;
    }

//
// ---------- printable -------------------------------------------------------
//

    void
    MutableBlock::print(std::ostream& stream) const
    {
      stream << "mutable block{"
             << this->_revision
             << "}";
    }
  }
}
