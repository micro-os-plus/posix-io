/*
 * This file is part of the µOS++ distribution.
 *   (https://github.com/micro-os-plus)
 * Copyright (c) 2015 Liviu Ionescu.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef POSIX_IO_CHAR_DEVICE_H_
#define POSIX_IO_CHAR_DEVICE_H_

// ----------------------------------------------------------------------------

#include "posix-io/IO.h"

// ----------------------------------------------------------------------------

#if ! defined(OS_STRING_POSIX_DEVICE_PREFIX)
#define OS_STRING_POSIX_DEVICE_PREFIX "/dev/"
#endif

// ----------------------------------------------------------------------------

namespace os
{
  namespace posix
  {
    // ------------------------------------------------------------------------

    class CharDevice : public IO
    {
      // ----------------------------------------------------------------------

      friend IO*
      vopen (const char* path, int oflag, std::va_list args);

      // ----------------------------------------------------------------------

    public:

      CharDevice (const char* name);
      CharDevice (const CharDevice&) = delete;

      virtual
      ~CharDevice ();

      int
      ioctl (int request, ...);

      int
      vioctl (int request, std::va_list args);

      // ----------------------------------------------------------------------

      virtual bool
      matchName (const char* name) const;

      const char*
      getName (void) const;

      static const char*
      getDevicePrefix (void);

      // ----------------------------------------------------------------------

    protected:

      virtual int
      do_vopen (const char* path, int oflag, std::va_list args) = 0;

      virtual int
      do_vioctl (int request, std::va_list args);

      virtual int
      do_isatty (void) override;

      // ----------------------------------------------------------------------

    protected:

      const char* fName;

    };

    // ------------------------------------------------------------------------

    inline const char*
    CharDevice::getDevicePrefix (void)
    {
      return OS_STRING_POSIX_DEVICE_PREFIX;
    }

    inline const char*
    CharDevice::getName (void) const
    {
      return fName;
    }

  } /* namespace posix */
} /* namespace os */

// ----------------------------------------------------------------------------

#endif /* POSIX_IO_CHAR_DEVICE_H_ */
