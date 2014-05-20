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
      LinkSendMachine(Transaction& transaction,
                      uint32_t id,
                      std::vector<std::string> files,
                      std::shared_ptr<Data> data);
      virtual
      ~LinkSendMachine();
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
      aws::Credentials
      _aws_credentials(bool regenerate) override;
    protected:
      virtual
      void
      _create_transaction() override;
      virtual
      void
      _finalize(infinit::oracles::Transaction::Status) override;
    private:
      void
      _upload();
      ELLE_ATTRIBUTE(reactor::fsm::State&, upload_state);
      ELLE_ATTRIBUTE(aws::Credentials, credentials);
    };
  }
}

#endif
