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

# include <frete/fwd.hh>
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
      typedef ::frete::Frete::FileID FileID;
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
      _cloud_operation() override;
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

    protected:
      void
      cleanup() override;

    private:
      std::map<boost::filesystem::path, boost::filesystem::path> _root_component_mapping;
      /* Transfer pipelining data
      */
      struct TransferData;
      struct IndexedBuffer
      {
        IndexedBuffer(elle::Buffer&& buf, FileSize pos, FileID index);
        IndexedBuffer(IndexedBuffer&& b);
        void operator = (IndexedBuffer && b);
        elle::Buffer buffer;
        FileSize start_position;
        FileID file_index;
        // *REVERSED* since priority queu returns top(max) element
        bool operator <(const IndexedBuffer& b) const;
      };
      // Fetcher threads will write blocks here, priority queue orders them
      reactor::Channel<IndexedBuffer, std::priority_queue<IndexedBuffer>> _buffers;
      /* disk_writer thread needs to be waken up only when the top block is
      *  the next one to write */
      reactor::Barrier _disk_writer_barrier;
      FileID   _store_expected_file;
      FileSize _store_expected_position;

      // Current state for fetcher threads
      FileID   _fetch_current_file_index;
      FileSize _fetch_current_position;
      FileSize _fetch_current_file_full_size; // cached
      typedef std::unordered_map<FileID, std::unique_ptr<boost::filesystem::ofstream>> TransferDataMap;
      TransferDataMap _transfer_stream_map;
      /** Initialize transfer data for given file index
       *  @return start position or -1 for nothing to do at this index.
       */
      FileSize  _initialize_one(FileID index,
                                const std::string& file_name,
                                FileSize file_size,
                                const std::string& name_policy);
      /** Switch fetcher data to next file, returns false if nothing else to do
      *   Fills all _fetcher state in
      */
      bool _fetch_next_file(const std::string& name_policy,
                            const std::vector<std::pair<std::string, FileSize>>& infos);
      template <typename Source>
      void
      _get(Source& source,
           bool strong_encryption,
           std::string const& name_policy,
           elle::Version const& peer_version);
      template <typename Source>
      void _disk_thread(Source& source,
                          elle::Version peer_version,
                          size_t chunk_size);
      template <typename Source>
      void _fetcher_thread(Source& source, int id,
                           const std::string& name_policy,
                           bool explicit_ack,
                           bool strong_encryption,
                           size_t chunk_size,
                           const infinit::cryptography::SecretKey& key);

    /*--------------.
    | Static Method |
    `--------------*/
    public:
      /* XXX: Exposed for debugging purposes.
       * root_component_mapping is a list of path names that we already
       * attributed a maping to (either themselves, or renamed)
       * It will be updated according to new mapping mades.
       */
      static
      boost::filesystem::path
      eligible_name(boost::filesystem::path start_point,
                    boost::filesystem::path path,
                    std::string const& name_policy,
                    std::map<boost::filesystem::path, boost::filesystem::path>& root_component_mapping);

      static
      boost::filesystem::path
      trim(boost::filesystem::path const& item,
           boost::filesystem::path const& root);
    };
  }
}

#endif
