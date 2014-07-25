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
      /// Constructor when sending from other device.
      LinkSendMachine(Transaction& transaction,
                      uint32_t id,
                      std::shared_ptr<Data> data);

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
      cancel() override;

    private:
      virtual
      void
      _fail() override;

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
      void
      _run_from_snapshot();
      ELLE_ATTRIBUTE(reactor::fsm::State&, upload_state);
      ELLE_ATTRIBUTE(boost::optional<aws::Credentials>, credentials);
    };
  }
}

#endif
