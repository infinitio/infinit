#include "../State.hh"

#include <elle/printf.hh>
#include <elle/format/json.hh>
#include <reactor/exception.hh>
#include <boost/python.hpp>

ELLE_LOG_COMPONENT("infinit.surface.gap.State");

namespace surface
{
  namespace gap
  {

    namespace json = elle::format::json;

    static
    std::string
    parse_python_exception()
    {
      namespace py = boost::python;

      PyObject* type_ptr = NULL;
      PyObject* value_ptr = NULL;
      PyObject* traceback_ptr = NULL;

      PyErr_Fetch(&type_ptr, &value_ptr, &traceback_ptr);

      std::string ret("Unfetchable Python error");

      if(type_ptr != NULL)
      {
        py::handle<> h_type(type_ptr);
        py::str type_pstr(h_type);
        py::extract<std::string> e_type_pstr(type_pstr);
        if(e_type_pstr.check())
          ret = e_type_pstr();
        else
          ret = "Unknown exception type";
      }

      if(value_ptr != NULL)
      {
        py::handle<> h_val(value_ptr);
        py::str a(h_val);
        py::extract<std::string> returned(a);
        if(returned.check())
          ret +=  ": " + returned();
        else
          ret += std::string(": Unparseable Python error: ");
      }

      if(traceback_ptr != NULL)
      {
        py::handle<> h_tb(traceback_ptr);
        py::object tb(py::import("traceback"));
        py::object fmt_tb(tb.attr("format_tb"));
        py::object tb_list(fmt_tb(h_tb));
        py::object tb_str(py::str("\n").join(tb_list));
        py::extract<std::string> returned(tb_str);
        if(returned.check())
          ret += ": " + returned();
        else
          ret += std::string(": Unparseable Python traceback");
      }
      return ret;
    }

    void
    State::call_error_handlers(gap_Status status,
                               std::string const& s,
                               std::string const& tid)
    {
      try
      {
        for (auto const& c: this->_error_handlers)
        {
          c(status, s, tid);
        }
      }
      catch (surface::gap::Exception const& e)
      {
        ELLE_WARN("error handlers: %s", e.what());
      }
      catch (boost::python::error_already_set const&)
      {
        std::string msg = parse_python_exception();
        ELLE_WARN("error handlers python: %s", msg);
      }
      catch (elle::Exception const& e)
      {
        ELLE_WARN("error handlers: %s", e.what());
        auto bt = e.backtrace();
        for (auto const& f: bt)
          ELLE_WARN("%s", f);
      }
      catch (...)
      {
        ELLE_ERR("error handlers: unknown error");
      }
    }

    size_t
    State::poll(size_t max)
    {
      if (!this->_trophonius)
        throw Exception{gap_error, "Trophonius is not connected"};

      size_t count = 0;
      while (count < max)
      {
        std::unique_ptr<Notification> notif{
          this->_trophonius->poll()
        };

        if (!notif)
          break;
        // Try to retrieve Transaction ID if possible
        std::string transaction_id = "";
        {
          if (notif->notification_type == NotificationType::transaction_status)
          {
            auto ptr = static_cast<TransactionStatusNotification*>(notif.get());
            transaction_id = ptr->transaction_id;
          }
          else if (notif->notification_type == NotificationType::transaction)
          {
            auto ptr = static_cast<TransactionNotification*>(notif.get());
            transaction_id = ptr->transaction.transaction_id;
          }
        }

        try
        {
          this->_handle_notification(*notif);
        }
        catch (surface::gap::Exception const& e)
        {
          ELLE_WARN("poll: %s: %s", notif->notification_type, e.what());
          call_error_handlers(e.code,
                              elle::sprintf("%s: %s",
                                            notif->notification_type,
                                            e.what()),
                              transaction_id);
          continue;
        }
        catch (boost::python::error_already_set const&)
        {
          std::string msg = parse_python_exception();
          ELLE_WARN("poll python: %s: %s", notif->notification_type, msg);
          call_error_handlers(gap_error,
                              elle::sprintf("%s: %s",
                                            notif->notification_type,
                                            msg),
                              transaction_id);
        }
        catch (elle::Exception const& e)
        {
          ELLE_WARN("Poll: %s: %s", notif->notification_type, e.what());
          auto bt = e.backtrace();
          for (auto const& f: bt)
            ELLE_WARN("%s", f);
          call_error_handlers(gap_error,
                              elle::sprintf("%s: %s",
                                            notif->notification_type,
                                            e.what()),
                              transaction_id);
          continue;
        }
        catch (...)
        {
          ELLE_ERR("poll: %s: unknown error", notif->notification_type);
          call_error_handlers(gap_unknown,
                              elle::sprintf("%s: unexpected error",
                                            notif->notification_type),
                              transaction_id);
          continue;
        }
        ++count;
      }

      return count;
    }

