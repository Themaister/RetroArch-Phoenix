#ifndef NALL_THREAD_HPP
#define NALL_THREAD_HPP

#include <utility>
#include <functional>
#include <assert.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <iostream>
#endif

namespace nall {
  class mutex {
    public:
      mutex() {
#ifdef _WIN32
        InitializeCriticalSection(&m_lock);
#else
        pthread_mutex_init(&m_lock, 0);
#endif
      }

      void operator=(const mutex&) = delete;
      mutex(const mutex&) = delete;

      void lock() {
#ifdef _WIN32
        EnterCriticalSection(&m_lock);
#else
        pthread_mutex_lock(&m_lock);
#endif
      }

      void unlock() {
#ifdef _WIN32
        LeaveCriticalSection(&m_lock);
#else
        pthread_mutex_unlock(&m_lock);
#endif
      }

      ~mutex() {
#ifdef _WIN32
        DeleteCriticalSection(&m_lock);
#else
        pthread_mutex_destroy(&m_lock);
#endif
      }

    private:
#ifdef _WIN32
      CRITICAL_SECTION m_lock;
#else
      pthread_mutex_t m_lock;
#endif
  };

  class scoped_lock
  {
    public:
      scoped_lock(mutex &lock) : m_lock(lock), locked(true) { m_lock.lock(); }
      void unlock() { if(locked) m_lock.unlock(); locked = false; } // Early unlock
      ~scoped_lock() { if(locked) m_lock.unlock(); }
    private:
      mutex &m_lock;
      bool locked;
  };

  // Events using Win32 model where a signaled object stays signaled until it is waited on.
  // Less flexible, but more convenient.
  // Also avoids some hard-to-debug race conditions on wait/sleep producer/consumer patterns.
  class event {
    public:
      event() {
#ifdef _WIN32
        evnt = CreateEvent(0, false, false, 0);
#else
        pthread_cond_init(&cond, 0);
        pthread_mutex_init(&lock, 0);
#endif
      }

      void operator=(const event&) = delete;
      event(const event&) = delete;

      ~event() {
#ifdef _WIN32
        CloseHandle(evnt);
#else
        pthread_cond_destroy(&cond);
        pthread_mutex_destroy(&lock);
#endif
      }

      bool wait(bool block = true) {
#ifdef _WIN32
        return WaitForSingleObject(evnt, block ? INFINITE : 0) == WAIT_OBJECT_0;
#else
        pthread_mutex_lock(&lock);
        if(signaled) {
          signaled = false;
          pthread_mutex_unlock(&lock);
          return true;
        }

        bool ret = true;
        if(!block)
          ret = signaled;
        else
          pthread_cond_wait(&cond, &lock);

        signaled = false;
        pthread_mutex_unlock(&lock);
        return ret;
#endif
      }

      void signal() {
#ifdef _WIN32
        SetEvent(evnt);
#else
        pthread_mutex_lock(&lock);
        pthread_cond_signal(&cond);
        signaled = true;
        pthread_mutex_unlock(&lock);
#endif
      }

    private:
#ifdef _WIN32
      HANDLE evnt;
#else
      pthread_cond_t cond;
      pthread_mutex_t lock;
      bool signaled;
#endif
  };

  namespace internal {
    struct callable {
      virtual void run() = 0;
    };

    // We let the thread entry take ownership of the call object to avoid
    // ugly race conditions where a very early delete or move constructor of nall::thread might cause
    // callobj here to be invalidated before it gets to call the function.
#ifdef _WIN32
    static DWORD CALLBACK thread_entry(void *data) {
      internal::callable *callobj = reinterpret_cast<internal::callable*>(data);
      callobj->run();
      delete callobj;

      return 0;
    }
#else
    extern "C"
    {
      static void* thread_entry(void *data) {
        internal::callable *callobj = reinterpret_cast<internal::callable*>(data);
        callobj->run();
        delete callobj;

        pthread_exit(0);
      }
    }
#endif
  }

  class thread
  {
    public:
      thread() : is_running(false), is_detached(true) {}

      template <typename... P>
      thread(P&&... args) : is_running(false), is_detached(false) {
        auto obj = std::bind(std::forward<P>(args)...);
        start_thread(new fn_callable<decltype(obj)>(obj));
      }

      thread(thread&& in_thread) { *this = std::move(in_thread); }

      thread& operator=(thread&& in_thread) {
        detach();
        is_running = in_thread.is_running;
        is_detached = in_thread.is_detached;

        in_thread.is_running = false;
        in_thread.is_detached = true;
        return *this;
      }

      enum class priority : unsigned {
        very_low = 0,
        low,
        normal,
        high,
        very_high,
        time_critical
      };

      void set_priority(priority prio_) {
        assert(!is_detached && is_running);

#ifdef _WIN32
        static const int prio_list[] = {
          THREAD_PRIORITY_LOWEST,
          THREAD_PRIORITY_BELOW_NORMAL,
          THREAD_PRIORITY_NORMAL,
          THREAD_PRIORITY_ABOVE_NORMAL,
          THREAD_PRIORITY_HIGHEST,
          THREAD_PRIORITY_TIME_CRITICAL
        };

        SetThreadPriority(handle, prio_list[static_cast<unsigned>(prio_)]);
#else
        // Modern Unix kernels will most likely not honor this.
        struct sched_param param = {0};
        param.sched_priority = 2 - static_cast<int>(prio_);

        if(pthread_setschedparam(handle, SCHED_OTHER, &param) < 0)
          std::cerr << "Couldn't set priority ..." << std::endl;
#endif
      }

      // Detach the thread. We will never join with it,
      // and it will automatically be free'd when it exits.
      // Cannot join after this.
      void detach() {
        if(!is_detached && is_running)
        {
#ifdef _WIN32
          // Not 100 % sure if this is how it is supposed to be done.
          CloseHandle(handle);
#else
          pthread_detach(handle);
#endif
          is_detached = true;
        }
      }

      void join() {
        assert(!is_detached && is_running);
#ifdef _WIN32
        WaitForSingleObject(handle, INFINITE);
        CloseHandle(handle);
#else
        pthread_join(handle, 0);
#endif
        is_running = false;
      }

      ~thread() {
        if(!is_detached && is_running)
          detach();
      }

      void operator=(const thread&) = delete;
      thread(const thread&) = delete;

    private:
      template <typename T>
      struct fn_callable : internal::callable {
        fn_callable(const T& obj) : func(obj) {}
        T func;

        void run() { func(); }
      };

      bool is_running;
      bool is_detached;

      // TODO: Error handling (exceptions?).
      void start_thread(internal::callable *callobj) {
#ifdef _WIN32
        handle = CreateThread(0, 0, internal::thread_entry, callobj, 0, 0);
#else
        pthread_create(&handle, 0, internal::thread_entry, callobj);
#endif
        is_running = true;
      }

#ifdef _WIN32
      HANDLE handle;
#else
      pthread_t handle;
#endif
  };

  // Spawns a thread, and detaches it.
  template<typename... T>
  inline void spawn_thread(T&&... args) {
    thread thr = thread(std::forward<T>(args)...);
    thr.detach();
  }

  template<typename... T>
  inline void spawn_thread(thread::priority prio, T&&... args) {
    thread thr = thread(std::forward<T>(args)...);
    thr.set_priority(prio);
    thr.detach();
  }
}

#endif

