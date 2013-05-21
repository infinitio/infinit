
Coding guidelines
=================

Add a license file in every library
-----------------------------------

### Rationale

  Default copyright is full ownership of Infinit when no license is mentioned.
Thus, any library that is meant to be open source should indicate otherwise.

### License to be applied for opened software.

 XXX: BBSD accepted ?

### How to apply the license

 * Add the license in plain text in the root directory of the library.
 * Add an header in every source file

 XXX: Form of the header in general (example in C and python)

Use `ELLE_ASSERT` and `ELLE_ENFORCE`
------------------------------------

### Rationale

  Contract programming is not integrated in the language, so additional tools
for that task must be provided to the programmer.

### Discussion

  Assertion must be used only to check your own code, not user inputs. This
applies especially for a library: arguments of the API of your library should
not be checked with assertions.

### Notes

When applicable, prefer the use of specialized assert macros (like
`ELLE_ASSERT_EQ`), which gives better display.

Use `ELLE_ENFORCE` for tests, or to ensure some conditions even for production
code.

### Discussion

XXX discuss assert implem (Implement ELLE_ASSERT with an exception ?)
-> opportunity to handle the assert case
-> incompatible with catch (...)
-> rely on others to write good try/except clauses
-> Should terminate the program.

XXX Exception handling
----------------------



XXX Use `ELLE_TRACE_FUNCTION/METHOD` ?
--------------------------------------

Should be logically added (more semantic): Only job related function
!!! Not in ctor/dtor !!!

XXX Use `ELLE_ATTRIBUTE_* everywhere` ?
---------------------------------------

### Pros
Factorize, fast, enforce naming

### Cons
Doesn't cover every cases (&, const&, mutable, ...)
Obscure for newbies
impossible to parse the code without a preprocessor

### Proposal
generate only the getter/setter code

XXX Use `ELLE_OPERATOR` (especially `OPERATOR_RELATIONALS`)
-----------------------------------------------------------

Rely on mefyl's Orderable ?
Rely on std::rel_ops ?

Documenting your code
---------------------

### Style

Documentation of the code, internal or external, should always be concise, in
the norminal or imperative form. Most of the time, correct and explicit naming
should be self-explanatory, though.

### Doxygen documentation

All classes and their public members (including types, variables and functions)
must be documented. Protected members might be documented if they are intented
to be inherited by the user of your library.

### XXX Internal documentation

Documenting your own code to understand it faster, or to indicate some tricky,
dangereous part of it, is encouraged. It should be written in a way it's not
exported into doxygen documentation.

### Notes

Internal documentation should not be abused, the code should be straight
forward, well named and splitted into simple functions. Documenting the body of
a function should be extremely rare. Prefer to add developer documentation
below the function implementation, and refer to external libraries
documentation there.

XXX Derive and implement elle::Printable as much as possible
------------------------------------------------------------

### Pros

 * Print always the final type
 * Act as a contract
 * Factorize (no more crappy friend, and forward declaration)
 * Simpler to access private members

### Cons

 * Add a vtable
 * temptation to print internal members
 * As printing in ctor or dtor might be possible, every class that is printable
   should implement the Printable contract for a "correct" print.

### Discussion

 * Only small and intensively used objects should suffer the vtable problem
 * Object should be adapted, because `Printable` is a capacity, not a behavior
 * What to print ? Example with the class Endpoint:
	<Endpoint ip=192.168.0.1 port=4242>
	Vs
	192.168.0.1:4242

Use forward files (`fwd.hh`) as much as possible
------------------------------------------------

### Rationale

Including too much headers lead to longer compile time. In many cases, it's
sufficient to only forward declare classes and functions. The need for a
special header with all public module forward declarations comes when writing
the module itself, and obviously, modules that depends on it. Forwarding by
hand would be both tedious and error prone.

### How to

All public types and functions (from the module point of view) of a module
should be forward declared in a special header called `fwd.hh`.
XXX Each header of the module should include it as the first include that appears
in the file.  That way, forward declaration are validated when confronted to
the real declaration.


Express ownership with smart pointers
-------------------------------------

### Rationale

C++ lack of a garbage collector requires developpers to manually manage
allocated memory. Correctness of it is often hard to prove, but using some
tools, some basic assertion can be done on the lifetime of managed memory.

### Rules

Whenever possible, use:
 * `std::unique_ptr` for single ownership
 * `std::shared_ptr` for shared ownership
 * `std::weak_ptr` for weak dependency on a shared object
 * `elle::Buffer` for raw buffers
 * Standard containers for multiples objects

