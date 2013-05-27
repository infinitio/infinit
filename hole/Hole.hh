#ifndef HOLE_HOLE_HH
# define HOLE_HOLE_HH

# include <boost/signals.hpp>

# include <elle/attribute.hh>
# include <elle/types.hh>

# include <nucleus/proton/fwd.hh>

# include <hole/Passport.hh>

# include <hole/fwd.hh>
# include <hole/storage/Storage.hh>

namespace hole
{
  /// The storage layer of an Infinit filesystem.
  class Hole
  {
  /*-------------.
  | Construction |
  `-------------*/
  public:
    Hole(storage::Storage& storage,
         elle::Passport const& passport,
         elle::Authority const& authority);
    virtual ~Hole();

  /*--------.
  | Storage |
  `--------*/
  public:
    /// Store a block.
    void
    push(const nucleus::proton::Address& address,
         const nucleus::proton::Block& block);
    /// Retreive a block.
    std::unique_ptr<nucleus::proton::Block>
    pull(const nucleus::proton::Address& address,
         const nucleus::proton::Revision& revision);
    /// Remove a block.
    void
    wipe(const nucleus::proton::Address& address);
  private:
    ELLE_ATTRIBUTE_RX(storage::Storage&, storage);

  /*---------------.
  | Implementation |
  `---------------*/
  protected:
    virtual
    void
    _push(const nucleus::proton::Address& address,
         const nucleus::proton::ImmutableBlock& block) = 0;
    virtual
    void
    _push(const nucleus::proton::Address& address,
         const nucleus::proton::MutableBlock& block) = 0;
    virtual
    std::unique_ptr<nucleus::proton::Block>
    _pull(const nucleus::proton::Address&) = 0;
    virtual
    std::unique_ptr<nucleus::proton::Block>
    _pull(const nucleus::proton::Address&, const nucleus::proton::Revision&) = 0;
    virtual
    void
    _wipe(const nucleus::proton::Address& address) = 0;

  /*-----------.
  | Attributes |
  `-----------*/
  private:
    ELLE_ATTRIBUTE_R(elle::Passport, passport);
    ELLE_ATTRIBUTE_R(elle::Authority, authority);
  };
}

#endif
