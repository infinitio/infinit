#include <elle/io/Console.hh>

#include <stdio.h>
#include <unistd.h>

#include <iostream>

namespace elle
{
  namespace io
  {

//
// ---------- methods ---------------------------------------------------------
//

    ///
    /// this method retrieves the user's input from the console.
    ///
    Status              Console::Input(String&                  text,
                                       const String&            prompt,
                                       const Console::Option    option)
    {
      // retrieve the input depending on the option.
      switch (option)
        {
        case OptionPassword:
          {
            // retrieve the input and assign it.
            std::cout << prompt << std::endl;
            if (!std::getline(std::cin, text))
            {
              if (std::cin.bad()) // IO error.
              {
                return elle::Status::Error;
              }
              else if (!std::cin.eof()) // Format error.
              {
                return elle::Status::Error;
              }
              else // Signal or EOF.
              {
                return elle::Status::Error;
              }
            }

            break;
          }
        case OptionNone:
        default:
          {
            // retrieve the input and assign it.
            std::cout << prompt << std::endl;
            if (!std::getline(std::cin, text))
            {
              if (std::cin.bad()) // IO error.
              {
                return elle::Status::Error;
              }
              else if (!std::cin.eof()) // Format error.
              {
                return elle::Status::Error;
              }
              else // Signal or EOF.
              {
                return elle::Status::Error;
              }
            }

            break;
          }
        }

      return Status::Ok;
    }

  }
}
