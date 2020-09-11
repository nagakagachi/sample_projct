
#include "timer.h"


namespace ngl
{
namespace time
{
	Timer::Timer()
	{
		timers_.clear();
		timers_.insert(std::pair<TimerName, Timer::TimerEntity>("default", Timer::TimerEntity()));
	}

	Timer::~Timer()
	{
		timers_.clear();
	}

	void	Timer::startTimer(const char* name)
	{
		std::map<TimerName, Timer::TimerEntity>::iterator it = timers_.find(name);
		if (timers_.end() == it)
		{
			timers_.insert(std::pair<TimerName, Timer::TimerEntity >(name, Timer::TimerEntity()));
			it = timers_.find(name);
		}
		QueryPerformanceCounter(&it->second.time_begin_);
		
		it->second.time_stop_start_.QuadPart = -1;
		memset(&it->second.time_suspemd_total, 0x00, sizeof(LARGE_INTEGER));
	}
	double	Timer::getElapsedSec(const char* name)
	{
		return getPerformanceCounter(name) * TimerEntity::getFriqInv();
	}
	// 経過時間をパフォーマンスカウンタのまま取得
	ngl::s64 Timer::getPerformanceCounter(const char* name)
	{
		std::map<TimerName, Timer::TimerEntity>::iterator it = timers_.find(name);
		if (timers_.end() == it)
		{
			return 0;
		}
		QueryPerformanceCounter(&it->second.time_end_);
		return (it->second.time_end_.QuadPart - it->second.time_begin_.QuadPart - getSuspendTotalPerformanceCounter(name));
	}
	void	Timer::removeTimer(const char* name)
	{
		std::map<TimerName, Timer::TimerEntity>::iterator it = timers_.find(name);
		if (timers_.end() == it)
		{
			return;
		}
		timers_.erase(it);
	}

	// タイマーを一時停止
	void	Timer::suspendTimer(const char* name)
	{
		std::map<TimerName, Timer::TimerEntity>::iterator it = timers_.find(name);
		if (timers_.end() == it)
			return;
		if (0 <= it->second.time_stop_start_.QuadPart)
			return;
		QueryPerformanceCounter(&it->second.time_stop_start_);
	}
	// タイマーを再開
	void	Timer::resumeTimer(const char* name)
	{
		std::map<TimerName, Timer::TimerEntity>::iterator it = timers_.find(name);
		if (timers_.end() == it)
			return;
		if (0 > it->second.time_stop_start_.QuadPart)
			return;
		LARGE_INTEGER time;
		QueryPerformanceCounter(&time);
		it->second.time_suspemd_total.QuadPart += time.QuadPart - it->second.time_stop_start_.QuadPart;
		it->second.time_stop_start_.QuadPart = -1;
	}
	// 
	ngl::s64 Timer::getSuspendTotalPerformanceCounter(const char* name)
	{
		std::map<TimerName, Timer::TimerEntity>::iterator it = timers_.find(name);
		if (timers_.end() == it)
			return 0;
		LARGE_INTEGER time;
		QueryPerformanceCounter(&time);

		s64 pc = it->second.time_suspemd_total.QuadPart;
		pc += 0 <= it->second.time_stop_start_.QuadPart ? time.QuadPart - it->second.time_stop_start_.QuadPart : 0;
		return pc;
	}
}
}
