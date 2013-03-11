#include <util/singleton.h>
#include <util/scheduler.h>
#include <util/timer.h>
#include <iostream>

#include <map>
#include <boost/thread.hpp>

using namespace std;

namespace izenelib{
namespace util{

class QueueTimer : public Timer
{
public:
    QueueTimer(void (*callback)(void *),
               void *arg,
               uint32_t due_time,
               uint32_t period)
            :callback_(callback),
             arg_(arg),
             due_time_(due_time),
             period_(period)
    {}

    bool start()
    {
        return Timer::start(due_time_, period_);
    }

    bool startNow()
    {
        return Timer::start(0, period_);
    }

    virtual void signaled()
    {
        callback_(arg_);
    }

private:
    void (*callback_)(void *);
    void *arg_;
    uint32_t due_time_;
    uint32_t period_;
};

struct ScheduleOP
{
    std::string name;
    uint32_t default_interval;
    uint32_t delay_start;
    boost::function<void (int)> callback;
    QueueTimer* timer;
    volatile bool running;
    volatile bool immediatly_running;
    boost::mutex jobmutex;
};

class SchedulerImpl
{
public:
    SchedulerImpl() {}

    virtual ~SchedulerImpl()
    {
        removeAllJobs();
    }

    void removeAllJobs()
    {
        boost::mutex::scoped_lock l(mutex_);
        for (std::map<std::string, boost::shared_ptr<ScheduleOP> >::iterator itr = jobs_.begin();
                itr != jobs_.end(); ++itr)
        {
            boost::shared_ptr<ScheduleOP> job = itr->second;
            if (job->timer)
            {
                job->timer->stop();
                delete job->timer;
                job->timer = NULL;
            }
        }
        jobs_.clear();
    }

    bool addJob(const string &name, uint32_t default_interval,
                uint32_t delay_start, const boost::function<void (int)>& func)
    {
        boost::mutex::scoped_lock l(mutex_);
        std::map<string, boost::shared_ptr<ScheduleOP> >::iterator find_itr = jobs_.find(name);
        if (find_itr != jobs_.end())
            return false;

        boost::shared_ptr<ScheduleOP> newjob(new ScheduleOP());
        newjob->name = name;
        newjob->default_interval = default_interval;
        newjob->delay_start = delay_start;
        newjob->callback = func;
        newjob->timer = NULL;
        newjob->running = false;
        newjob->immediatly_running = false;

        std::pair<std::map<std::string, boost::shared_ptr<ScheduleOP> >::iterator, bool> result
        = jobs_.insert(make_pair(name, newjob));
        if (!result.second)
        {
            return false;
        }
        boost::shared_ptr<ScheduleOP> job = result.first->second;

        uint32_t delay = job->delay_start;
        job->timer = new QueueTimer(&timerCallback, job.get(), delay,
                                    job->default_interval);
        if (job->timer == NULL)
        {
            return false;
        }
        const bool started =  job->timer->start();
        if (started)
        {
            return true;
        }
        else
        {
            delete job->timer;
            job->timer = NULL;
            return false;
        }
    }

    bool runJobImmediatly(const std::string& name, int calltype, bool sync)
    {
        boost::shared_ptr<ScheduleOP> job;
        {
            boost::mutex::scoped_lock l(mutex_);
            std::map<std::string, boost::shared_ptr<ScheduleOP> >::iterator itr = jobs_.find(name);
            if (itr == jobs_.end())
            {
                std::cout << "schedule job not found:" << name << std::endl;
                return false;
            }
            job = itr->second;
        }
        if (job)
        {
            int retry = 5;
            while (job->running || job->immediatly_running)
            {
                if (retry-- < 0)
                {
                    std::cout << "schedule job already running:" << name << std::endl;
                    return false;
                }
                sleep(1);
            }
            {
                boost::mutex::scoped_lock job_guard(job->jobmutex);
                if (job->running || job->immediatly_running)
                    return false;
                job->immediatly_running = true;
            }
            try{
                boost::scoped_ptr<boost::thread> run_thread;
                run_thread.reset(new boost::thread(boost::bind(job->callback, calltype)));
                run_thread->join();
            } catch(const std::exception& e) {
                std::cout << "run job exception: " << e.what() << std::endl;
                {
                    boost::mutex::scoped_lock job_guard(job->jobmutex);
                    job->immediatly_running = false;
                }
                throw e;
            }
            {
                boost::mutex::scoped_lock job_guard(job->jobmutex);
                job->immediatly_running = false;
            }
            return true;
        }
        std::cout << "schedule job null:" << name << std::endl;
        return false;
    }

    bool removeJob(const std::string &name)
    {
        boost::mutex::scoped_lock l(mutex_);
        std::map<std::string, boost::shared_ptr<ScheduleOP> >::iterator itr = jobs_.find(name);
        if (itr == jobs_.end())
        {
            return false;
        }
        else
        {
            boost::shared_ptr<ScheduleOP> job = itr->second;
            if (job->timer != NULL)
            {
                job->timer->stop();
                delete job->timer;
                job->timer = NULL;
            }
            jobs_.erase(itr);
            return true;
        }
    }

private:
    static void timerCallback(void *param)
    {
        ScheduleOP *job = reinterpret_cast<ScheduleOP *>(param);

        {
            boost::mutex::scoped_lock job_guard(job->jobmutex);
            if (job->running || job->immediatly_running)
                return;
            job->running = true;
        }

        job->callback(0);

        {
            boost::mutex::scoped_lock job_guard(job->jobmutex);
            job->running = false;
        }
    }

    std::map<std::string, boost::shared_ptr<ScheduleOP> > jobs_;
    boost::condition_variable cond_;
    boost::mutex mutex_;
};

bool Scheduler::addJob(const string &name, uint32_t default_interval,
                       uint32_t delay_start, const boost::function<void (int)>& func)
{
    return Singleton<SchedulerImpl>::get()->addJob(name, default_interval, delay_start, func);
}

bool Scheduler::removeJob(const string &name)
{
    return Singleton<SchedulerImpl>::get()->removeJob(name);
}

void Scheduler::removeAllJobs()
{
    Singleton<SchedulerImpl>::get()->removeAllJobs();
}

bool Scheduler::runJobImmediatly(const std::string& name, int calltype, bool sync)
{
    return Singleton<SchedulerImpl>::get()->runJobImmediatly(name, calltype, sync);
}

}
}
