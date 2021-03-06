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

#include "posix-io/FileDescriptorsManager.h"
#include "posix-io/IO.h"
#include "posix-io/CharDevice.h"
#include <cmsis-plus/diag/trace.h>

#include <cerrno>
#include <cassert>
#include <cstdio>
#include <cstring>

// ----------------------------------------------------------------------------

// Mock class, all methods return ENOSYS, as not implemented.

class TestIO : public os::posix::IO
{
public:

  virtual int
  do_vopen (const char *path, int oflag, va_list args);

};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

int
TestIO::do_vopen (const char *path, int oflag, va_list args)
{
  errno = ENOSYS;
  return -1;
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------

// Size must be 5 for this test.
constexpr std::size_t FD_MANAGER_ARRAY_SIZE = 5;

os::posix::FileDescriptorsManager descriptorsManager
  { FD_MANAGER_ARRAY_SIZE };

TestIO test1;
TestIO test2;
TestIO test3;

// ----------------------------------------------------------------------------

int
main (int argc __attribute__((unused)), char* argv[] __attribute__((unused)))
{
  size_t sz = os::posix::FileDescriptorsManager::getSize ();
  // Size must be 5 for this test
  assert (sz == FD_MANAGER_ARRAY_SIZE);

  for (std::size_t i = 0; i < sz; ++i)
    {
      assert (os::posix::FileDescriptorsManager::getIo (i) == nullptr);
    }

  // Check limits.
  assert (os::posix::FileDescriptorsManager::isValid (-1) == false);
  assert (os::posix::FileDescriptorsManager::isValid (sz) == false);

  // Allocation should start with 3 (stdin, stdout, stderr preserved).
  int fd1;
  fd1 = os::posix::FileDescriptorsManager::alloc (&test1);
  assert (fd1 == 3);

  // Get it back; is it the same?
  assert (os::posix::FileDescriptorsManager::getIo (fd1) == &test1);
  assert (test1.getFileDescriptor () == fd1);

  // Reallocate opened file, must be busy.
  int fd2;
  fd2 = os::posix::FileDescriptorsManager::alloc (&test1);
  assert ((fd2 == -1) && (errno == EBUSY));

  // Free descriptor.
  assert (os::posix::FileDescriptorsManager::free (fd1) == 0);
  assert (os::posix::FileDescriptorsManager::getIo (fd1) == nullptr);
  assert (test1.getFileDescriptor () == os::posix::noFileDescriptor);

  // With clean table, alloc repeatedly to fill the table (size is 5).
  fd1 = os::posix::FileDescriptorsManager::alloc (&test1);
  assert (fd1 == 3);
  fd2 = os::posix::FileDescriptorsManager::alloc (&test2);
  assert (fd2 == 4);

  // Table full.
  int fd3;
  fd3 = os::posix::FileDescriptorsManager::alloc (&test3);
  assert ((fd3 == -1) && (errno == ENFILE));

  // Free outside range.
  assert (
      (os::posix::FileDescriptorsManager::free (-1) == -1) && (errno == EBADF));
  assert (
      (os::posix::FileDescriptorsManager::free (sz) == -1) && (errno == EBADF));

  // Free last.
  assert (os::posix::FileDescriptorsManager::free (sz - 1) == 0);

  // Reallocate last.
  fd3 = os::posix::FileDescriptorsManager::alloc (&test3);
  assert (fd3 == ((os::posix::fileDescriptor_t) (sz - 1)));

  trace_puts ("'test-descriptors-manager-debug' done.");

  // Success!
  return 0;
}

// ----------------------------------------------------------------------------

