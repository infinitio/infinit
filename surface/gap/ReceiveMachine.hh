#ifndef RECEIVEMACHINE_HH
# define RECEIVEMACHINE_HH

# include <memory>
# include <string>
# include <unordered_set>

# include <boost/filesystem.hpp>
# include <boost/filesystem/fstream.hpp>

# include <elle/attribute.hh>

# include <reactor/Channel.hh>
# include <reactor/waitable.hh>
# include <reactor/signal.hh>

# include <frete/TransferSnapshot.hh>
# include <frete/Frete.hh>

# include <surface/gap/State.hh>
# include <surface/gap/TransactionMachine.hh>
# include <surface/gap/TransferBufferer.hh>

namespace surface
{
  namespace gap
  {
    struct ReceiveMachine:
      public TransactionMachine
    {

    public:
      typedef ::frete::Frete::FileSize FileSize;
      // Construct from notification.
      ReceiveMachine(surface::gap::State const& state,
                     uint32_t id,
                     std::shared_ptr<TransactionMachine::Data> data);

      // Construct from snapshot (with current_state).
      ReceiveMachine(surface::gap::State const& state,
                     uint32_t id,
                     TransactionMachine::State const current_state,
                     std::shared_ptr<TransactionMachine::Data> data);
      virtual
      ~ReceiveMachine();

      virtual
      void
      transaction_status_update(infinit::oracles::Transaction::Status status) override;

    public:
      void
      accept();

      void
      reject();

    private:
      ReceiveMachine(surface::gap::State const& state,
                     uint32_t id,
                     std::shared_ptr<TransactionMachine::Data> data,
                     bool);

    private:
      void
      _wait_for_decision();

      void
      _accept();

      void
      _transfer_operation(frete::RPCFrete& frete) override;

      void
      _fail();

      /*-----------------------.
      | Machine implementation |
      `-----------------------*/
      ELLE_ATTRIBUTE(reactor::fsm::State&, wait_for_decision_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, accept_state);

      // Transaction status signals.
      ELLE_ATTRIBUTE(reactor::Barrier, accepted);

      /*-----------------.
      | Transaction data |
      `-----------------*/
      ELLE_ATTRIBUTE(std::string, recipient);
      ELLE_ATTRIBUTE(std::unordered_set<std::string>, files);
      ELLE_ATTRIBUTE(boost::filesystem::path, snapshot_path);
      ELLE_ATTRIBUTE_R(std::unique_ptr<frete::TransferSnapshot>, snapshot)
    private:
      std::unique_ptr<frete::RPCFrete>
      rpcs(infinit::protocol::ChanneledStream& channels) override;

    public:
      float
      progress() const override;

    public:
      virtual
      bool
      is_sender() const override
      {
        return false;
      }

    /*----------.
    | Printable |
    `----------*/
    public:
      std::string
      type() const override;

    /*---------.
    | Transfer |
    `---------*/
    public:
      void
      get(frete::RPCFrete& frete,
          std::string const& name_policy = " (%s)");
      void
      get(TransferBufferer& bufferer,
          std::string const& name_policy = " (%s)");

    private:
      /* Transfer pipelining data
      */
      // next file for which we'll request data
      ELLE_ATTRIBUTE(size_t, next_file);
      struct TransferData
      {
        TransferData(
          frete::TransferSnapshot::TransferProgressInfo&,
          boost::filesystem::path full_path,
          FileSize current_position = 0);
        frete::TransferSnapshot::TransferProgressInfo& tr;
        boost::filesystem::path full_path;
        FileSize start_position; // next expected recieve buffer pos
        boost::filesystem::ofstream output;
      };
      struct IndexedBuffer{
        elle::Buffer buffer;
        FileSize start_position;
        size_t file_index;
      };
      reactor::Channel<IndexedBuffer> _buffers;

      typedef std::unique_ptr<TransferData> TransferDataPtr;
      typedef std::unordered_map<size_t, TransferDataPtr> TransferDataMap;
      ELLE_ATTRIBUTE(TransferDataMap, transfer_data_map);
      template <typename Source>
      void
      _get(Source& source,
           bool strong_encryption,
           std::string const& name_policy,
           elle::Version const& peer_version);
      template <typename Source>
      void _reader_thread(Source& source,
                          elle::Version peer_version,
                          size_t chunk_size);
      TransferDataPtr _initialize_one(size_t index,
                                      const std::string& file_name,
                                      FileSize file_size,
                                      boost::filesystem::path output_path,
                                      const std::string& name_policy);

    /*--------------.
    | Static Method |
    `--------------*/
    public:
      // XXX: Exposed for debugging purposes.
      static
      boost::filesystem::path
      eligible_name(boost::filesystem::path const path,
                    std::string const& name_policy);

      static
      boost::filesystem::path
      trim(boost::filesystem::path const& item,
           boost::filesystem::path const& root);
    };
  }
}

#endif
