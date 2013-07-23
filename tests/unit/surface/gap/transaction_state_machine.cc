
#include <surface/gap/TransactionStateMachine.hh>

#define BOOST_TEST_MODULE gap
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <functional>

using namespace surface::gap;

namespace
{
  class TestCase
  {
  private:
    ELLE_ATTRIBUTE_R(bool, is_canceled);
    ELLE_ATTRIBUTE_R(bool, is_failed);
    ELLE_ATTRIBUTE_R(bool, has_cleaned);
    ELLE_ATTRIBUTE_R(bool, has_prepared_upload);
    ELLE_ATTRIBUTE_R(bool, has_started_upload);
    ELLE_ATTRIBUTE_R(bool, has_started_download);
    ELLE_ATTRIBUTE(Self, self);
    ELLE_ATTRIBUTE(Device, device);
    ELLE_ATTRIBUTE(TransactionStateMachine, state_machine);

  public:
    TestCase(Transaction const& tr,
             bool const is_sender = true):
      _is_canceled{false},
      _has_cleaned{false},
      _has_prepared_upload{false},
      _has_started_upload{false},
      _has_started_download{false},
      _self{_make_self(is_sender)},
      _device(_make_device(is_sender)),
      _state_machine{
        std::bind(&TestCase::_canceled,
                  this,
                  std::placeholders::_1),
        std::bind(&TestCase::_failed,
                  this,
                  std::placeholders::_1),
        std::bind(&TestCase::_clean,
                  this,
                  std::placeholders::_1),
        std::bind(&TestCase::_prepare_upload,
                  this,
                  std::placeholders::_1),
        std::bind(&TestCase::_start_upload,
                  this,
                  std::placeholders::_1),
        std::bind(&TestCase::_start_download,
                  this,
                  std::placeholders::_1),
        std::bind(&TestCase::_device_status,
                  this,
                  std::placeholders::_1,
                  std::placeholders::_2),
        std::bind(&TestCase::_self, this),
        std::bind(&TestCase::_device, this),
      }
    {
      this->_state_machine(tr);
    }

  protected:
    virtual
    void
    _canceled(Transaction const&)
    {
      this->_is_canceled = true;
    }

    virtual
    void
    _failed(Transaction const&)
    {
      this->_is_failed = true;
    }

    virtual
    void
    _clean(Transaction const&)
    {
      this->_has_cleaned = true;
    }

    virtual
    void
    _prepare_upload(Transaction const&)
    {
      this->_has_prepared_upload = true;
    }

    virtual
    void
    _start_upload(Transaction const&)
    {
      this->_has_started_upload = true;
    }

    virtual
    void
    _start_download(Transaction const&)
    {
      this->_has_started_download = true;
    }

    virtual
    bool
    _device_status(std::string const& /*user_id*/,
                   std::string const& /*device_id*/)
    {
      return true;
    }

    Self const&
    _get_self() const
    {
      return this->_self;
    }

    Device const&
    _get_device() const
    {
      return this->_device;
    }

    virtual
    Self
    _make_self(bool const is_sender) const
    {
      std::string prefix = (is_sender ? "sender" : "recipient");
      Self self;
      self.id = prefix + "_user_id";
      self.connected_devices.push_back(prefix + "_device_id");
      return self;
    }

    virtual
    Device
    _make_device(bool const is_sender) const
    {
      std::string prefix = (is_sender ? "sender" : "recipient");
      return Device{prefix + "_device_id", prefix + " device name", ""};
    }
  };

  Transaction
  make_transaction(TransactionStatus const status)
  {
    Transaction tr;
    tr.status = status;
    tr.sender_id = "sender_user_id";
    tr.sender_device_id = "sender_device_id";
    tr.recipient_id = "recipient_user_id";
    tr.recipient_device_id = "recipient_device_id";
    tr.accepted = false;
    return tr;
  }
}

BOOST_AUTO_TEST_CASE(created)
{
  auto tr = make_transaction(TransactionStatus::created);
  TestCase test(tr);
  BOOST_CHECK(not test.is_canceled());
  BOOST_CHECK(not test.is_failed());
  BOOST_CHECK(not test.has_cleaned());
  BOOST_CHECK(test.has_prepared_upload());
  BOOST_CHECK(not test.has_started_upload());
  BOOST_CHECK(not test.has_started_download());
}

