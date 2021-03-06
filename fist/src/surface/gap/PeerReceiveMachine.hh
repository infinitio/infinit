#ifndef SURFACE_GAP_PEER_RECEIVE_MACHINE_HH
# define SURFACE_GAP_PEER_RECEIVE_MACHINE_HH

# include <memory>
# include <string>
# include <unordered_set>

# include <boost/filesystem.hpp>
# include <boost/filesystem/fstream.hpp>

# include <elle/attribute.hh>

# include <reactor/Channel.hh>
# include <reactor/waitable.hh>
# include <reactor/signal.hh>

# include <frete/Frete.hh>
# include <frete/fwd.hh>
# include <oracles/src/infinit/oracles/PeerTransaction.hh>
# include <surface/gap/PeerMachine.hh>
# include <surface/gap/ReceiveMachine.hh>
# include <surface/gap/TransferBufferer.hh>

namespace surface
{
  namespace gap
  {
    struct PeerReceiveMachine
      : public ReceiveMachine
      , public PeerMachine
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef infinit::oracles::PeerTransaction Data;
      typedef ::frete::Frete::FileSize FileSize;
      typedef ::frete::Frete::FileID FileID;
      typedef ::frete::Frete::FileInfo FileInfo;
      typedef ::frete::Frete::FilesInfo FilesInfo;
      typedef PeerReceiveMachine Self;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      PeerReceiveMachine(Transaction& transaction,
                         uint32_t id,
                         std::shared_ptr<Data> data);
      virtual
      ~PeerReceiveMachine();

    private:
      void
      _run_from_snapshot();

    public:
      virtual
      void
      accept(boost::optional<std::string> relative_output_dir = {}) override;
      virtual
      void
      reject() override;

    private:
      virtual
      infinit::oracles::meta::UpdatePeerTransactionResponse
      _accept() override;
      void
      _transfer_operation(frete::RPCFrete& frete) override;
      void
      _cloud_operation() override;
      void
      _cloud_synchronize() override;
      virtual
      void
      _wait_for_decision() override;

    /*-----------------------.
    | Machine implementation |
    `-----------------------*/
    public:
      virtual
      void
      notify_user_connection_status(std::string const& user_id,
                                    bool user_status,
                                    elle::UUID const& device_id,
                                    bool device_status) override;
    /*-----------------.
    | Transaction data |
    `-----------------*/
    public:
      ELLE_ATTRIBUTE(boost::filesystem::path, frete_snapshot_path);
      ELLE_ATTRIBUTE_R(std::unique_ptr<frete::TransferSnapshot>, snapshot)

    protected:
      void
      _save_frete_snapshot();
    private:
      std::unique_ptr<frete::RPCFrete>
      rpcs(infinit::protocol::ChanneledStream& channels) override;

    public:
      virtual
      float
      progress() const override;

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
      template <typename Source>
      void
      get(Source& source,
          EncryptionLevel level,
          std::string const& name_policy,
          elle::Version const& peer_version);
      virtual
      bool
      completed() const override;
      ELLE_ATTRIBUTE(bool, completed);
    protected:
      void
      cleanup() override;

    private:
      // If the accept response returns no credentials, we can deduce that the
      // sender didn't push anything in the cloud.
      ELLE_ATTRIBUTE_R(bool, nothing_in_the_cloud);
      ELLE_ATTRIBUTE(boost::optional<elle::Version>, peer_version);
      ELLE_ATTRIBUTE(std::streamsize const, chunk_size);
      template <typename Source>
      elle::Version const&
      peer_version(Source& source);
      ELLE_ATTRIBUTE(boost::optional<frete::Frete::TransferInfo>, transfer_info);
      template <typename Source>
      frete::Frete::TransferInfo const&
      transfer_info(Source& source);
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
      typedef std::unordered_map<FileID, boost::filesystem::path> DestinationPathMap;
      DestinationPathMap _destination_files_map;
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
                            FilesInfo const& infos);
      template <typename Source>
      void _disk_thread(Source& source,
                          elle::Version peer_version,
                          size_t chunk_size);
      template <typename Source>
      void _fetcher_thread(Source& source, int id,
                           std::string const& name_policy,
                           bool explicit_ack,
                           EncryptionLevel encryption,
                           size_t chunk_size,
                           infinit::cryptography::SecretKey const& key,
                           FilesInfo const& infos
                           );

      // Transfer bufferer for cloud operations
       std::unique_ptr<TransferBufferer> _bufferer;
    };
  }
}

#endif
