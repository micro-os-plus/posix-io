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

#include "posix-io/FileDescriptorsManager.h"
#include "posix-io/IO.h"
#include "posix-io/File.h"
#include "posix-io/FileSystem.h"
#include "posix-io/TPool.h"
#include "posix-io/MountManager.h"
#include "posix-io/BlockDevice.h"
#include "posix-io/Socket.h"
#include <cerrno>
#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#if defined(OS_INCLUDE_TRACE_PRINTF)
#include "diag/Trace.h"
#endif
#include "utime.h"

// ----------------------------------------------------------------------------

enum class Cmds
  : unsigned int
    { UNKNOWN,
  NOTSET,
  SYNC,
  CHMOD,
  STAT,
  TRUNCATE,
  RENAME,
  UNLINK,
  UTIME,
  MKDIR,
  RMDIR,
  OPEN,
  CLOSE,
  READ,
  WRITE,
  IOCTL,
  LSEEK,
  ISATTY,
  FCNTL,
  FSTAT,
  FTRUNCATE,
  FSYNC
};

// Test class, all methods store the input in local variables,
// to be checked later.

class TestSocket : public os::posix::Socket
{
public:

  TestSocket ();

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

  virtual bool
  tryMatch(int domain, int type, int protocol);

  virtual int
  do_socket (int domain, int type, int protocol) override;

  virtual int
  do_close (void) override;

  virtual ssize_t
  do_read (void* buf, std::size_t nbyte) override;

  virtual ssize_t
  do_write (const void* buf, std::size_t nbyte) override;

  ssize_t
  do_writev (const struct iovec* iov, int iovcnt) override;

  virtual int
  do_ioctl (int request, std::va_list args) override;

  virtual int
  do_fcntl (int cmd, va_list args) override;

  Socket*
  do_accept (struct sockaddr* address, socklen_t* address_len) override;

  int
  do_bind (const struct sockaddr* address, socklen_t address_len) override;

  int
  do_connect (const struct sockaddr* address, socklen_t address_len) override;

  int
  do_getpeername (struct sockaddr* address, socklen_t* address_len) override;

  int
  do_getsockname (struct sockaddr* address, socklen_t* address_len) override;

  int
  do_getsockopt (int level, int option_name, void* option_value,
                 socklen_t* option_len) override;

  int
  do_listen (int backlog) override;

  ssize_t
  do_recv (void* buffer, size_t length, int flags) override;

  ssize_t
  do_recvfrom (void* buffer, size_t length, int flags,
               struct sockaddr* address, socklen_t* address_len) override;

  ssize_t
  do_recvmsg (struct msghdr* message, int flags) override;

  ssize_t
  do_send (const void* buffer, size_t length, int flags) override;

  ssize_t
  do_sendmsg (const struct msghdr* message, int flags) override;

  ssize_t
  do_sendto (const void* message, size_t length, int flags,
             const struct sockaddr* dest_addr, socklen_t dest_len) override;

  int
  do_setsockopt (int level, int option_name, const void* option_value,
                 socklen_t option_len) override;

  int
  do_shutdown (int how) override;

  int
  do_sockatmark (void) override;

private:

  uint32_t fSomething;
  const char* fPath;
  int fMode;
  unsigned int fNumber;
  void* fPtr;
  Cmds fCmd;

};

TestSocket::TestSocket ()
{
  fCmd = Cmds::NOTSET;
  fPath = nullptr;
  fMode = -1;
  fSomething = 1;
  fNumber = 1;
  fPtr = nullptr;
}

inline Cmds
TestSocket::getCmd (void)
{
  return fCmd;
}

inline unsigned int
TestSocket::getNumber (void)
{
  return fNumber;
}

inline int
TestSocket::getMode (void)
{
  return fMode;
}

inline const char*
TestSocket::getPath (void)
{
  return fPath;
}

inline void*
TestSocket::getPtr (void)
{
  return fPtr;
}


int
TestSocket::do_close (void)
{
  fCmd = Cmds::CLOSE;
  return 0; // Always return success
}

ssize_t
TestSocket::do_read (void *buf, std::size_t nbyte)
{
  fCmd = Cmds::READ;
  fPtr = buf;
  fNumber = nbyte;
  return 0;
}

ssize_t
TestSocket::do_write (const void* buf, std::size_t nbyte)
{
  fCmd = Cmds::WRITE;
  fPtr = (void*) buf;
  fNumber = nbyte;
  return 0;
}