BOOST_AUTO_TEST_CASE(created_accepted)
{
  auto tr = make_transaction(TransactionStatus::created);
  tr.accepted = true;
  TestCase test(tr);
  BOOST_CHECK(not test.is_canceled());
  BOOST_CHECK(not test.is_failed());
  BOOST_CHECK(not test.has_cleaned());
  BOOST_CHECK(test.has_prepared_upload());
  BOOST_CHECK(not test.has_started_upload());
  BOOST_CHECK(not test.has_started_download());
}

BOOST_AUTO_TEST_CASE(started)
{
  auto tr = make_transaction(TransactionStatus::started);
  TestCase test(tr);
  BOOST_CHECK(not test.is_canceled());
  BOOST_CHECK(not test.is_failed());
  BOOST_CHECK(not test.has_cleaned());
  BOOST_CHECK(not test.has_prepared_upload());
  BOOST_CHECK(not test.has_started_upload());
  BOOST_CHECK(not test.has_started_download());
}

BOOST_AUTO_TEST_CASE(started_accepted)
{
  auto tr = make_transaction(TransactionStatus::started);
  tr.accepted = true;
  TestCase test(tr);
  BOOST_CHECK(not test.is_canceled());
  BOOST_CHECK(not test.is_failed());
  BOOST_CHECK(not test.has_cleaned());
  BOOST_CHECK(not test.has_prepared_upload());
  BOOST_CHECK(test.has_started_upload());
  BOOST_CHECK(not test.has_started_download());
}

BOOST_AUTO_TEST_CASE(started_recipient)
{
  auto tr = make_transaction(TransactionStatus::started);
  TestCase test(tr, false);
  BOOST_CHECK(not test.is_canceled());
  BOOST_CHECK(not test.is_failed());
  BOOST_CHECK(not test.has_cleaned());
  BOOST_CHECK(not test.has_prepared_upload());
  BOOST_CHECK(not test.has_started_upload());
  BOOST_CHECK(not test.has_started_download());
}

BOOST_AUTO_TEST_CASE(started_accepted_recipient)
{
  auto tr = make_transaction(TransactionStatus::started);
  tr.accepted = true;
  TestCase test(tr, false);
  BOOST_CHECK(not test.is_canceled());
  BOOST_CHECK(not test.is_failed());
  BOOST_CHECK(not test.has_cleaned());
  BOOST_CHECK(not test.has_prepared_upload());
  BOOST_CHECK(not test.has_started_upload());
  BOOST_CHECK(test.has_started_download());
}

BOOST_AUTO_TEST_CASE(canceled)
{
  auto tr = make_transaction(TransactionStatus::canceled);
  TestCase test(tr);
  BOOST_CHECK(test.is_canceled());
  BOOST_CHECK(not test.is_failed());
  BOOST_CHECK(test.has_cleaned());
  BOOST_CHECK(not test.has_prepared_upload());
  BOOST_CHECK(not test.has_started_upload());
  BOOST_CHECK(not test.has_started_download());
}

BOOST_AUTO_TEST_CASE(finished)
{
  auto tr = make_transaction(TransactionStatus::finished);
  TestCase test(tr);
  BOOST_CHECK(not test.is_canceled());
  BOOST_CHECK(not test.is_failed());
  BOOST_CHECK(test.has_cleaned());
  BOOST_CHECK(not test.has_prepared_upload());
  BOOST_CHECK(not test.has_started_upload());
  BOOST_CHECK(not test.has_started_download());
}

BOOST_AUTO_TEST_CASE(failed)
{
  auto tr = make_transaction(TransactionStatus::failed);
  TestCase test(tr);
  BOOST_CHECK(not test.is_canceled());
  BOOST_CHECK(test.is_failed());
  BOOST_CHECK(test.has_cleaned());
  BOOST_CHECK(not test.has_prepared_upload());
  BOOST_CHECK(not test.has_started_upload());
  BOOST_CHECK(not test.has_started_download());
}
