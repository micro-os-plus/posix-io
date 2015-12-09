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

#ifndef INCLUDE_POSIX_IO_FILE_DESCRIPTORS_MANAGER_H_
#define INCLUDE_POSIX_IO_FILE_DESCRIPTORS_MANAGER_H_

#include "posix-io/types.h"
#include <cstddef>

// ----------------------------------------------------------------------------

// This definition should be somewhere else, in a configuration header
#if !defined(OS_INTEGER_FILE_DESCRIPTORS_MANAGER_ARRAY_SIZE)
#define OS_INTEGER_FILE_DESCRIPTORS_MANAGER_ARRAY_SIZE   (10)
#endif

// ----------------------------------------------------------------------------

namespace os
{

  class FileDescriptorsManager
  {
  public:
    FileDescriptorsManager ();

    static size_t
    getSize (void);

    static bool
    checkFileDescriptor (int fildes);

    static PosixIo*
    getPosixIo (int fildes);

    static int
    allocFileDescriptor (PosixIo* io);

    static int
    freeFileDescriptor (fileDescriptor_t fildes);

    // ------------------------------------------------------------------------
  private:
    static PosixIo* openedFileDescriptors[OS_INTEGER_FILE_DESCRIPTORS_MANAGER_ARRAY_SIZE];
  };

  // --------------------------------------------------------------------------

  inline size_t
  FileDescriptorsManager::getSize (void)
  {
    return sizeof(openedFileDescriptors) / sizeof(openedFileDescriptors[0]);
  }

} // namespace os

// ----------------------------------------------------------------------------

#endif /* INCLUDE_POSIX_IO_FILE_DESCRIPTORS_MANAGER_H_ */