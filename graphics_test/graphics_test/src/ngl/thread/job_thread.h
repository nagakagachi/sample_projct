#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace ngl
{
namespace thread
{   
    class SingleJobThread
    {
    public:
        SingleJobThread()
        {
            thread_instance_ = std::thread([&](){Execute();});
        }
        ~SingleJobThread()
        {
            // 実行部停止通知.
            {
                std::unique_lock<std::mutex> lock(condition_mutex_);
                terminate_signal_ = true;
			
                condition_var_.notify_all();
            }
		
            // thread完了待ち.
            thread_instance_.join();
        }

        // Job実行をリクエスト.
        //	以前のJobが実行中の場合は失敗して falseを返す.
        bool Begin(std::function< void(void) > func)
        {
            std::unique_lock<std::mutex> lock(condition_mutex_);
		
            if(job_signal_)
                return false;
		
            func_ = func;
            job_signal_ = true;
		
            condition_var_.notify_all();
            return true;
        }

        // Job実行完了を待機する.
        void Wait()
        {
            std::unique_lock<std::mutex> lock(condition_mutex_);
            // すでに終了していれば即リターン.
            if(!job_signal_)
                return;
		
            // Job完了までConditionで待機.
            condition_var_.wait(lock, [&]{return (!job_signal_ || terminate_signal_);});
        }
	
        // Job実行が可能な状態か.
        bool IsReady()
        {
            std::unique_lock<std::mutex> lock(condition_mutex_);
            return !job_signal_;
        }
	
    private:
        // Job Thread 実行部.
        void Execute()
        {
            while (true)
            {
                // Job実行リクエストか終了通知が来るまで待機.
                std::unique_lock<std::mutex> lock(condition_mutex_);
                condition_var_.wait(lock, [&]{return (job_signal_ || terminate_signal_);});

                if(job_signal_)
                {
                    // Job実行.
                    func_();
                    job_signal_ = false;
                }

                // 完了待ち側への通知.
                condition_var_.notify_all();
			
                // 終了.
                if(terminate_signal_)
                    break;
            }
        }
	
        std::thread	thread_instance_;

        std::mutex				condition_mutex_;
        std::condition_variable	condition_var_;

        bool			terminate_signal_ = false;
        bool			job_signal_ = false;	
        std::function< void(void) > func_;
    };

}
}

