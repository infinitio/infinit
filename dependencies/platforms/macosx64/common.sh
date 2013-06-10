# extension
PLATFORM_LIBRARY_EXTENSION="dylib"

set_rpath()
{
  rpath="${1}"
  path="${2}"

  install_name_tool -delete_rpath "${rpath}" "${path}"
  install_name_tool -add_rpath "${rpath}" "${path}"
}
