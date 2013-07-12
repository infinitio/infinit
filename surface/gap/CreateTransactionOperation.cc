#include "CreateTransactionOperation.hh"

#include <common/common.hh>

#include <elle/os/file.hh>
#include <elle/os/getenv.hh>

#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>

ELLE_LOG_COMPONENT("infinit.surface.gap.CreateTransactionOperation");

namespace surface
{
  namespace gap
  {
    namespace fs = boost::filesystem;

    static
    size_t
    total_size(std::unordered_set<std::string> const& files)
    {
      ELLE_TRACE_FUNCTION(files);

      size_t size = 0;
      {
        for (auto const& file: files)
        {
          auto _size = elle::os::file::size(file);
          ELLE_DEBUG("%s: %i", file, _size);
          size += _size;
        }
      }
      return size;
    }

    CreateTransactionOperation::CreateTransactionOperation(
        TransactionManager& transaction_manager,
        NetworkManager& network_manager,
        UserManager& user_manager,
        plasma::meta::Client& meta,
        metrics::Reporter& reporter,
        plasma::meta::SelfResponse& me,
        std::string const& device_id,
        std::string const& recipient_id_or_email,
        std::unordered_set<std::string> const& files,
        std::function<void(std::string const&)> cb):
      Operation{"create_transaction_"},
      _transaction_manager(transaction_manager),
      _network_manager(network_manager),
      _user_manager(user_manager),
      _reporter(reporter),
      _meta(meta),
      _me(me),
      _device_id(device_id),
      _recipient_id_or_email{recipient_id_or_email},
      _files{files},
      _on_transaction{cb}
    {
      ELLE_TRACE_METHOD("");
    }

    void
    CreateTransactionOperation::_run()
    {
      ELLE_TRACE_METHOD("");

      ELLE_TRACE("files: %s", this->_files);

      int size = total_size(this->_files);

      std::string first_file =
        fs::path(*(this->_files.cbegin())).filename().string();
      elle::utility::Time time; time.Current();
      std::string network_name = elle::sprintf("%s-%s",
                                               this->_recipient_id_or_email,
                                               time.nanoseconds);
      this->_network_id = this->_network_manager.create(network_name);
      // XXX add locally

      // Preparing the network before sending the notification ensures that the
      // recipient can't prepare it by himself.
      this->_network_manager.prepare(this->_network_id);
      this->_network_manager.to_directory(
        this->_network_id,
        common::infinit::network_shelter(this->_me.id,
                                         this->_network_id));

      plasma::meta::CreateTransactionResponse res;
      ELLE_DEBUG("(%s): (%s) %s [%s] -> %s throught %s (%s)",
                 this->_device_id,
                 this->_files.size(),
                 first_file,
                 size,
                 this->_recipient_id_or_email,
                 network_name,
                 this->_network_id);

      try
      {
        res = this->_meta.create_transaction(this->_recipient_id_or_email,
                                             first_file,
                                             this->_files.size(),
                                             size,
                                             fs::is_directory(first_file),
                                             this->_network_id,
                                             this->_device_id);
        // Creating a transaction ensures that user has an id.
        auto recipient = this->_user_manager.one(this->_recipient_id_or_email);
        ELLE_TRACE("add user %s to network %s", recipient, this->_network_id)
          this->_meta.network_add_user(this->_network_id, recipient.id);
      }
      catch (...)
      {
        ELLE_DEBUG("transaction creation failed: %s",
                   elle::exception_string());
        // Something went wrong, we need to destroy the network.
        this->_network_manager.delete_(this->_network_id, false);
        throw;
      }
      this->_transaction_id = res.created_transaction_id;
      this->_name += this->_transaction_id;
      this->_me.remaining_invitations = res.remaining_invitations;

      if (this->_on_transaction != nullptr)
        this->_on_transaction(this->_transaction_id);
    }

    void
    CreateTransactionOperation::_cancel()
    {
      ELLE_TRACE_METHOD("");

      ELLE_DEBUG("cancelling %s", this->name());
      if (this->_transaction_id.size() > 0)
      {
        this->_transaction_manager.update(this->_transaction_id,
                                          plasma::TransactionStatus::canceled);
      }
    }
  }
}