XXX **Note:** Usage of `new []` is forbidden.

**Note:** All usage of `delete ptr` must be followed by a `ptr = nullptr` line.

### Exceptions

Sometime, raw pointers must be used, especially when using external libraries.
Even in these cases, you are encouraged to:
 * Write a `deleter` for any smart pointer
 * Use `ELLE_FINALLY`
 * Write explicit guards to manage these resources

XXX Writing contructors
-----------------------

### General rules

 1. An objet is either valid or not present: objects should not be constructed but
yet in an invalid state.

 2. All attributes defined in the class should be initialized in the same order of
their declaration.

 3. Use of c++11 delegate constructor facilities is encouraged.

### Default constructible types

Only if a default value for that object makes sense.

### Copyable types

An object is either copyable or non-copyable. In any cases, it have to be
explicitly tagged as such:

A fully copyable class should look like:
<code>struct A {
  A(A const& other);
  A& operator =(A const& other);
};</code>

While a non copyable one
<code>struct A {
  A(A const& other) = delete;
  A& operator =(A const& other) = delete;
};</code>

Using `boost::noncopyable` (when interoperability with boost is required) do
not remove the need to explicitly specify copyable constructors.

### Movable types

They follow the same rules as copyable types.

**Note:** After beeing moved, an instance should not be used anymore, and
any call except calling the destructor goes in the land of "undefined behavior".

### Serializable types

To be instanciated easily on the stack from an archiver, types must implement
a deserialization constructor. A special care should be given about it: type
should be "not initialized" but yet "destructible".

### General constructors

Single argument constructors should be explicit.

### Discussion

 * Define as little as possible class constructor, and forbid use of others
 * Use `ELLE_NO_ASSIGNMENT` as much as possible


XXX Throwing exception from a destructor
----------------------------------------

### Discussion

XXX Calling virtual member function in constructors/destructors
---------------------------------------------------------------

### Rationale

Calling virtual member function will not do what you want in a ctor/dtor. It
will call the current implementation of that function looking up in the
inheritance tree. While a contract might be implemented "down" in tree, this
might cause a pure virtual function call.

### Discussion

 * Tag function with final or static
 * Document the function to not be made virtual (in the header)

XXX How to ship library
-----------------------

### How to

 * README file
 * LICENSE file
 * Makefile file
 * Sources files header

### Discussion

Julien: I believe samples should be written right in the README file because
users, especially on Github want to have access to all the information directly
from the main page: how to build, install and write a test program in just a
few minutes. If it works, I'll consider the library, otherwise I am frustrated
with my first experience as a developer.


XXX Never use postincrement or postdecrement
--------------------------------------------

### Discussion

XXX Prefer the use of c++11 for-range loop
------------------------------------------

### Discussion

XXX In unit tests, add at least on deserialization from hardcoded string & upgrade the formats	detect format changes
--------------------------------------------------------------------------------------------------------------------

### Discussion

XXX Always specify full namespace path in macros (with root namespace)
----------------------------------------------------------------------

### Discussion

XXX Hide underlying libraries when abstracting them
---------------------------------------------------

### Discussion

XXX Use PIMPL pattern for complex classes (with unique_ptr)
-----------------------------------------------------------

### Discussion

XXX Do not include "heavy" external headers (like iostream or boost/asio) in header files
-----------------------------------------------------------------------------------------

### Discussion

XXX Highlight early returns in function (and prefer short function with few block)
----------------------------------------------------------------------------------

### Discussion

XXX Use of template should be considered twice (or more)
--------------------------------------------------------

### Discussion

XXX Rely on a DumpArchive for displaying a class internals (as Dump() does today)
---------------------------------------------------------------------------------

### Discussion

XXX Add a README file in every library explaining everything: purpose, build, install and samples
-------------------------------------------------------------------------------------------------

### Discussion

XXX Branch and commit naming
----------------------------

### Discussion

XXX git commit naming
---------------------

### Discussion

XXX git merge Vs git rebase
---------------------------

### Discussion

XXX Limit use macros
--------------------

### Discussion

XXX "Always compare values explicitly:
--------------------------------------

if (my_bool == true ||
    my_ptr == nullptr ||
    my_size != 0) {}"

### Discussion

XXX Always use override keyword when overriding a virtual method
----------------------------------------------------------------

### Discussion

