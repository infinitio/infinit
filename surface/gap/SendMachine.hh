#ifndef SURFACE_GAP_SEND_MACHINE_HH
# define SURFACE_GAP_SEND_MACHINE_HH

# include <surface/gap/TransactionMachine.hh>
# include <surface/gap/State.hh>

namespace surface
{
  namespace gap
  {
    class  SendMachine:
      virtual public TransactionMachine
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef SendMachine Self;
      typedef TransactionMachine Super;

    /*-------------.
    | Construction |
    `-------------*/
    protected:
      SendMachine(Transaction& transaction,
                  uint32_t id,
                  std::vector<std::string> files,
                  std::shared_ptr<Data> data);
    public:
      virtual
      ~SendMachine();

    /*-------------.
    | Plain upload |
    `-------------*/
    public:
      virtual
      float
      progress() const override;
    protected:
      // cleartext upload one file to cloud
      void
      _plain_upload();
      ELLE_ATTRIBUTE(float, plain_progress);

    /*----.
    | FSM |
    `----*/
    protected:
      virtual
      void
      _create_transaction() = 0;
      reactor::fsm::State& _create_transaction_state;

    /*-----------------.
    | Transaction data |
    `-----------------*/
    public:
      typedef std::vector<std::string> Files;
      ELLE_ATTRIBUTE_R(Files, files);

    public:
      virtual
      bool
      is_sender() const override
      {
        return true;
      }

      /// Return (name, is_an_archive) if we have to put all transfer data
      /// in one file
      std::pair<std::string, bool>
      archive_info();

    protected:
      virtual
      void cleanup () override;
    };
  }
}

#endif
