/* atomic_append.hpp
Efficient many actor read-write lock
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: March 2016


Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef BOOST_AFIO_SHARED_FS_MUTEX_ATOMIC_APPEND_HPP
#define BOOST_AFIO_SHARED_FS_MUTEX_ATOMIC_APPEND_HPP

#include "../../file_handle.hpp"
#include "base.hpp"

BOOST_AFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  namespace shared_fs_mutex
  {
    namespace atomic_append_detail
    {
#pragma pack(push)
#pragma pack(1)
      struct alignas(16) header
      {
        uint128 hash;                   // Hash of remaining 112 bytes
        uint64 _zero;                   // Zero
        uint64 time_offset;             // time_t in microseconds at time of creation. Used to offset us_count below.
        uint64 first_known_good;        // offset to first known good lock_request
        uint64 first_after_hole_punch;  // offset to first byte after last hole punch
        uint64 first_last_user_detect;  // Byte range locked to detect first and last user of the lock file
        uint64 _padding[9];
      };
      static_assert(sizeof(header) == 128, "header structure is not 128 bytes long!");

      struct alignas(16) lock_request
      {
        uint128 hash;                               // Hash of remaining 112 bytes
        uint64 unique_id;                           // A unique id identifying this locking instance
        uint64 us_count : 56;                       // Microseconds since the lock file created
        uint64 items : 8;                           // The number of entities below which are valid
        shared_fs_mutex::entity_type entities[12];  // Entities to exclusive or share lock
      };
      static_assert(sizeof(lock_request) == 128, "lock_request structure is not 128 bytes long!");
#pragma pack(pop)
    }
    /*! \class atomic_append
    \brief Scalable many entity shared/exclusive file system based lock

    Lock files and byte ranges scale poorly to the number of items being concurrently locked with typically an exponential
    drop off in performance as the number of items being concurrently locked rises. This file system
    algorithm solves this problem using IPC via a shared append-only lock file.

    - Compatible with networked file systems (NFS too if the special nfs_compatibility flag is true).
    - Invariant complexity to number of entities being locked.
    - Linear complexity to number of processes concurrently using the lock (i.e. number of waiters).
    - Very fast skip code path for when there is a just a single user.
    - Optionally can sleep until a lock becomes free in a power-efficient manner.
    - Sudden process exit with lock held causes a stall of a second or so to the other users, but
    is recovered correctly.
    - Sudden power loss during use is recovered from.

    Caveats:
    - Maximum of twelve entities may be locked concurrently.
    - Wasteful of disk space if used on a non-extents based filing system (e.g. FAT32, ext3).
    It is best used in `/tmp` if possible. If you really must use a non-extents based filing
    system, destroy and recreate the object instance periodically to force resetting the lock
    file's length to zero.
    - Similarly older operating systems (e.g. Linux < 3.0) do not implement extent hole punching
    and therefore will also see excessive disk space consumption. Note at the time of writing
    OS X doesn't implement hole punching at all.
    - Byte range locks need to work properly on your system. Misconfiguring NFS or Samba
    to cause byte range locks to not work right will produce bad outcomes.
    - If your OS doesn't have sane byte range locks (OS X, BSD, older Linuxes) and multiple
    threads in your process use the same lock file, bad things will happen due to the lack
    of sanity in POSIX byte range locks. This may get fixed with a process-local byte range
    lock implementation if there is demand, otherwise simply don't use the same lock file
    from more than one thread on such systems.
    */
    class atomic_append : public shared_fs_mutex
    {
      file_handle _h;

      atomic_append() {}
      atomic_append(const atomic_append &) = delete;
      atomic_append &operator=(const atomic_append &) = delete;

    public:
      //! The type of an entity id
      using entity_type = shared_fs_mutex::entity_type;
      //! The type of a sequence of entities
      using entities_type = shared_fs_mutex::entities_type;

      //! Move constructor
      atomic_append(atomic_append &&o) noexcept : _h(std::move(o._h)) {}
      //! Move assign
      atomic_append &operator=(atomic_append &&o) noexcept
      {
        _h = std::move(o._h);
        return *this;
      }

      //! Initialises a shared filing system mutex using the file at \em lockfile
      //[[bindlib::make_free]]
      static result<atomic_append> fs_mutex_append(file_handle::path_type lockfile) noexcept {}

      //! Return the handle to file being used for this lock
      const file_handle &handle() const noexcept { return _h; }

    protected:
      virtual result<void> _lock(entities_guard &out, deadline d) noexcept override final {}
    public:
      virtual void unlock(entities_type entities) noexcept override final {}
    };

  }  // namespace
}  // namespace

BOOST_AFIO_V2_NAMESPACE_END


#endif