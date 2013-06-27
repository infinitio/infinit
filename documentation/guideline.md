
Coding guidelines
=================

Library structure
-----------------

### XXX Directory structure

src Vs source Vs srcs Vs sources

### XXX drake and makefiles

### XXX Add a `readme` file

The library should contain at its top a file called README.md containing the
following:

 * Short description.
 * A synopsis.
 * Quick information on how to install it (and required dependencies).
 * A short but useful sample.
 * Some user manual (or a link to it).
 * Some library reference (or a link to it).

### Apply the B-BSD license (for an open source library)

Default copyright is full ownership of Infinit when no license is mentioned.
Thus, any library that is meant to be open source should indicate otherwise.

The suggested license is the Beer BSD, which is a 3-clause BSD license with the
beer clause.

**How to apply the license**

 * Add the license in plain text in the root directory of the library.
 * Add an header in every source file

The header should contain the following:

	Copyright (c) ``YEAR'', infinit.io

	This software is provided "as is" without warranty of any kind,
	either expressed or implied, including but not limited to the
	implied warranties of fitness for a particular purpose.

	See the LICENSE file for more information on the terms and
	conditions.

Replace `YEAR` with all years of application of the license, for example:

	Copyright (c) 2012-2017, 2019, infinit.io


Contract programming
--------------------

### Rationale

Contract programming is not integrated into the language, so additional tools
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

### Handling assertions

