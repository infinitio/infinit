#ifndef SURFACE_GAP_SEND_MACHINE_HH
# define SURFACE_GAP_SEND_MACHINE_HH

# include <surface/gap/TransactionMachine.hh>
# include <surface/gap/State.hh>

namespace surface
{
  namespace gap
  {
    class  SendMachine:
      public TransactionMachine
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

    protected:
      virtual
      void
      _create_transaction() = 0;
      // cleartext upload one file to cloud
      void
      _ghost_cloud_upload();

    /*-----------------------.
    | Machine implementation |
    `-----------------------*/
    public:
      virtual
      void
      notify_user_connection_status(std::string const& user_id,
                                    std::string const& device_id,
                                    bool online) override;
    protected:
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

    protected:
      virtual
      void cleanup () override;
    };
  }
}

#endif