int
TestSocket::do_ioctl (int request, std::va_list args)
{
  fCmd = Cmds::IOCTL;
  fNumber = request;
  fMode = va_arg(args, int);
  return 0;
}


int
TestSocket::do_fcntl (int cmd, std::va_list args)
{
  fCmd = Cmds::FCNTL;
  fNumber = cmd;
  fMode = va_arg(args, int);
  return 0;
}


Socket*
TestSocket::do_accept (struct sockaddr* address, socklen_t* address_len)
{
  errno = ENOSYS; // Not implemented
  return nullptr;
}

int
TestSocket::do_bind (const struct sockaddr* address, socklen_t address_len)
{
  errno = ENOSYS; // Not implemented
  return -1;
}

int
TestSocket::do_connect (const struct sockaddr* address, socklen_t address_len)
{
  errno = ENOSYS; // Not implemented
  return -1;
}

int
TestSocket::do_getpeername (struct sockaddr* address, socklen_t* address_len)
{
  errno = ENOSYS; // Not implemented
  return -1;
}

int
TestSocket::do_getsockname (struct sockaddr* address, socklen_t* address_len)
{
  errno = ENOSYS; // Not implemented
  return -1;
}

int
TestSocket::do_getsockopt (int level, int option_name, void* option_value,
                       socklen_t* option_len)
{
  errno = ENOSYS; // Not implemented
  return -1;
}

int
TestSocket::do_listen (int backlog)
{
  errno = ENOSYS; // Not implemented
  return -1;
}

ssize_t
TestSocket::do_recv (void* buffer, size_t length, int flags)
{
  errno = ENOSYS; // Not implemented
  return -1;
}

ssize_t
TestSocket::do_recvfrom (void* buffer, size_t length, int flags,
                     struct sockaddr* address, socklen_t* address_len)
{
  errno = ENOSYS; // Not implemented
  return -1;
}

ssize_t
TestSocket::do_recvmsg (struct msghdr* message, int flags)
{
  errno = ENOSYS; // Not implemented
  return -1;
}

ssize_t
TestSocket::do_send (const void* buffer, size_t length, int flags)
{
  errno = ENOSYS; // Not implemented
  return -1;
}

ssize_t
TestSocket::do_sendmsg (const struct msghdr* message, int flags)
{
  errno = ENOSYS; // Not implemented
  return -1;
}

ssize_t
TestSocket::do_sendto (const void* message, size_t length, int flags,
                   const struct sockaddr* dest_addr, socklen_t dest_len)
{
  errno = ENOSYS; // Not implemented
  return -1;
}

int
TestSocket::do_setsockopt (int level, int option_name, const void* option_value,
                       socklen_t option_len)
{
  errno = ENOSYS; // Not implemented
  return -1;
}

int
TestSocket::do_shutdown (int how)
{
  errno = ENOSYS; // Not implemented
  return -1;
}

int
TestSocket::do_sockatmark (void)
{
  errno = ENOSYS; // Not implemented
  return -1;
}



// ----------------------------------------------------------------------------

using TestFilePool = os::posix::TPool<TestSocket>;

constexpr std::size_t FILES_POOL_ARRAY_SIZE = 2;

// Pool of File objects, used in common by all filesystems.
TestFilePool filesPool
  { FILES_POOL_ARRAY_SIZE };

// File systems, all using the same pool.
TestFileSystem root_fs
  { &filesPool, nullptr };
TestFileSystem fs1
  { &filesPool, nullptr };
TestFileSystem fs2
  { &filesPool, nullptr };

// Static manager
os::posix::FileDescriptorsManager dm
  { 5 };

// Static manager
os::posix::MountManager mm
  { 2 };

// Block devices, just referenced, no calls forwarded to them.
TestBlockDevice root_dev;
TestBlockDevice dev1;
TestBlockDevice dev2;

// ----------------------------------------------------------------------------

