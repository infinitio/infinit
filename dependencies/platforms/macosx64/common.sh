# extension
PLATFORM_LIBRARY_EXTENSION="dylib"
set_rpath()
{
  echo install_name_tool -add_rpath $1 $2
  install_name_tool -add_rpath $1 $2
}
