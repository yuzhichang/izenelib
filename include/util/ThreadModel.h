#ifndef AM_CONCEPT_THREADMODEL_H
#define AM_CONCEPT_THREADMODEL_H

#include <boost/noncopyable.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include <types.h>

NS_IZENELIB_UTIL_BEGIN

class NullLock
{
public:
    inline int acquire_read_lock()
    {
        return 0;
    }

    inline int release_read_lock()
    {
        return 0;
    }

    inline int acquire_write_lock()
    {
        return 0;
    }

    inline int release_write_lock()
    {
        return 0;
    }

};

class ReadWriteLock : private boost::noncopyable
{
private:
    boost::shared_mutex rwMutex_;
public:
    explicit ReadWriteLock()
    {}

    ~ReadWriteLock()
    {}

    /**
     * @ brief Get entity mutex.
     */
    boost::shared_mutex& get_entity()
    {
        return rwMutex_;
    }

    /**
     * @ brief Attempts to get the read lock.
     */
    inline int acquire_read_lock()
    {
        rwMutex_.lock_shared();
        return 0;
    }
    /**
     *  @ brief Attempts to get the write lock.
     */
    inline int acquire_write_lock()
    {
        rwMutex_.lock();
        return 0;
    }
    /**
     *  @ brief Attempts to release the  read lock .
     */
    inline int release_read_lock()
    {
        rwMutex_.unlock_shared();
        return 0;
    }
    /**
     * @ brief Attempts to release the write lock.
     */
    inline int release_write_lock()
    {
        rwMutex_.unlock();
        return 0;
    }
};

class RecursiveLock : private boost::noncopyable
{
private:
    boost::recursive_mutex recurMutex_;
public:
    explicit RecursiveLock()
    {}

    ~RecursiveLock()
    {
    }

    /**
     * @ brief Attempts to get the read lock.
     */
    inline int acquire_read_lock()
    {
        recurMutex_.lock();
        return 0;
    }
    /**
     *  @ brief Attempts to get the write lock.
     */
    inline int acquire_write_lock()
    {
        recurMutex_.lock();
        return 0;
    }
    /**
     *  @ brief Attempts to release the  read lock .
     */
    inline int release_read_lock()
    {
        recurMutex_.unlock();
        return 0;
    }
    /**
     * @ brief Attempts to release the write lock.
     */
    inline int release_write_lock()
    {
        recurMutex_.unlock();
        return 0;
    }
};

template<class LockType>
class ScopedReadLock
{
    LockType& lock_;
public:
    explicit ScopedReadLock( LockType& lock):lock_(lock)
    {
        lock_.acquire_read_lock();
    }
    ~ScopedReadLock()
    {
        lock_.release_read_lock();
    }
private:
    DISALLOW_COPY_AND_ASSIGN(ScopedReadLock);
};

template<class LockType>
class ScopedWriteLock
{
    LockType& lock_;
public:
    explicit ScopedWriteLock( LockType& lock):lock_(lock)
    {
        lock_.acquire_write_lock();
    }
    ~ScopedWriteLock()
    {
        lock_.release_write_lock();
    }
private:
    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLock);
};

NS_IZENELIB_UTIL_END

namespace boost
{
typedef boost::detail::try_lock_wrapper<boost::shared_mutex> shared_scoped_try_lock;
}

#endif