extern "C"
{
  int
  __posix_chmod (const char* path, mode_t mode);

  int
  __posix_stat (const char* path, struct stat* buf);

  int
  __posix_truncate (const char* path, off_t length);

  int
  __posix_rename (const char* existing, const char* _new);

  int
  __posix_unlink (const char* path);

  int
  __posix_utime (const char* path, const struct utimbuf* times);

  // -----

  int
  __posix_mkdir (const char* path, mode_t mode);

  int
  __posix_rmdir (const char* path);

  void
  __posix_sync (void);

  // -----

  int
  __posix_open (const char* path, int oflag, ...);

  int
  __posix_close (int fildes);

  ssize_t
  __posix_read (int fildes, void* buf, size_t nbyte);

  ssize_t
  __posix_write (int fildes, const void* buf, size_t nbyte);

  int
  __posix_ioctl (int fildes, int request, ...);

  off_t
  __posix_lseek (int fildes, off_t offset, int whence);

  int
  __posix_isatty (int fildes);

  int
  __posix_fcntl (int fildes, int cmd, ...);

  int
  __posix_fstat (int fildes, struct stat* buf);

  int
  __posix_ftruncate (int fildes, off_t length);

  int
  __posix_fsync (int fildes);

}

// ----------------------------------------------------------------------------

