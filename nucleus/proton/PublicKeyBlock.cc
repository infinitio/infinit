#include <nucleus/proton/PublicKeyBlock.hh>
#include <nucleus/proton/Family.hh>
#include <nucleus/proton/Address.hh>
#include <nucleus/Exception.hh>

namespace nucleus
{
  namespace proton
  {

//
// ---------- constructors & destructors --------------------------------------
//

    PublicKeyBlock::PublicKeyBlock():
      MutableBlock()
    {
    }

    PublicKeyBlock::PublicKeyBlock(
        Network const& network,
        neutron::Component const component,
        cryptography::PublicKey const& creator_K,
        cryptography::PublicKey const& block_K):
      MutableBlock(network, Family::public_key_block, component, creator_K),

      _block_K(block_K)
    {
    }

//
// ---------- block -----------------------------------------------------------
//

    Address
    PublicKeyBlock::bind() const
    {
      // The computation of the address simply consists in hashing the public
      // key of the block.
      //
      // Note that the address computation of a public key block does not
      // require the inclusion of the creation timestamp and salt as it is
      // for imprint blocks for example because the block's public key K
      // is supposed to be unique since it is though extremely unlikely to
      // generate the same public keys.
      Address address(this->network(), this->family(), this->component(),
                      this->_block_K);

      return (address);
    }

    void
    PublicKeyBlock::validate(Address const& address) const
    {
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

      //
      // at this point the node knows that the recorded address corresponds
      // to the recorded public key and identifies the block requested.
      //
    }

//
// ---------- dumpable --------------------------------------------------------
//

    ///
    /// this function dumps an block object.
    ///
    elle::Status        PublicKeyBlock::Dump(const
                                               elle::Natural32  margin) const
    {
      elle::String      alignment(margin, ' ');

      std::cout << alignment << "[PublicKeyBlock]" << std::endl;

      // dump the parent class.
      if (MutableBlock::Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the underlying block");

      // dump the PKB's public key.
      std::cout << alignment << elle::io::Dumpable::Shift << "[K] "
                << this->_block_K << std::endl;

      return elle::Status::Ok;
    }

//
// ---------- printable -------------------------------------------------------
//

    void
    PublicKeyBlock::print(std::ostream& stream) const
    {
      stream << "public key block{"
             << this->_block_K
             << "}";
    }

  }
}
