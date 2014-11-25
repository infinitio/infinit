#ifndef SURFACE_GAP_SEND_MACHINE_HH
# define SURFACE_GAP_SEND_MACHINE_HH

# include <surface/gap/TransactionMachine.hh>
# include <surface/gap/State.hh>

namespace surface
{
  namespace gap
  {
    class SendMachine:
      virtual public TransactionMachine
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef SendMachine Self;
      typedef TransactionMachine Super;
      typedef std::vector<std::string> Files;
    /*-------------.
    | Construction |
    `-------------*/
    protected:
      /// Constructor for sending device.
      SendMachine(Transaction& transaction,
                  uint32_t id,
                  Files files,
                  std::shared_ptr<Data> data);
      /// Constructor for another device.
      SendMachine(Transaction& transaction,
                  uint32_t id,
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
      typedef std::unordered_map<int, float> PlainProgressChunks;
      ELLE_ATTRIBUTE(PlainProgressChunks, plain_progress_chunks);

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
      /// List of files and/or directories as selected by the user.
      ELLE_ATTRIBUTE_R(Files, files);
      /// Computed total file size (only set if create_transaction was called)
      ELLE_ATTRIBUTE_RW(int64_t, total_size);
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
      /* If possible, copy _files to a safer place out of reach of the user,
      * and update _files accordingly.
      * Must be called *before* the first frete creation/snapshoting (where
      * _files is expanded to yield the definitive file list).
      */
      void
      try_mirroring_files(frete::Frete::FileSize total_size);
      virtual
      void
      cleanup () override;
      ELLE_ATTRIBUTE_R(bool, files_mirrored);

      virtual
      bool
      concerns_this_device() override;
    };
  }
}

#endif
