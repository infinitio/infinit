# -*- encoding: utf-8 -*-

"""
Welcome to errors! I hope you enjoy your stay. Put errors into their relevant
sections and ensure that you don't duplicate codes (there is a check on launch).
"""

errors = {
  # AWS
  'unable_to_get_aws_credentials': (-300, "Unable to fetch AWS credentials."),

  # Device
  'device_already_registered': (-10008, "This device name has already been taken."),
  'device_doesnt_belong_to_you': (-217, "This device doesn't belong to you."),
  'device_id_not_valid': (-216, "This device id is not valid."),
  'device_not_found': (-20003, "Device not found."),
  'device_not_valid': (-212, "This device name is not valid."),

  # Server
  'no_apertus': (-50008, "No apertus available."),

  # Transaction
  'file_name_empty': (-40000, "The name is null."),
  'transaction_already_finalized': (-50006, "This operation has already been finalized"),
  'transaction_already_has_this_status': (-50007, "This operation has already been done."),
  'transaction_cant_be_accepted': (-50004, "You can't accept this transaction."),
  'transaction_doesnt_belong_to_you': (-50002, "This transaction doesn't belong to you."),
  'transaction_doesnt_exist': (-50001, "This transaction doesn't exists."),
  'transaction_id_not_valid': (-219, "This transaction id is not valid."),
  'transaction_operation_not_permitted': (-50003, "This operation is not permited with this transaction."),

  # User
  'already_logged_in': (-102, "You are already logged in."),
  'email_already_confirmed': (-106, "Your email has already been confirmed."),
  'unknown_email_confirmation_hash': (-118, "Unknown hash."),
  'unknown_email_address': (-117, "Unknown email address."),
  'email_is_the_same': (-119, "Current email address is the same."),
  'email_already_registered': (-10003, "This email has already been taken."),
  'email_not_confirmed': (-105, "Your email has not been confirmed."),
  'email_not_valid': (-210, "This email is not valid."),
  'email_password_dont_match': (-10101, "Login/Password don't match."),
  'fullname_not_valid': (-220, "This fullname is not valid."),
  'handle_already_registered': (-10005, "This handle has already been taken."),
  'handle_not_valid': (-211, "This handle is not valid."),
  'no_more_invitation': (-50005, "You don't have any invitations left."),
  'not_logged_in': (-101, "You must be logged in."),
  'password_not_valid': (-213, "This password is not valid."),
  'unknown_user': (-103, "This user doesn't seem to exist."),
  'user_already_in_infinit': (-30002, "This user already use infinit."),
  'user_already_invited': (-30001, "This user has already been invited."),
  'user_id_not_valid': (-214, "This user id is not valid."),

  # Other
  'deprecated': (-888, "Some args are deprecated."),
  'field_is_empty': (-201, "This field is empty."),
  'unknown': (-666666, "Unknown error."),

  # Legacy
  'activation_code_doesnt_exist': (-10009, "This activation code doesn't match any."),
  'activation_code_not_valid': (-218, "This activation code is not valid."),
  'bad_request': (-200, "This request is bad formed."), # Send an HTTP 400
  'device_not_in_network': (-20004, "Cannot find the device in this network."),
  'maximum_device_number_reached': (-20008, "The maximum amount of devices has been reached for this network."),
  'network_doesnt_belong_to_you': (-20007, "This network doesn't belong to you."),
  'network_id_not_valid': (-215, "This network id is not valid."),
  'network_not_found': (-20002, "Network not found."),
  'operation_not_permitted': (-104, "Operation not permitted."), # Send an HTTP 403
  'root_block_already_exist': (-20005, "This network has already a root block."),
  'root_block_badly_signed': (-20006, "The root block was not properly signed."),
  'user_already_in_network': (-20001, "This user is already in the network."),
}

# Ensure that there are no duplicated error codes.
error_list = list(map(lambda x: x[0], errors.values()))
error_set = set(error_list)
assert len(error_list) == len(error_set)

# XXX
for name, value in errors.items():
  globals()[name.upper()] = value

class Error(Exception):
  def __init__(self, error_code, error_message = None):
    super().__init__(error_code,
                     error_message and error_message or error_code[1])
