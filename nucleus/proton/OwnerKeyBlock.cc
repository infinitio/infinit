#include <nucleus/proton/OwnerKeyBlock.hh>
#include <nucleus/proton/Family.hh>
#include <nucleus/proton/Network.hh>
#include <nucleus/proton/Address.hh>

#include <cryptography/KeyPair.hh>

#include <elle/log.hh>

ELLE_LOG_COMPONENT("infinit.nucleus.proton.OwnerKeyBlock");

namespace nucleus
{
  namespace proton
  {

    /*----------.
    | Constants |
    `----------*/

    elle::Natural32 const OwnerKeyBlock::Constants::keypair_length{2048};

//
// ---------- construction ----------------------------------------------------
//

    OwnerKeyBlock::OwnerKeyBlock():
      MutableBlock(),

      _owner_subject(nullptr)
    {
    }

    OwnerKeyBlock::OwnerKeyBlock(
        Network const& network,
        neutron::Component const component,
        cryptography::PublicKey const& creator_K):
      OwnerKeyBlock(network, component, creator_K,
                    cryptography::KeyPair::generate(
                      cryptography::Cryptosystem::rsa,
                      OwnerKeyBlock::Constants::keypair_length))
    {
    }

    OwnerKeyBlock::OwnerKeyBlock(
        Network const& network,
        neutron::Component const component,
        cryptography::PublicKey const& creator_K,
        cryptography::KeyPair const& block_keypair):
      MutableBlock(network, Family::owner_key_block, component, creator_K),

      _block_K(block_keypair.K()),
      _owner_K(creator_K),
      _owner_signature(block_keypair.k().sign(this->_owner_K)),
      _owner_subject(nullptr)
    {
    }

    OwnerKeyBlock::~OwnerKeyBlock()
    {
      delete this->_owner_subject;
    }

//
// ---------- block -----------------------------------------------------------
//

    Address
    OwnerKeyBlock::bind() const
    {
      ELLE_TRACE_METHOD("");

      // Note that the address computation of an owner key block is similar
      // to the one of a pubilc key block: the block's public key K is hashed
      // while the creation timestamp and salt do not need to be included
      // because such a public key is believed to be unique.
      Address address(this->network(), this->family(), this->component(),
                      this->_block_K);

      return (address);
    }

    void
    OwnerKeyBlock::validate(Address const& address) const
    {
      ELLE_TRACE_METHOD(address);

      if ((this->network() != address.network()) ||
          (this->family() != address.family()) ||
          (this->component() != address.component()))
        throw Exception(elle::sprintf("the address %s does not seem to "
                                      "represent the given block", address));

      //
      // make sure the address has not be tampered and correspond to the
      // hash of the public key.
      //
      Address self(this->network(), this->family(), this->component(),
                   this->_block_K);

      // verify with the recorded address.
      if (address != self)
        throw Exception(
          elle::sprintf("the recorded address does not correspond "
                        "to this block: given(%s) versus "
                        "self(%s)", address, self));

      // verify the owner's key signature with the block's public key.
      if (this->_block_K.verify(this->_owner_signature,
                                this->_owner_K) == false)
        throw Exception("unable to verify the owner's signature");
    }

    neutron::Subject const&
    OwnerKeyBlock::owner_subject()
    {
      // Create the subject corresponding to the block owner, if necessary.
      // Note that this subject will never be serialized but is used to ease
      // the process of access control since most method require a subject.
      if (this->_owner_subject == nullptr)
        this->_owner_subject = new neutron::Subject(this->_owner_K);

      assert(this->_owner_subject != nullptr);

      return (*this->_owner_subject);
    }

//
// ---------- dumpable --------------------------------------------------------
//

    elle::Status
    OwnerKeyBlock::Dump(const elle::Natural32  margin) const
    {
      elle::String      alignment(margin, ' ');

      std::cout << alignment << "[OwnerKeyBlock]" << std::endl;

      // dump the parent class.
      if (MutableBlock::Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the underlying block");

      // dump the OKB's public key.
      std::cout << alignment << elle::io::Dumpable::Shift
                << "[Block K]" << this->_block_K << std::endl;

      // dump the owner part.
      std::cout << alignment << elle::io::Dumpable::Shift << "[Owner]"
                << std::endl;

      std::cout << alignment << elle::io::Dumpable::Shift
                << elle::io::Dumpable::Shift
                << "[K] " << this->_owner_K << std::endl;

      std::cout << alignment << elle::io::Dumpable::Shift
                << elle::io::Dumpable::Shift
                << "[Signature] " << this->_owner_signature << std::endl;

      if (this->_owner_subject != nullptr)
        {
          if (this->_owner_subject->Dump(margin + 6) == elle::Status::Error)
            throw Exception("unable to dump the subject");
        }

      return elle::Status::Ok;
    }

//
// ---------- printable -------------------------------------------------------
//

    void
    OwnerKeyBlock::print(std::ostream& stream) const
    {
      stream << "owner key block{"
             << this->_block_K
             << ", "
             << this->_owner_K
             << "}";
    }

  }
}
