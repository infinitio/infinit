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
      ELLE_ATTRIBUTE_R(std::shared_ptr<Data>, data);

    /*---------------.
    | Implementation |
    `---------------*/
    public:
      virtual
      float
      progress() const override;
      virtual
      void
      transaction_status_update(
        infinit::oracles::Transaction::Status status) override;
    protected:
      virtual
      void
      _create_transaction() override;
    };
  }
}

#endif
