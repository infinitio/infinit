This howto is a memo that roughly describes important steps I had to
perform to get Infinit to compile on windows. It is probably
incomplete because my environment was already a development one,
please complete it. Notable missing steps are:

* Installing Visual Studio 11 (2012).
* Cloning the project (I share my VM filesystem with my Unix one, so
  all edition / git operations happen on Linux for me).

All in all, Drake pretty much handles all the dirty stuff
transparently. The "only" things you need to do is make sure the
required dependencies are somehow available on your computer: Python,
Boost, C++ compiler ...


Steps to get Infinit to compile on Windows:

* Install Python: http://www.python.org/download/releases/.

Install python version 3 to be able to run Drake. Select a 32bit
version because greenlet doesn't seem to be available for 64 bits for
now.

* Install Greenlet: https://pypi.python.org/pypi/greenlet#downloads.

Greenlet is required to run Drake.

* Install Boost: http://sourceforge.net/projects/boost/files/boost/1.53.0/.

Go to the boost directory, run:
  - .\bootstrap.bat
  - .\b2 install --without-wave --without-serialization --without-mpi --without-graph --without-parallel_graph --build-type=complete
The build-type=complete asks for the DLL to be compiled.

FIXME: for now you have to rename the include dir and DLL, but this should be fixed soon.

* Install Git: http://git-scm.com/download/win.

Git is required for Drake to be able to generate the version
files. When installing, choose the "windows command line tool" option.

* Install variadic templates capable cl version: http://www.microsoft.com/en-us/download/confirmation.aspx?id=35515.

Now this kinda sucks, but Microsoft provides no configuration files
for this compiler, you have to set it up by hand. In the
_build/win64/drake.py file, paths are passed assuming you installed it
in the default location, "C:\Program Files (x86)\Microsoft Visual C++
Compiler Nov 2012 CTP\bin".  If you changed it, adapt.
