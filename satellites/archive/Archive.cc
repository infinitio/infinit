#include <elle/archive/archive.hh>
#include <elle/system/system.hh>
#include <elle/windows/locale.hh>

static const Locale utf8_setter{};

int main(void)
{
  boost::filesystem::path path{
    "АБВГДЕЖЅZЗИІКЛМНОПҀРСТȢѸФХѾЦЧШЩЪЫЬѢꙖѤЮѦѪѨѬѠѺѮѰѲѴ.mp3"};
  boost::filesystem::path dest{path};
  dest.replace_extension("zip");
  elle::system::write_file(path , elle::Buffer{"@@@@@@@@@@", 10});

  elle::archive::archive(elle::archive::Format::zip,
                         {path,},
                         dest,
                         elle::archive::Renamer(),
                         elle::archive::Excluder(),
                         false);
}
