#ifndef TRANSFERMACHINE_HH
# define TRANSFERMACHINE_HH

# include <lune/Identity.hh>
# include <lune/Lune.hh>

# include <etoile/Etoile.hh>

# include <hole/Passport.hh>
# include <hole/storage/Directory.hh>
# include <hole/implementations/slug/Slug.hh>

# include <nucleus/proton/Network.hh>

# include <plasma/meta/Client.hh>

# include <reactor/fsm.hh>
# include <reactor/network/Protocol.hh>
# include <reactor/scheduler.hh>
# include <reactor/thread.hh>
# include <reactor/waitable.hh>

# include <thread>

namespace surface
{
  namespace gap
  {
    class State;

    class TransferMachine
    {
    public:
      TransferMachine(surface::gap::State const& state);

      virtual
      ~TransferMachine() = 0;

    public:
      void
      run();

      /*-----------------------.
      | Machine implementation |
      `-----------------------*/
    private:
      ELLE_ATTRIBUTE(reactor::Scheduler, scheduler);
      ELLE_ATTRIBUTE(std::unique_ptr<std::thread>, scheduler_thread);

    protected:
      reactor::fsm::Machine _machine;
      std::unique_ptr<reactor::Thread> _machine_thread;

      ELLE_ATTRIBUTE_R(surface::gap::State const&, state);

      /*------------.
      | Transaction |
      `------------*/
      ELLE_ATTRIBUTE(std::string, transaction_id);
    protected:
      std::string const&
      transaction_id();

      void
      transaction_id(std::string const& id);

      /*--------.
      | Network |
      `--------*/
      ELLE_ATTRIBUTE(std::string, network_id);
    protected:
      std::string const&
      network_id();

      void
      network_id(std::string const& id);

      ELLE_ATTRIBUTE(std::unique_ptr<nucleus::proton::Network>, network);
    protected:
      nucleus::proton::Network&
      network();

      ELLE_ATTRIBUTE(std::unique_ptr<lune::Descriptor>, descriptor);
    protected:
      lune::Descriptor const&
      descriptor();

      /*-------.
      | Etoile |
      `-------*/
      ELLE_ATTRIBUTE(std::unique_ptr<hole::storage::Directory>, storage);
    protected:
      hole::storage::Directory&
      storage();

      ELLE_ATTRIBUTE(std::unique_ptr<hole::implementations::slug::Slug>, hole);

      hole::implementations::slug::Slug&
      hole();

      ELLE_ATTRIBUTE(std::unique_ptr<etoile::Etoile>, etoile);
    protected:
      etoile::Etoile&
      etoile();
    };
  }
}

#endif
