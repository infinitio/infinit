#ifndef SURFACE_GAP_LINK_SEND_MACHINE_HH
# define SURFACE_GAP_LINK_SEND_MACHINE_HH

# include <oracles/src/infinit/oracles/LinkTransaction.hh>
# include <surface/gap/SendMachine.hh>

namespace surface
{
  namespace gap
  {
    class LinkSendMachine
      : public SendMachine
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef LinkSendMachine Self;
      typedef SendMachine Super;
      typedef infinit::oracles::LinkTransaction Data;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      /// Constructor when sending from other device or if you have no snapshot
      /// as sender. In that case, run_to_fail is set to true.
      LinkSendMachine(Transaction& transaction,
                      uint32_t id,
                      std::shared_ptr<Data> data,
                      bool run_to_fail = false);

      /// Constructor for sending device.
      LinkSendMachine(Transaction& transaction,
                      uint32_t id,
                      std::vector<std::string> files,
                      std::string const& message,
                      std::shared_ptr<Data> data);
      virtual
      ~LinkSendMachine();
      ELLE_ATTRIBUTE_R(std::string, message);
      ELLE_ATTRIBUTE_R(std::shared_ptr<Data>, data);
    /*---------------.
    | Implementation |
    `---------------*/
    public:
      virtual
      void
      transaction_status_update(
        infinit::oracles::Transaction::Status status) override;
      virtual
      std::unique_ptr<infinit::oracles::meta::CloudCredentials>
      _cloud_credentials(bool regenerate) override;
      virtual
      bool
      completed() const override;
    protected:
      virtual
      void
      _create_transaction() override;
      virtual
      void
      _initialize_transaction() override;
      virtual
      void
      _transfer() override;
      virtual
      void
      _update_meta_status(infinit::oracles::Transaction::Status) override;
    private:
      void
      _run_from_snapshot();
      ELLE_ATTRIBUTE(std::unique_ptr<infinit::oracles::meta::CloudCredentials>,
                     credentials);
      ELLE_ATTRIBUTE(bool, completed);
    };
  }
}

#endif