int
main (int argc __attribute__((unused)), char* argv[] __attribute__((unused)))
{
    {
      // ----- MountManager -----

      // Check initial size.
      assert(mm.getSize () == 2);

      // Check if FileSystemsManger is empty.
      for (std::size_t i = 0; i < mm.getSize (); ++i)
        {
          assert(mm.getFileSystem (i) == nullptr);
          assert(mm.getPath (i) == nullptr);
        }
      assert(mm.getRoot () == nullptr);

      const char* path1 = "/babu/riba";
      const char* path2 = path1;

      // No file system, identify nothing
      assert(
          os::posix::MountManager::identifyFileSystem (&path2, nullptr)
              == nullptr);

      // Check if root_fs file system flags are those set by constructor.
      assert(root_fs.getFlags () == 1);

      // Check setRoot(), and mount().
      assert(os::posix::MountManager::setRoot (&root_fs, &root_dev, 123) == 0);
      assert(os::posix::MountManager::getRoot () == &root_fs);
      assert(root_fs.getBlockDevice () == &root_dev);

      // Check mount flags.
      assert(root_fs.getFlags () == 123);

      // No file systems mounted, identify root.
      assert(
          os::posix::MountManager::identifyFileSystem (&path2, nullptr)
              == &root_fs);
      assert(path2 == path1);
    }

    {
      // ----- MountManager mounts & umounts -----

      errno = -2;
      assert(
          (os::posix::MountManager::mount (&fs1, "/fs1/", &dev1, 124) == 0) && (errno == 0));
      assert(os::posix::MountManager::getFileSystem (0) == &fs1);
      assert(fs1.getBlockDevice () == &dev1);

      assert(fs1.getFlags () == 124);

      // Check not mounted file, should return root
      const char* path1 = "/baburiba";
      const char* path2 = path1;

      assert(
          os::posix::MountManager::identifyFileSystem (&path2, nullptr)
              == &root_fs);
      assert(path2 == path1);

      // Check busy error
      errno = -2;
      assert(os::posix::MountManager::mount (&fs1, "/fs1/", &dev1, 124) == -1);
      assert(errno == EBUSY);

      path1 = "/fs1/babu";
      path2 = path1;

      const char* path3 = "/fs1/riba";
      const char* path4 = path3;

      // Check if identified properly
      assert(
          os::posix::MountManager::identifyFileSystem (&path2, &path4) == &fs1);

      // Check if path adjusted properly
      assert(path2 == (path1 + std::strlen ("/fs1")));
      assert(path4 == (path3 + std::strlen ("/fs1")));

      // Check size exceeded
      errno = -2;
      assert(
          (os::posix::MountManager::mount (&fs2, "/fs2/", &dev2, 124) == 0) && (errno == 0));
      errno = -2;
      assert(os::posix::MountManager::mount (&fs2, "/fs3/", &dev2, 124) == -1);
      assert(errno == ENOENT);

      // Check umounts
      unsigned int cnt = fs1.getSyncCount ();
      errno = -2;
      assert(
          (os::posix::MountManager::umount ("/fs1/", 134) == 0) && (errno == 0));
      assert(fs1.getFlags () == 134);
      assert(fs1.getSyncCount () == cnt + 1);
      assert(fs1.getBlockDevice () == nullptr);

      // Check umounts
      cnt = fs2.getSyncCount ();
      errno = -2;
      assert(
          (os::posix::MountManager::umount ("/fs2/", 144) == 0) && (errno == 0));
      assert(fs2.getFlags () == 144);
      assert(fs2.getSyncCount () == cnt + 1);
      assert(fs2.getBlockDevice () == nullptr);
    }

    {
      // Mount again
      errno = -2;
      assert(
          (os::posix::MountManager::mount (&fs1, "/fs1/", &dev1, 124) == 0) && (errno == 0));
    }

    {
      // C API

      // CHMOD
      errno = -2;
      assert((__posix_chmod ("/fs1/p1", 321) == 0) && (errno == 0));
      assert(fs1.getCmd () == Cmds::CHMOD);
      assert(fs1.getNumber () == 321);
      assert(std::strcmp ("/p1", fs1.getPath ()) == 0);

      // STAT
      errno = -2;
      struct stat stat_buf;
      assert((__posix_stat ("/fs1/p2", &stat_buf) == 0) && (errno == 0));
      assert(fs1.getCmd () == Cmds::STAT);
      assert(fs1.getPtr () == &stat_buf);
      assert(std::strcmp ("/p2", fs1.getPath ()) == 0);

      // TRUNCATE
      errno = -2;
      assert((__posix_truncate ("/fs1/p3", 876) == 0) && (errno == 0));
      assert(fs1.getCmd () == Cmds::TRUNCATE);
      assert(fs1.getNumber () == 876);
      assert(std::strcmp ("/p3", fs1.getPath ()) == 0);

      // RENAME
      errno = -2;
      assert((__posix_rename ("/fs1/p4", "/fs1/p4-new") == 0) && (errno == 0));
      assert(fs1.getCmd () == Cmds::RENAME);
      assert(std::strcmp ("/p4", fs1.getPath ()) == 0);
      assert(std::strcmp ("/p4-new", (const char* )fs1.getPtr ()) == 0);

      // UNLINK
      errno = -2;
      assert((__posix_unlink ("/fs1/p5") ==0) && (errno == 0));
      assert(fs1.getCmd () == Cmds::UNLINK);
      assert(std::strcmp ("/p5", fs1.getPath ()) == 0);

      // UTIME
      errno = -2;
      struct utimbuf times;
      assert((__posix_utime ("/fs1/p6", &times) ==0) && (errno == 0));
      assert(fs1.getCmd () == Cmds::UTIME);
      assert(fs1.getPtr () == &times);
      assert(std::strcmp ("/p6", fs1.getPath ()) == 0);

      // MKDIR
      errno = -2;
      assert((__posix_mkdir ("/fs1/p7", 654) ==0) && (errno == 0));
      assert(fs1.getCmd () == Cmds::MKDIR);
      assert(fs1.getNumber () == 654);
      assert(std::strcmp ("/p7", fs1.getPath ()) == 0);

      // RMDIR
      errno = -2;
      assert((__posix_rmdir ("/fs1/p8") ==0) && (errno == 0));
      assert(fs1.getCmd () == Cmds::RMDIR);
      assert(std::strcmp ("/p8", fs1.getPath ()) == 0);

      // SYNC
      unsigned int cnt = fs1.getSyncCount ();
      errno = -2;
      __posix_sync ();
      assert(errno == 0);
      assert(fs1.getCmd () == Cmds::RMDIR);
      assert(fs1.getSyncCount () == cnt + 1);
    }

    {
      // C++ API

      // CHMOD
      errno = -2;
      assert((os::posix::chmod ("/fs1/p1", 321) == 0) && (errno == 0));
      assert(fs1.getCmd () == Cmds::CHMOD);
      assert(fs1.getNumber () == 321);
      assert(std::strcmp ("/p1", fs1.getPath ()) == 0);

      // STAT
      errno = -2;
      struct stat stat_buf;
      assert((os::posix::stat ("/fs1/p2", &stat_buf) == 0) && (errno == 0));
      assert(fs1.getCmd () == Cmds::STAT);
      assert(fs1.getPtr () == &stat_buf);
      assert(std::strcmp ("/p2", fs1.getPath ()) == 0);

      // TRUNCATE
      errno = -2;
      assert((os::posix::truncate ("/fs1/p3", 876) == 0) && (errno == 0));
      assert(fs1.getCmd () == Cmds::TRUNCATE);
      assert(fs1.getNumber () == 876);
      assert(std::strcmp ("/p3", fs1.getPath ()) == 0);

      // RENAME
      errno = -2;
      assert(
          (os::posix::rename ("/fs1/p4", "/fs1/p4-new") == 0) && (errno == 0));
      assert(fs1.getCmd () == Cmds::RENAME);
      assert(std::strcmp ("/p4", fs1.getPath ()) == 0);
      assert(std::strcmp ("/p4-new", (const char* )fs1.getPtr ()) == 0);

      // UNLINK
      errno = -2;
      assert((os::posix::unlink ("/fs1/p5") ==0) && (errno == 0));
      assert(fs1.getCmd () == Cmds::UNLINK);
      assert(std::strcmp ("/p5", fs1.getPath ()) == 0);

      // UTIME
      errno = -2;
      struct utimbuf times;
      assert((os::posix::utime ("/fs1/p6", &times) ==0) && (errno == 0));
      assert(fs1.getCmd () == Cmds::UTIME);
      assert(fs1.getPtr () == &times);
      assert(std::strcmp ("/p6", fs1.getPath ()) == 0);

      // MKDIR
      errno = -2;
      assert((os::posix::mkdir ("/fs1/p7", 654) ==0) && (errno == 0));
      assert(fs1.getCmd () == Cmds::MKDIR);
      assert(fs1.getNumber () == 654);
      assert(std::strcmp ("/p7", fs1.getPath ()) == 0);

      // RMDIR
      errno = -2;
      assert((os::posix::rmdir ("/fs1/p8") ==0) && (errno == 0));
      assert(fs1.getCmd () == Cmds::RMDIR);
      assert(std::strcmp ("/p8", fs1.getPath ()) == 0);

      // SYNC
      unsigned int cnt = fs1.getSyncCount ();
      errno = -2;
      os::posix::sync ();
      assert(errno == 0);
      assert(fs1.getCmd () == Cmds::RMDIR);
      assert(fs1.getSyncCount () == cnt + 1);
    }

    {
      // C API

      // Test OPEN
      errno = -2;
      int fd = __posix_open ("/fs1/f1", 123, 234);
      assert((fd >= 0) && (errno == 0));

      os::posix::IO* io = os::posix::FileDescriptorsManager::getIo (fd);
      assert(io != nullptr);

      assert(io->getType () == os::posix::IO::Type::FILE);

      TestSocket* file = static_cast<TestSocket*> (io);
      // Must be the first used slot in the pool.
      assert(filesPool.getObject (0) == file);
      assert(filesPool.getFlag (0) == true);

      // Check params passing.
      assert(std::strcmp ("/f1", file->getPath ()) == 0);
      assert(file->getNumber () == 123);
      assert(file->getMode () == 234);

      int ret;

      // Test READ
      errno = -2;
      char buf[3];
      ret = __posix_read (fd, (void*) buf, 321);
      assert((ret == 0) && (errno == 0));
      assert(file->getCmd () == Cmds::READ);
      assert(file->getPtr () == buf);
      assert(file->getNumber () == 321);

      // Test WRITE
      errno = -2;
      ret = __posix_write (fd, (const void*) buf, 432);
      assert((ret == 0) && (errno == 0));
      assert(file->getCmd () == Cmds::WRITE);
      assert(file->getPtr () == buf);
      assert(file->getNumber () == 432);

      // Test IOCTL
      errno = -2;
      ret = __posix_ioctl (fd, 222, 876);
      assert((ret == 0) && (errno == 0));
      assert(file->getCmd () == Cmds::IOCTL);
      assert(file->getNumber () == 222);
      assert(file->getMode () == 876);

      // Test LSEEK
      errno = -2;
      ret = __posix_lseek (fd, 333, 555);
      assert((ret == 0) && (errno == 0));
      assert(file->getCmd () == Cmds::LSEEK);
      assert(file->getNumber () == 333);
      assert(file->getMode () == 555);

      // Test ISATTY
      errno = -2;
      ret = __posix_isatty (fd);
      assert((ret == 0) && (errno == 0));
      assert(file->getCmd () == Cmds::ISATTY);

      // Test FCNTL
      errno = -2;
      ret = __posix_fcntl (fd, 444, 987);
      assert((ret == 0) && (errno == 0));
      assert(file->getCmd () == Cmds::FCNTL);
      assert(file->getNumber () == 444);
      assert(file->getMode () == 987);

      // Test FSTAT
      errno = -2;
      struct stat stat_buf;
      ret = __posix_fstat (fd, &stat_buf);
      assert((ret == 0) && (errno == 0));
      assert(file->getCmd () == Cmds::FSTAT);
      assert(file->getPtr () == &stat_buf);

      // Test FTRUNCATE
      errno = -2;
      ret = __posix_ftruncate (fd, 999);
      assert((ret == 0) && (errno == 0));
      assert(file->getCmd () == Cmds::FTRUNCATE);
      assert(file->getNumber () == 999);

      // Test FSYNC
      errno = -2;
      ret = __posix_fsync (fd);
      assert((ret == 0) && (errno == 0));
      assert(file->getCmd () == Cmds::FSYNC);

      // Test CLOSE
      errno = -2;
      ret = __posix_close (fd);
      assert((ret == 0) && (errno == 0));
      assert(file->getCmd () == Cmds::CLOSE);

      // Must no longer be in the pool
      assert(filesPool.getFlag (0) == false);
    }

    {
      // C++ API

      // Test OPEN
      errno = -2;
      os::posix::IO* file = os::posix::open ("/fs1/f1", 123, 234);
      assert((file != nullptr) && (errno == 0));

      assert(file->getType () == os::posix::IO::Type::FILE);

      TestSocket* tfile = static_cast<TestSocket*> (file);
      // Must be the first used slot in the pool.
      assert(filesPool.getObject (0) == tfile);
      assert(filesPool.getFlag (0) == true);

      // Check params passing.
      assert(std::strcmp ("/f1", tfile->getPath ()) == 0);
      assert(tfile->getNumber () == 123);
      assert(tfile->getMode () == 234);

      int ret;

      // Test READ
      errno = -2;
      char buf[3];
      ret = file->read ((void*) buf, 321);
      assert((ret == 0) && (errno == 0));
      assert(tfile->getCmd () == Cmds::READ);
      assert(tfile->getPtr () == buf);
      assert(tfile->getNumber () == 321);

      // Test WRITE
      errno = -2;
      ret = file->write ((const void*) buf, 432);
      assert((ret == 0) && (errno == 0));
      assert(tfile->getCmd () == Cmds::WRITE);
      assert(tfile->getPtr () == buf);
      assert(tfile->getNumber () == 432);

      // Test IOCTL
      errno = -2;
      ret = file->ioctl (222, 876);
      assert((ret == 0) && (errno == 0));
      assert(tfile->getCmd () == Cmds::IOCTL);
      assert(tfile->getNumber () == 222);
      assert(tfile->getMode () == 876);

      // Test LSEEK
      errno = -2;
      ret = file->lseek (333, 555);
      assert((ret == 0) && (errno == 0));
      assert(tfile->getCmd () == Cmds::LSEEK);
      assert(tfile->getNumber () == 333);
      assert(tfile->getMode () == 555);

      // Test ISATTY
      errno = -2;
      ret = file->isatty ();
      assert((ret == 0) && (errno == 0));
      assert(tfile->getCmd () == Cmds::ISATTY);

      // Test FCNTL
      errno = -2;
      ret = file->fcntl (444, 987);
      assert((ret == 0) && (errno == 0));
      assert(tfile->getCmd () == Cmds::FCNTL);
      assert(tfile->getNumber () == 444);
      assert(tfile->getMode () == 987);

      // Test FSTAT
      errno = -2;
      struct stat stat_buf;
      ret = file->fstat (&stat_buf);
      assert((ret == 0) && (errno == 0));
      assert(tfile->getCmd () == Cmds::FSTAT);
      assert(tfile->getPtr () == &stat_buf);

      // Test FTRUNCATE
      errno = -2;
      ret = file->ftruncate (999);
      assert((ret == 0) && (errno == 0));
      assert(tfile->getCmd () == Cmds::FTRUNCATE);
      assert(tfile->getNumber () == 999);

      // Test FSYNC
      errno = -2;
      ret = file->fsync ();
      assert((ret == 0) && (errno == 0));
      assert(tfile->getCmd () == Cmds::FSYNC);

      // Test CLOSE
      errno = -2;
      ret = file->close ();
      assert((ret == 0) && (errno == 0));
      assert(tfile->getCmd () == Cmds::CLOSE);

      // Must no longer be in the pool
      assert(filesPool.getFlag (0) == false);
    }

  const char* msg = "'test-file-debug' succeeded.\n";
#if defined(OS_INCLUDE_TRACE_PRINTF)
  trace_puts (msg);
#else
  std::puts (msg);
#endif

  // Success!
  return 0;
}

// ----------------------------------------------------------------------------
