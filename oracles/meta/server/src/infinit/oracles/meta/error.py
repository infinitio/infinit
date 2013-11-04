errors = {
  'not_logged_in': (-101, "You must be logged in."),
  'already_logged_in': (-102, "You are already logged in."),
  'unknown_user': (-103, "This user doesn't seems to exist."),
  'operation_not_permitted': (-104, "Operation not permitted."),
  'bad_request': (-200, "This request is bad formed."),
  'field_is_empty': (-201, "This field is empty."),
  'email_not_valid': (-210, "This email is not valid."),
  'handle_not_valid': (-211, "This handle is not valid."),
  'device_not_valid': (-212, "This device name is not valid."),
  'password_not_valid': (-213, "This password is not valid."),
  'user_id_not_valid': (-214, "This user id is not valid."),
  'network_id_not_valid': (-215, "This network id is not valid."),
  'device_id_not_valid': (-216, "This device id is not valid."),
  'device_doesnt_belong_tou_you': (-217, "This device doesn't belong to you."),
  'activation_code_not_valid': (-218, "This activation code is not valid."),
  'transaction_id_not_valid': (-219, "This transaction id si not valid."),
  'deprecated': (-888, "Some args are deprecated."),
  'email_already_registred': (-10003, "This email has already been taken."),
  'handle_already_registred': (-10005, "This handle has already been taken."),
  'device_already_registred': (-10008, "This device name has already been taken."),
  'activation_code_doesnt_exist': (-10009, "This activation code doesn't match any."),
  'email_password_dont_match': (-10101, "Login/Password don't match."),
  'user_already_in_network': (-20001, "This user is already in the network."),
  'network_not_found': (-20002, "Network not found."),
  'device_not_found': (-20003, "Device not found."),
  'device_not_in_network': (-20004, "Cannot find the device in this network."),
  'root_block_already_exist': (-20005, "This network has already a root block."),
  'root_block_badly_signed': (-20006, "The root block was not properly signed."),
  'network_doesnt_belong_to_you': (-20007, "This network doesn't belong to you."),
  'maximum_device_number_reached': (-20008, "The maximum amount of devices has been reached for this network."),
  'user_already_invited': (-30001, "This user has already been invited."),
  'user_already_in_infinit': (-30002, "This user already use infinit."),
  'file_name_empty': (-40000, "The name is null."),
  'transaction_doesnt_exist': (-50001, "This transaction doesn't exists."),
  'transaction_doesnt_belong_to_you': (-50002, "This transaction doesn't belong to you."),
  'transaction_operation_not_permitted': (-50003, "This operation is not permited with this transaction."),
  'transaction_cant_be_accepted': (-50004, "You can't accept this transaction."),
  'no_more_invitation': (-50005, "You don't have any invitations left."),
  'transaction_already_finalized': (-50006, "This operation has already been finalized"),
  'transaction_already_has_this_status': (-50007, "This operation has already been done."),
  'no_apertus': (-50008, "No apertus available."),
  'unknown': (-666666, "Unknown error."),
}

# XXX
for name, value in errors.items():
  globals()[name.upper()] = value

class Error(Exception):
  def __init__(self, error_code, error_message = None):
    super().__init__(error_code,
                     error_message and error_message or error_code[1])
