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
#include "posix-io/Directory.h"
#include "posix-io/File.h"
#include "posix-io/FileSystem.h"
#include "posix-io/TPool.h"
#include "posix-io/MountManager.h"
#include "posix-io/BlockDevice.h"
#include <cmsis-plus/diag/trace.h>

#include <cerrno>
#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include "utime.h"

#if defined(__ARM_EABI__)
#include "posix-io/redefinitions.h"
#endif

// ----------------------------------------------------------------------------

enum class Cmds
  : unsigned int
    { UNKNOWN, NOTSET, OPEN, CLOSE, READ, REWIND
};

class TestFile : public os::posix::File
{
public:

  TestFile ();

  // Methods used for test purposes only.
  Cmds
  getCmd (void);

  int
  getMode (void);

  const char*
  getPath (void);

  unsigned int
  getNumber (void);

  void*
  getPtr (void);

protected:

  // Implementations.

  virtual int
  do_vopen (const char* path, int oflag, std::va_list args) override;

  virtual int
  do_close (void) override;

private:

  const char* fPath;
  int fMode;
  unsigned int fNumber;
  Cmds fCmd;

};

TestFile::TestFile ()
{
  fCmd = Cmds::NOTSET;
  fPath = nullptr;
  fMode = -1;
  fNumber = 1;
}

inline Cmds
TestFile::getCmd (void)
{
  return fCmd;
}

inline unsigned int
TestFile::getNumber (void)
{
  return fNumber;
}

inline int
TestFile::getMode (void)
{
  return fMode;
}

inline const char*
TestFile::getPath (void)
{
  return fPath;
}

int
TestFile::do_vopen (const char* path, int oflag, std::va_list args)
{
  fPath = path;

  fNumber = oflag;

  fMode = va_arg(args, int);
  return 0;
}

