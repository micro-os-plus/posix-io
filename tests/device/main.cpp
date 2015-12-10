/*
 * This file is part of the µOS++ distribution.
 *   (https://github.com/micro-os-plus)
 * Copyright (c) 2015 Liviu Ionescu.
 *
 * µOS++ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3.
 *
 * µOS++ is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "posix-io/PosixDevice.h"
#include "posix-io/FileDescriptorsManager.h"
#include "posix-io/PosixDevicesManager.h"
#include "posix-io/PosixDeviceImplementation.h"
#include <cerrno>
#include <cassert>
#include <cstdio>
#include <cstdarg>

// -----------------------------------------------------------------------------

// Test class, all methods return ENOSYS, as not implemented, except open().

class TestPosixDeviceImplementation : public os::PosixDeviceImplementation
{
public:

  TestPosixDeviceImplementation (const char* deviceName, uint32_t deviceNumber);

  virtual int
  open (const char *path, int oflag, va_list args);

  int
  getMode (void);

private:

  uint32_t fDeviceNumber;
  uint32_t fMode;

};

TestPosixDeviceImplementation::TestPosixDeviceImplementation (
    const char* deviceName, uint32_t deviceNumber) :
    PosixDeviceImplementation (deviceName)
{
  fDeviceNumber = deviceNumber;
  fMode = 0;
}

int
TestPosixDeviceImplementation::getMode (void)
{
  return fMode;
}

#if defined ( __GNUC__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

int
TestPosixDeviceImplementation::open (const char *path, int oflag, va_list args)
{
  fMode = va_arg(args, int);

  return 0;
}

#if defined ( __GNUC__ )
#pragma GCC diagnostic pop
#endif

// -----------------------------------------------------------------------------

#define DESCRIPTORS_ARRAY_SIZE (5)
os::FileDescriptorsManager descriptorsManager
  { DESCRIPTORS_ARRAY_SIZE };

#define DEVICES_ARRAY_SIZE (3)
os::PosixDevicesManager devicesManager
  { DEVICES_ARRAY_SIZE };

// This device will be mapped as "/dev/test"
TestPosixDeviceImplementation testImpl
  { "test", 1 };
os::PosixDevice test
  { testImpl };

// -----------------------------------------------------------------------------

extern "C"
{
  int
  __posix_open (const char *path, int oflag, ...);

  int
  __posix_close (int fildes);
}

// -----------------------------------------------------------------------------

int
main (int argc __attribute__((unused)), char* argv[] __attribute__((unused)))
{
  std::size_t sz = os::PosixDevicesManager::getSize ();
  assert(sz == DEVICES_ARRAY_SIZE);

  // Check if initial status is empty
  for (std::size_t i = 0; i < sz; ++i)
    {
      assert(os::PosixDevicesManager::getRegisteredDevice (i) == nullptr);
    }

  // Register device
  devicesManager.registerDevice (&test);

  // Check if first device is registered.
  assert(os::PosixDevicesManager::getRegisteredDevice (0) == &test);

  int fd;
  fd = __posix_open ("/dev/test", 0, 123);
  assert((fd >= 3) && (errno == 0));

  // Get it back; is it the same?
  assert(os::FileDescriptorsManager::getPosixIo (fd) == &test);
  assert(test.getFileDescriptor () == fd);

  // Check passing variadic mode
  assert(testImpl.getMode () == 123);

  // Close and free descriptor
  int ret = __posix_close (fd);
  assert((ret == 0) && (errno == 0));

  // Check if descriptor freed.
  assert(os::FileDescriptorsManager::getPosixIo (fd) == nullptr);
  assert(test.getFileDescriptor () == os::noFileDescriptor);

  std::printf ("'test-device-debug' done.\n");

  // Success!
  return 0;
}

// -----------------------------------------------------------------------------