    static
    std::unique_ptr<Notification>
    _xxx_dict_to_notification(json::Dictionary const& d)
    {
      std::unique_ptr<Notification> res;
      NotificationType notification_type = (NotificationType) d["notification_type"].as_integer().value();

      std::unique_ptr<UserStatusNotification> user_status{
          new UserStatusNotification
      };

      std::unique_ptr<TransactionNotification> transaction{
          new TransactionNotification
      };
      std::unique_ptr<TransactionStatusNotification> transaction_status{
          new TransactionStatusNotification
      };
      std::unique_ptr<MessageNotification> message{
          new MessageNotification
      };

      switch (notification_type)
        {
        case NotificationType::user_status:
          user_status->user_id = d["user_id"].as_string();
          user_status->status = d["status"].as_integer();
          res = std::move(user_status);
          break;

        case NotificationType::transaction:
#define GET_TR_FIELD(f, type) \
          try { \
              ELLE_DEBUG("Get transaction field " #f);\
              transaction->transaction.f = d["transaction"][#f].as_ ## type ();\
          } catch (...) { \
              ELLE_ERR("Couldn't get field " #f);\
          } \
/**/

          GET_TR_FIELD(transaction_id, string);
          GET_TR_FIELD(sender_id, string);
          GET_TR_FIELD(sender_fullname, string);
          GET_TR_FIELD(sender_device_id, string);
          GET_TR_FIELD(recipient_id, string);
          GET_TR_FIELD(recipient_fullname, string);
          GET_TR_FIELD(recipient_device_id, string);
          GET_TR_FIELD(recipient_device_name, string);
          GET_TR_FIELD(network_id, string);
          GET_TR_FIELD(message, string);
          GET_TR_FIELD(first_filename, string);
          GET_TR_FIELD(files_count, integer);
          GET_TR_FIELD(total_size, integer);
          GET_TR_FIELD(is_directory, integer);
          GET_TR_FIELD(status, integer);
          res = std::move(transaction);
          break;

        case NotificationType::transaction_status:
          transaction_status->transaction_id = d["transaction_id"].as_string();
          transaction_status->status = d["status"].as_integer();
          res = std::move(transaction_status);
          break;

        case NotificationType::message:
          message->sender_id = d["sender_id"].as_string();
          message->message = d["message"].as_string();
          res = std::move(message);
          break;

        case NotificationType::connection_enabled:
          res.reset(new Notification);
          break;

        default:
          throw elle::Exception{
              elle::sprintf("Unknown notification type %s", notification_type)
          };
        }
      res->notification_type = notification_type;
      return res;
    }


    void
    State::pull_notifications(int count, int offset)
    {
      ELLE_TRACE("pull_notifications(%s, %s)", count, offset);

      if (count < 1)
        return;

      if (offset < 0)
        return;

      ELLE_DEBUG("Pulling from meta");
      auto res = this->_meta->pull_notifications(count, offset);

      // Handle old notif first to act like a queue.
      for (auto& dict : res.old_notifs)
        {
          ELLE_DEBUG("Handling old notif %s", dict.repr());
          try {
            this->_handle_notification(*_xxx_dict_to_notification(dict), false);
          } catch (std::bad_cast const&) {
              ELLE_ERR("COULDN'T CAST: %s", dict.repr());
          } catch (std::ios_base::failure const&) {
              ELLE_ERR(" IOS FAILURE: %s", dict.repr());
          } catch (std::exception const& e) {
              ELLE_ERR(" EXCEPTIOJN: %s: %s", dict.repr(), e.what());
          } catch (...) {
              ELLE_ERR("COULDN'T HANDle: %s", dict.repr());
          }
        }

      for (auto& dict: res.notifs)
        {
          ELLE_DEBUG("Handling new notif %s", dict.repr());
          try {
            this->_handle_notification(*_xxx_dict_to_notification(dict), true);
          } catch (std::bad_cast const&) {
              ELLE_ERR("COULDN'T CAST: %s", dict.repr());
          } catch (std::ios_base::failure const&) {
              ELLE_ERR(" IOS FAILURE: %s", dict.repr());
          } catch (std::exception const& e) {
              ELLE_ERR(" EXCEPTIOJN: %s: %s", dict.repr(), e.what());
          } catch (...) {
              ELLE_ERR("COULDN'T HANDle: %s", dict.repr());
          }

        }
    }

    void
    State::notifications_read()
    {
      this->_meta->notification_read();
    }

    void
    State::_handle_notification(Notification const& notif,
                                bool new_)
    {
      // Connexion established.
      if (notif.notification_type == NotificationType::connection_enabled)
        // XXX set _connection_enabled to true
        return;

      auto handler_list = _notification_handlers.find(notif.notification_type);

      if (handler_list == _notification_handlers.end())
        {
          ELLE_DEBUG("Handler missing for notification '%u'",
                     notif.notification_type);
          return;
        }

      for (auto& handler: handler_list->second)
        {
          ELLE_ASSERT(handler != nullptr);
          handler(notif, new_);
        }
    }
  }
}