int
TestFile::do_close (void)
{
  fCmd = Cmds::CLOSE;
  return 0; // Always return success
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

// Test class, all methods store the input in local variables,
// to be checked later.

class TestDir : public os::posix::Directory
{
public:

  TestDir ();

  // Methods used for test purposes only.
  Cmds
  getCmd (void);

  const char*
  getPath (void);

protected:

  // Implementations.

  virtual Directory*
  do_vopen (const char* dirname) override;

  virtual struct dirent*
  do_read (void) override;

  virtual void
  do_rewind (void) override;

  virtual int
  do_close (void) override;

private:

  const char* fPath;
  Cmds fCmd;
  unsigned int fCount;

};

#pragma GCC diagnostic pop

TestDir::TestDir ()
{
  fCmd = Cmds::NOTSET;
  fPath = nullptr;
  fCount = 0;
}

inline Cmds
TestDir::getCmd (void)
{
  return fCmd;
}

inline const char*
TestDir::getPath (void)
{
  return fPath;
}

os::posix::Directory*
TestDir::do_vopen (const char* dirname)
{
  fPath = dirname;
  fCount = 1;

  return this;
}

int
TestDir::do_close (void)
{
  fCmd = Cmds::CLOSE;
  return 0; // Always return success
}

struct dirent*
TestDir::do_read ()
{
  fCmd = Cmds::READ;
  sprintf (getDirEntry ()->d_name, "file%d", fCount);

  fCount++;
  return getDirEntry ();
}

void
TestDir::do_rewind (void)
{
  fCmd = Cmds::REWIND;
  fCount = 1;

  return;
}

// ----------------------------------------------------------------------------

class TestFileSystem : public os::posix::FileSystem
{
public:

  TestFileSystem (os::posix::Pool* filesPool, os::posix::Pool* dirsPool);

  // Methods used for test purposes only.
  unsigned int
  getFlags (void);

  Cmds
  getCmd (void);

  unsigned int
  getSyncCount (void);

protected:

  // Implementations.

  virtual int
  do_mount (unsigned int flags) override;

  virtual int
  do_unmount (unsigned int flags) override;

  virtual void
  do_sync (void) override;

private:

  unsigned int fMountFlags;
  Cmds fCmd;
  unsigned int fSyncCount;
};

TestFileSystem::TestFileSystem (os::posix::Pool* filesPool,
                                os::posix::Pool* dirsPool) :
    os::posix::FileSystem (filesPool, dirsPool)
{
  fMountFlags = 1;
  fCmd = Cmds::NOTSET;
  fSyncCount = 1;
}

inline unsigned int
TestFileSystem::getFlags (void)
{
  return fMountFlags;
}

inline Cmds
TestFileSystem::getCmd (void)
{
  return fCmd;
}

inline unsigned int
TestFileSystem::getSyncCount (void)
{
  return fSyncCount;
}

int
TestFileSystem::do_mount (unsigned int flags)
{
  fMountFlags = flags;

  return 0;
}

int
TestFileSystem::do_unmount (unsigned int flags)
{
  fMountFlags = flags;

  return 0;
}

void
TestFileSystem::do_sync (void)
{
  fSyncCount++;
}

// ----------------------------------------------------------------------------

// Required only as a reference, no functionality needed.
class TestBlockDevice : public os::posix::BlockDevice
{
public:
  TestBlockDevice () = default;
};

// ----------------------------------------------------------------------------

using TestFilePool = os::posix::TPool<TestFile>;

constexpr std::size_t FILES_POOL_ARRAY_SIZE = 2;

// Pool of File objects, used in common by all filesystems.
TestFilePool filesPool
  { FILES_POOL_ARRAY_SIZE };

using TestDirPool = os::posix::TPool<TestDir>;

constexpr std::size_t DIRS_POOL_ARRAY_SIZE = 2;

// Pool of Dir objects, used in common by all filesystems.
TestDirPool dirsPool
  { DIRS_POOL_ARRAY_SIZE };

// File systems, all using the same pool.
TestFileSystem root (&filesPool, &dirsPool);
TestFileSystem babu (&filesPool, &dirsPool);

// Static manager
os::posix::FileDescriptorsManager dm
  { 5 };

// Static manager
os::posix::MountManager fsm
  { 2 };

// Block devices, just referenced, no calls forwarded to them.
TestBlockDevice rootDevice;
TestBlockDevice babuDevice;
TestBlockDevice babuDevice2;

// ----------------------------------------------------------------------------

int
main (int argc __attribute__((unused)), char* argv[] __attribute__((unused)))
{
    {
      // Mount
      errno = -2;
      assert(
          (os::posix::MountManager::mount (&babu, "/babu/", &babuDevice, 124) == 0) && (errno == 0));
    }

    {
      // Test OPEN
      errno = -2;
      DIR* pdir = __posix_opendir ("/babu/d1");
      assert((pdir != nullptr) && (errno == 0));

      TestDir* dir = reinterpret_cast<TestDir*> (pdir);
      // Must be the first used slot in the pool.
      assert(dirsPool.getObject (0) == dir);
      assert(dirsPool.getFlag (0) == true);

      // Check params passing.
      assert(std::strcmp ("/d1", dir->getPath ()) == 0);

      // Test READ
      errno = -2;
      struct dirent* de = __posix_readdir (pdir);
      assert((de != nullptr) && (errno == 0));
      assert(dir->getCmd () == Cmds::READ);

      // Test REWIND
      errno = -2;
      __posix_rewinddir (pdir);
      assert((errno == 0));
      assert(dir->getCmd () == Cmds::REWIND);

      // Test CLOSE
      errno = -2;
      int ret = __posix_closedir (pdir);
      assert((ret == 0) && (errno == 0));
      assert(dir->getCmd () == Cmds::CLOSE);

      // Must no longer be in the pool
      assert(dirsPool.getFlag (0) == false);
    }

    {
      // C API

      // Test OPEN
      errno = -2;
      DIR* pdir = __posix_opendir ("/babu/d1");
      assert((pdir != nullptr) && (errno == 0));

      TestDir* dir = reinterpret_cast<TestDir*> (pdir);
      // Must be the first used slot in the pool.
      assert(dirsPool.getObject (0) == dir);
      assert(dirsPool.getFlag (0) == true);

      // Check params passing.
      assert(std::strcmp ("/d1", dir->getPath ()) == 0);

      // Test READ
      errno = -2;
      struct dirent* de = __posix_readdir (pdir);
      assert((de != nullptr) && (errno == 0));
      assert(dir->getCmd () == Cmds::READ);

      // Test REWIND
      errno = -2;
      __posix_rewinddir (pdir);
      assert((errno == 0));
      assert(dir->getCmd () == Cmds::REWIND);

      // Test CLOSE
      errno = -2;
      int ret = __posix_closedir (pdir);
      assert((ret == 0) && (errno == 0));
      assert(dir->getCmd () == Cmds::CLOSE);

      // Must no longer be in the pool
      assert(dirsPool.getFlag (0) == false);
    }

    {
      // C++ API

      // Test OPEN
      errno = -2;
      os::posix::Directory* dir = os::posix::opendir ("/babu/d2");
      assert((dir != nullptr) && (errno == 0));

      // Must be the first used slot in the pool.
      assert(dirsPool.getObject (0) == dir);
      assert(dirsPool.getFlag (0) == true);

      TestDir* tdir = static_cast<TestDir*> (dir);

      // Check params passing.
      assert(std::strcmp ("/d2", tdir->getPath ()) == 0);

      // Test READ
      errno = -2;
      struct dirent* de = dir->read ();
      assert((de != nullptr) && (errno == 0));
      assert(tdir->getCmd () == Cmds::READ);
      assert(std::strcmp (dir->getName (), "file1") == 0);

      // Second READ
      de = dir->read ();
      assert((de != nullptr) && (errno == 0));
      assert(tdir->getCmd () == Cmds::READ);
      assert(std::strcmp (dir->getName (), "file2") == 0);

      // Test REWIND
      errno = -2;
      dir->rewind ();
      assert((errno == 0));
      assert(tdir->getCmd () == Cmds::REWIND);

      // Test READ after REWIND
      de = dir->read ();
      assert((de != nullptr) && (errno == 0));
      assert(tdir->getCmd () == Cmds::READ);
      assert(std::strcmp (dir->getName (), "file1") == 0);

      // Test CLOSE
      errno = -2;
      int ret = dir->close ();
      assert((ret == 0) && (errno == 0));
      assert(tdir->getCmd () == Cmds::CLOSE);

      // Must no longer be in the pool
      assert(dirsPool.getFlag (0) == false);
    }

  trace_puts ("'test-directory-debug' succeeded.");

  // Success!
  return 0;
}

// ----------------------------------------------------------------------------

