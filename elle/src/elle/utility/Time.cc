#include <elle/utility/Time.hh>

#include <elle/log.hh>

#include <elle/utility/Duration.hh>

#include <pthread.h>
#include <ctime>

ELLE_LOG_COMPONENT("elle.utility.Time");

namespace elle
{
  namespace utility
  {
    /*---------------.
    | Static Methods |
    `---------------*/

    Time
    Time::current()
    {
      Time time;

      time.Current();

      return (time);
    }

//
// ---------- constructors & destructors --------------------------------------
//

    ///
    /// the constructor.
    ///
    Time::Time():
      nanoseconds(0)
    {
    }

//
// ---------- methods ---------------------------------------------------------
//

    ///
    /// this method fills the instance with the current time.
    ///
    Status              Time::Current()
    {
      auto now = std::chrono::system_clock::now();

      // gets the number of nanoseconds since Epoch UTC
       this->nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();

      ELLE_DEBUG("get current time: %s", this->nanoseconds);

      return Status::Ok;
    }

    ///
    /// this method converts a time object into a time_t structure.
    ///
    Status              Time::Get(::time_t&                     time) const
    {
      // set the time i.e in seconds.
      time = this->nanoseconds / 1000000000;

      return Status::Ok;
    }

    ///
    /// this method converts a time_t into a time object.
    ///
    Status              Time::Set(const ::time_t&               time)
    {
      this->nanoseconds = time * 1000000000;

      return Status::Ok;
    }

#if defined(INFINIT_WINDOWS)
    ///
    /// This method converts a FILETIME into a Time object.
    ///
    Status              Time::Get(::FILETIME&                   ft) const
    {
      ULARGE_INTEGER    value;

      // quad part is in 100ns, since 1601-01-01 ... fuck ms
      value.QuadPart = this->nanoseconds / 100
        + ((369ULL * 365 + 89) * 24 * 60 * 60 * 1000 * 1000 * 10);
      //   y         d     bis   h    m    s    ms     us     100ns

      ft.dwLowDateTime  = value.LowPart;
      ft.dwHighDateTime = value.HighPart;

      return Status::Ok;
    }

    ///
    /// This method converts a FILETIME into a Time object.
    ///
    Status              Time::Set(const ::FILETIME&             ft)
    {
      ULARGE_INTEGER    value;

      value.LowPart = ft.dwLowDateTime;
      value.HighPart = ft.dwHighDateTime;

      this->nanoseconds = value.QuadPart * 100;

      return Status::Ok;
    }
#endif

//
// ---------- object ----------------------------------------------------------
//

    ///
    /// this operator compares two objects.
    ///
    Boolean             Time::operator==(const Time&            element) const
    {
      return (this->nanoseconds == element.nanoseconds);
    }

    ///
    /// this operator compares two times.
    ///
    Boolean             Time::operator<(const Time&             element) const
    {
      if (this->nanoseconds < element.nanoseconds)
        return true;

      return false;
    }

    ///
    /// this operator compares two times.
    ///
    Boolean             Time::operator>(const Time&             element) const
    {
      if (this->nanoseconds > element.nanoseconds)
        return true;

      return false;
    }

    ///
    /// this operator adds a time to the current one.
    ///
    Time                Time::operator+(const Time&             element)
    {
      Time              time;

      time.nanoseconds = this->nanoseconds + element.nanoseconds;

      return (time);
    }

    ///
    /// this operator substracts a time to the current one.
    ///
    Time                Time::operator-(const Time&             element)
    {
      Time              time;

      time.nanoseconds = this->nanoseconds - element.nanoseconds;

      return (time);
    }

    ///
    /// this operator adds a duration to the current time.
    ///
    Time                Time::operator+(const Duration&         duration)
    {
      Time              result(*this);

      // depending on the unit.
      switch (duration.unit)
        {
        case Duration::UnitNanoseconds:
          {
            // add the value.
            result.nanoseconds += duration.value;

            break;
          }
        case Duration::UnitMicroseconds:
          {
            // add the value.
            result.nanoseconds += duration.value * 1000;

            break;
          }
        case Duration::UnitMilliseconds:
          {
            // add the value.
            result.nanoseconds += duration.value * 1000000;

            break;
          }
        case Duration::UnitSeconds:
          {
            // add the value.
            result.nanoseconds += duration.value * 1000000000LU;

            break;
          }
        case Duration::UnitMinutes:
          {
            // add the value.
            result.nanoseconds += duration.value * 1000000000LU * 60;

            break;
          }
        case Duration::UnitUnknown:
          goto _return;
        }

    _return:
      return (result);
    }

    ///
    /// this operator substracts a duration to the current time.
    ///
    Time                Time::operator-(const Duration&         duration)
    {
      Time              result(*this);

      // depending on the unit.
      switch (duration.unit)
        {
        case Duration::UnitNanoseconds:
          {
            // add the value.
            result.nanoseconds -= duration.value;

            break;
          }
        case Duration::UnitMicroseconds:
          {
            // add the value.
            result.nanoseconds -= duration.value * 1000;

            break;
          }
        case Duration::UnitMilliseconds:
          {
            // add the value.
            result.nanoseconds -= duration.value * 1000000;

            break;
          }
        case Duration::UnitSeconds:
          {
            // add the value.
            result.nanoseconds -= duration.value * 1000000000L;

            break;
          }
        case Duration::UnitMinutes:
          {
            // add the value.
            result.nanoseconds -= duration.value * 1000000000 * 60;

            break;
          }
        case Duration::UnitUnknown:
          break;
        }

      return (result);
    }

    Natural64
    Time::microseconds()
    {

    }

    Natural64
    Time::milliseconds()
    {

    }

    Natural64
    Time::seconds()
    {

    }

//
// ---------- dumpable --------------------------------------------------------
//

    ///
    /// this function dumps an time object.
    ///
    Status              Time::Dump(Natural32                    margin) const
    {
      String            alignment(margin, ' ');
      ::tm*             tm;
      ::time_t          time;

      // convert the nanoseconds in a time_t.
      time = this->nanoseconds / 1000000000;

      // retrieve a _tm_ structure.
      tm = ::gmtime(&time);

      // display the time.
      std::cout << alignment << "[Time] "
                << std::dec
                << (1900 + tm->tm_year) << "-" << (1 + tm->tm_mon)
                << "-" << (1 + tm->tm_mday) << " "
                << tm->tm_hour << ":" << tm->tm_min << ":" << tm->tm_sec
                << "." << (this->nanoseconds % 1000)
                << std::endl;

      return Status::Ok;
    }

    /*----------.
    | Printable |
    `----------*/

    void
    Time::print(std::ostream& stream) const
    {
      ::tm* tm;
      ::time_t time;

      // Convert the nanoseconds in a time_t.
      time = this->nanoseconds / 1000000000;

      // Retrieve a _tm_ structure.
      tm = ::gmtime(&time);

      stream << std::dec
             << (1900 + tm->tm_year) << "-" << (1 + tm->tm_mon)
             << "-" << (1 + tm->tm_mday) << " "
             << tm->tm_hour << ":" << tm->tm_min << ":" << tm->tm_sec
             << "." << (this->nanoseconds % 1000);
    }
  }
}
