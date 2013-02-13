#pragma once
#ifndef CLIENT_7I27SSAP
#define CLIENT_7I27SSAP

#include <elle/serialize/ListSerializer.hxx>
#include <elle/serialize/JSONArchive.hh>
#include <elle/serialize/Serializer.hh>
#include <elle/serialize/NamedValue.hh>

//- Notification serializers --------------------------------------------------

#define XXX_UGLY_SERIALIZATION_FOR_NOTIFICATION_TYPE()      \
  int* n = (int*) &value;                                   \
  ar & named("notification_type", *n)                       \
  /**/

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::Notification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::Notification, ar, value, version)
{
  enforce(version == 0);

  XXX_UGLY_SERIALIZATION_FOR_NOTIFICATION_TYPE();
}

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::UserStatusNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::UserStatusNotification, ar, value, version)
{
  enforce(version == 0);

  //ar & base_class<plasma::trophonius::Notification>(value);
  XXX_UGLY_SERIALIZATION_FOR_NOTIFICATION_TYPE();
  ar & named("user_id", value.user_id);
  ar & named("status", value.status);
}

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::TransactionNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::TransactionNotification, ar, value, version)
{
  enforce(version == 0);

  //ar & base_class<plasma::trophonius::Notification>(value);
  XXX_UGLY_SERIALIZATION_FOR_NOTIFICATION_TYPE();
  ar & named("transaction", value.transaction);
}

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::TransactionStatusNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::TransactionStatusNotification, ar, value, version)
{
  enforce(version == 0);

  //ar & base_class<plasma::trophonius::Notification>(value);
  XXX_UGLY_SERIALIZATION_FOR_NOTIFICATION_TYPE();
  ar & named("transaction_id", value.transaction_id);
  ar & named("status", value.status);
}

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::NetworkUpdateNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::NetworkUpdateNotification, ar, value, version)
{
  enforce(version == 0);

  //ar & base_class<plasma::trophonius::Notification>(value);
  XXX_UGLY_SERIALIZATION_FOR_NOTIFICATION_TYPE();
  ar & named("network_id", value.network_id);
  ar & named("what", value.what);
}

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::MessageNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::MessageNotification, ar, value, version)
{
  enforce(version == 0);

  //ar & base_class<plasma::trophonius::Notification>(value);
  XXX_UGLY_SERIALIZATION_FOR_NOTIFICATION_TYPE();
  ar & named("sender_id", value.sender_id);
  ar & named("message", value.message);
}

#endif /* end of include guard: CLIENT_7I27SSAP */
