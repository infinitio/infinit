#ifndef SURFACE_GAP_TRANSACTIONSTATEMACHINE_HH
# define SURFACE_GAP_TRANSACTIONSTATEMACHINE_HH

# include "Device.hh"
# include "Self.hh"
# include "Transaction.hh"

# include <elle/attribute.hh>

# include <functional>

namespace surface
{
  namespace gap
  {
    class TransactionStateMachine
    {
    public:
      typedef std::function<void(Transaction const&)> CanceledCallback;
      typedef std::function<void(Transaction const&)> CleanCallback;
      typedef std::function<void(Transaction const&)> PrepareUploadCallback;
      typedef std::function<void(Transaction const&)> StartUploadCallback;
      typedef std::function<void(Transaction const&)> StartDownloadCallback;
      typedef
        std::function<bool(std::string const&, std::string const&)>
        DeviceStatusCallback;

    private:
      ELLE_ATTRIBUTE(CanceledCallback, canceled);
      ELLE_ATTRIBUTE(CleanCallback, clean);
      ELLE_ATTRIBUTE(PrepareUploadCallback, prepare_upload);
      ELLE_ATTRIBUTE(StartUploadCallback, start_upload);
      ELLE_ATTRIBUTE(StartDownloadCallback, start_download);
      ELLE_ATTRIBUTE(DeviceStatusCallback, device_status);
      ELLE_ATTRIBUTE(Self, self);
      ELLE_ATTRIBUTE(Device, device);

    public:
      TransactionStateMachine(CanceledCallback canceled,
                              CleanCallback clean,
                              PrepareUploadCallback prepare_upload,
                              StartUploadCallback start_upload,
                              StartDownloadCallback start_download,
                              DeviceStatusCallback device_status,
                              Self self,
                              Device device);

      void
      operator ()(Transaction const& transaction);
    };
  }
}

#endif