Assertions are generated as an exception of type `AssertError` that does not
derive from `std::exception`. You should never catch that exception, but in the
main function of the program. See [[#Exception]] handling for information on
how to handle exceptions.

How to log
----------

### Rationale

A good log system is of a great help when it comes to debug locally, or to
reproduce a problem a client has. The log system should be coherently used
accross the whole software, using the following levels:

 * ERR
 * WARN
 * LOG
 * TRACE
 * DEBUG
 * DUMP

### Decision

Each module should declare in each implementation file a *log component* of
the form `complete.namespace.ClassName`.

All logs are **relative to the module emitting them** (read this again), and an
executable using them is responsible to select correct levels of each module.

 1. `ELLE_ERR`
All errors that will be reported to the caller of your module should logged as
errors.

 2. `ELLE_WARN`
All errors or exception that are not propagated *must be* logged as warnings.

 3. `ELLE_LOG`
Main information about the module, should be user-friendly.

 4. `ELLE_TRACE`
Logical information about what the module is doing.

 5. `ELLE_DEBUG`
Implementation information, like how many bytes have been read.

 6. `ELLE_DUMP`
Data information, like the content of the buffer just read.

To trace function and methods calls, use `ELLE_TRACE_FUNCTION/METHOD`.

**Note:** `ELLE_TRACE_FUNCTION/METHOD` is not related to the `TRACE` level, it's
activated or disabled as wanted.

XXX `ELLE_TRACE` should be renamed

Defining class attributes
-------------------------

### Rationale

The C++ language has no easy way to define properties (as C# and Python do).
The only pratical way to do it is by using macros that will dump appropriate
methods into the class itself.

This could be done by hand, but it would have been tedious, error prone, and
very verbose. In fact, there are countless examples where you want to give a
restricted access to an attribute that is privatly declared, even if it's only
for debugging.

### Decision

In order to be coherent in all attribute declarations, you must use one of
`ELLE_ATTRIBUTE_*` macro collection everywhere.

### Exceptions

With template meta-programming, attributes are often `static const` values that
are public and meant to be used at compile time. Such constants are not subject
to be properties.

Relational operators overloading
--------------------------------

### Rationale

Making an object fully comparable require to overload 6 operators, which is
redudent and error-prone.

### Decision

You should only implement the two comparison operators `<` and `==`. When using
that class, you might want to use `std::rel_ops` namespace in order to have
access to other operators.

See [http://www.cplusplus.com/reference/utility/rel_ops/](http://www.cplusplus.com/reference/utility/rel_ops/) for more information.

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

### Internal documentation

Documenting your own code to understand it faster, or to indicate some tricky,
dangereous part of it, is encouraged. It should be written in a way it's not
exported into doxygen documentation.

### Notes

Internal documentation should not be abused, the code should be straight
forward, well named and splitted into simple functions. Documenting the body of
a function should be extremely rare. Prefer to add developer documentation
below the function implementation, and refer to any external libraries
documentation there.

Make your types printable
-------------------------

### Rationale

The C++ way to make an object compatible with output streams is to overload
a free function that correctly output the object in a human readable form.

This introduce several problems and pitfalls. Firstly, the syntax is really
verbose and not obvious for beginners. Often, it will imply to add a friend
declaration to the class, and to add a forward declaration of the overload
itself. Secondly, the polymorphism can not be used: the printed type might not
be the final one in the context of inheritance. The overload called will be the
one of the compile time type.

### Discussion

We considered carefully what are the advantages of using inheritance instead of
the operator overload:

#### Pros
 * Print always the final type
 * Act as a contract
 * Factorize (no more crappy friend, and forward declaration)
 * Simpler to access private members

#### Cons
 * Add a vtable
 * temptation to print internal members
 * As printing in ctor or dtor might be possible, every class that is printable
   should implement the Printable contract for a "correct" print.

### Decision

Always inherit from the class elle::Printable in every level of inheritance,
even in the interfaces. The exceptions to that rule are POD objects or small
and intensively used objects, in which cases you should use the traditional
way to make that object printable (overloading of the operator << ).

XXX What to print ? Example with the class Endpoint:

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

XXX Each header of the module should include it as the first include that
appears in the file. That way, forward declaration are validated when
confronted to their real declaration.

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

Sometimes, raw pointers must be used, especially when using external libraries.
Even in these cases, you are encouraged to:

 * Write a `deleter` for any smart pointer
 * Use `ELLE_FINALLY`
 * Write explicit guards to manage these resources

Writing contructors
-------------------

### General rules

 1. An objet is either valid or not present: objects should not be constructed
    but yet in an invalid state.

 2. All attributes defined in the class should be initialized in the same order
    of their declaration.

 3. Use of c++11 delegate constructor facilities is encouraged.

 4. Classes declaration must explicitly show if the object is copyable or not.

 4. Movability is encouraged over copyability.

### Default constructible types

Only if a default value for that object makes sense.

### Copyable types

An object is either copyable or non-copyable. In any cases, it have to be
explicitly tagged as such:

A fully copyable class should look like:
<code lang="c++">

	struct A
	{
	  A(A const& other);
	  A& operator =(A const& other);
	};
</code>

While a non copyable one
<code lang="c++">

	struct A
	{
	  A(A const& other) = delete;
	  A& operator =(A const& other) = delete;
	};
</code>

If you declare a type moveable, you shall not explicitely delete copy ctor and
assignment, as it's the default behavior.

Using `boost::noncopyable` (when interoperability with boost is required) do
not remove the need to explicitly specify copyable constructors.

### Movable types

Making a type movable implicitly tag the copy constructor as `deleted`, as well
as copy assignment and move assignment operators. However, you should always be
clear on your intent. Movability can be an optimization of the copy, or express
precisely the opposite: the class is not copyable.

A fully movable class should look like:
<code lang="c++">

	struct A
	{
	  A(A&& other);
	  A& operator =(A&& other);
	  // The class is not copyable by default
	  // A(A const& other) = delete;
	  // A& operator =(A const& other) = delete;
	};
</code>

While a non movable one
<code lang="c++">

	struct A
	{
	  A(A&& other) = delete;
	  A& operator =(A&& other) = delete;
	};
</code>

**Note:** A copyable object support the move syntax.

**Note:** After beeing moved, an instance should not be used anymore, and any
call except calling the destructor goes in the land of "undefined behavior".
You are greatly encouraged to use scope blocks to limit visibility of a moved
object.

<code lang="c++">

	std::unique_ptr<int> func()
	{
	  std::unique_ptr<int> result;
	  {
	    std::unique_ptr<int> ptr(new int);
	    *ptr = 2;
	    result = std::move(ptr); // no more use of ptr
	  }
	  if (result != nullptr)
	    *result += 42;
	  return result; // no more use of result
	}
</code>

### Serializable types

To be instanciated easily on the stack from an archiver, types must implement
a deserialization constructor. A special care should be given about it: the
instance should be "not initialized" but "yet destructible".

### General constructors

Constructors that have one argument and/or any number of optional arguments
should be `explicit` or documented as implicit on purpose.

### Summary

Any class of type T should have the following constructors and assignment
operators:
<code lang="c++">

	T(T const& other)[ = default/delete ];
	T& operator =(T const& other)[ = default/delete];
</code>

and/or
<code lang="c++">

	T(T&& other)[ = default/delete]
	T& operator =(T&& other)[ = default/delete]
</code>

Any (possible) single argument constructor should be explicit:
<code lang="c++">

	explicit
	T(int i);

	explicit
	T(int i, int j = 0);

	explicit
	T(int i = 12, int j = 0);

	// or documented as implicit:
	/// Implicit construction from intptr_t;
	T(intptr_t ptr);
</code>

XXX Throwing exception from a destructor
----------------------------------------

### Description

When an exception is thrown from a destructor, many things can happen:

 1. If there is no try/catch block on the call path, the program instantly
terminates.

 2. If there is one, it will continue to unwind the stack. However, if a second
exception is thrown, the program instantly terminates.

See [this excellent blog post](http://www.kolpackov.net/projects/c++/eh/dtor-1.xhtml)
for a comprehensive guide to exceptions thrown in destructor.

### Pros

 * RAII easier to implement

### Cons

 * [Herb sutter][GOTW47], [Intel corporation][INTEL_EXCEPT],
   [CERT][CERT_EXCEPT], [Parashift][PARASHIFT_EXCEPT] and [the creator of the
   C++][STROUSTRUP_EXCEPT] strongly recommend against it.
 * Not compatible with STL
 * Not compatible with most external libraries (that don't throws in dtor)
 * leaks memory when heap is used.
 * Philosophically, in contract programming, post conditions should never throw
   an exception, but asserts (std::thread asserts if the thread was not joined
   before beeing destroyed).

[GOTW47]: http://www.gotw.ca/gotw/047.htm
[INTEL_EXCEPT]: http://software.intel.com/sites/products/documentation/doclib/iss/2013/sa-ptr/sa-ptr_win_lin/GUID-D2983B74-74E9-4868-90E0-D65A80F8F69F.htm
[CERT_EXCEPT]: https://www.securecoding.cert.org/confluence/display/cplusplus/ERR33-CPP.+Destructors+must+not+throw+exceptions
[PARASHIFT_EXCEPT]: http://www.parashift.com/c++-faq-lite/dtors-shouldnt-throw.html
[STROUSTRUP_EXCEPT]: http://www.stroustrup.com/bs_faq2.html#ctor-exceptions

### Discussion

#### Proposal 1: Checking if there is an exception on the fly
<code lang="c++">

	struct A
	{
		~A()
		{
			if (!std::uncaught_exception())
				throw Exception{"I can safely throw"};
		}
	};
</code>

##### Pros

 * Will do what you want *most of the time*.

##### Cons

 * Still not compatible with STL
 * Still not compatible with most external libraries (that don't throws in dtor)
 * Still leak.
 * All destructor should use a special syntax to ensure exceptions are only
thrown if `std::uncaught_exception()` is `false`.
 * Not correct:

<code lang="c++">

	struct BankAccountOperation
	{
		~BankAccountOperation()
		{
			try
			{
				Transaction t(this->from, this->to);
			}
			catch (...)
			{
				// rollback and report error
			}
		}
	};

</code>

If `Transaction` throws in its destructor, we can safely rollback the operation,
but if there is already an uncaught exception, `Transaction` destructor won't
throw, the rollback code won't be reached.

#### Proposal 2: Never throw from destructors and use STL-like APIs

Replace *flush* by *cleanup operation that must be done before destroying the
object*.

<code lang="c++">

	struct Socket
	{
		Socket& operator << (std::string msg);
		Socket& operator << (flush_type)
		{ this->flush(); }
		~Socket()
		{
			try
			{
				this->flush();
			}
			catch (...)
			{
				// log errors here
			}
		}
	};

	int main()
	{
		Socket s("some ip address");
		s << "Hello" << std::flush;
	}

</code>

##### Pros

 * Compatible with STL and libraries that assume object don't throw in their dtor.
 * Don't leak.
 * Straight forward
 * Still RAII
 * STL streams work like that (flush is explicit).

##### Cons

 * It's the user's responsability to flush the stream *if* one wants to check
errors.


Calling virtual member function in constructors/destructors
-----------------------------------------------------------

### Rationale

Calling virtual member function will not do what you want in a ctor/dtor. It
will call the current implementation of that function instead of looking down
in the inheritance tree. While a contract might be implemented "down" in tree,
this might cause a pure virtual function call.

### How to

When possible, don't do it at all. If you really need to call a function, make
a `static` or `final` version of it. Lastly, if you really need to call a
virtual function, document the call itself in ctor/dtor to explicitly show that
you know what you're doing.

Exception
---------

### Exception types

The base exception of all exceptions defined in infinit softwares is
`elle::Exception`, which you use any time you need to throw an exception.

A module might define its own exception types, but it should be only to add
semantics into exceptions, not just to *catch everything that comes from this
module*. Indeed, the module itself uses other modules that throws other kind of
exceptions. Moreover, the only reason to catch special kind of exceptions, is
to do a job related action, rarely a *module related* one. For example, an
exception like `ConnectionClosed` is perfectly clear and useful for the user of
the `reactor` library.

### Handling exceptions

If you want to prevent exceptions to propagate, you'll just want to catch
`std::exception const&`. For logic errors or special errors, refer to the
module you are using.

**Warning:** You should never catch `...`, except in rare case where you need
to do some cleanup operation before rethrowing the exception. Remember that
assert throws an exception that should not be caught (AssertError) except in
the `main()`.

Usage of post-increment and post-decrement operators
----------------------------------------------------

### Rationale

Post increment or decrement operators imply a copy, which is rarely wanted. The
copy itself has a cost for non trivial types.

### Decision

You should never use post-increment or post-decrement operators.

### Exceptions

Sometimes, the copy of the operator is wanted, and simple enough to be
understood. You should prefer the cleanest way to implement your algorithm,
rather than using more variables just to fit that rule.

Be aware that in few cases, pre-increment operators forbid some CPU
optimisations when the result of the incrementation is needed before another
load/store operation.
<code lang="c++">

	int array[42];
	int i = 0;
	while (i < 42)
	  array[i++] = 0; // increment and array store can be done in parallel
</code>

Prefer the use of c++11 for-range loop
--------------------------------------

### Rationale

Iterating over a container is often done in the exact same way. The C++11 adds
a new way to it, that is less verbose, and might be subject to optimizations as
it is now handled by the compilator.

### Decision

Whenever possible, use the for-range loop syntax.

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

Git branch naming
-----------------

### `develop`

Current version of infinit. Should always compile and pass the tests.

### `feature/name-of-the-feature`

For more than one day work, often a specific feature, you should work and use
a special feature branch, that should be deleted when merged.

### `release/X.X.X`

When the code is freezed in order to release a new version, the branch of the
version to come is created in a branch. After the release, the branch should
be deleted, but a tag should be made on the exact commit used for the build.

### `sandbox/your-pseudo/save-your-work`

If you need to save some work, in a dirty state, or not related to a feature,
you can always push it under `sandbox/your-pseudo`.

Formatting git commits
----------------------

### The title

The commit should begin with the path to the module in lower case, separated
with dots and followed by a colon. If the main module is a separated git
repository, you should not repeat it. The message should begin with a capital
and end with a dot, starting with a verb. The total line length may be less
than 72 chars.

If you need to add more text, add a blank line and write your explanations
below.

Example:
	module.submodule: Print exception backtrace when possible.

	Handle now elle::Exception and print the contained backtrace.

git merge Vs git rebase
-----------------------

Always use rebase your code, except when merging a branch from `feature/` or
`release/`.

XXX Limit use macros
--------------------

### Discussion

XXX "Always compare values explicitly
-------------------------------------

if (my_bool == true ||
    my_ptr == nullptr ||
    my_size != 0) {}"

### Discussion

Raph: I agree with that for everything but `bool` type.

Overriding a virtual method
---------------------------

### Rationale

Overriding a virtual method in C++ is done implicitly by using the exact same
signature at any level of the inheritance tree. If the root method signature
changes, one would have to change all overrides accordingly. The main problem
is that no compilation time checks are made to verify that signatures are
correct. However, the new explicit keywords `override` and `final` solve this
problem by checking that a method qualified with one of this keywords actually
overrides something.

### Decision

You should always use the `override` or `final` keywords when overriding a
virtual method. As these keywords imply that the method is virtual, do not
repeat the virtual keyword.

XXX How to include headers
--------------------------

### Quote or angle brackets ?

### Order

### Discussion

Raph: Use both, quote for module includes, angle brackets with absolute path
for everything else.


XXX Efficient argument passing
------------------------------
