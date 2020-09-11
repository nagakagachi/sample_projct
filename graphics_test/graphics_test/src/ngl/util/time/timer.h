#pragma once

#ifndef _NGL_UTIL_TIMER_
#define _NGL_UTIL_TIMER_

/*
	Windowsで動作するタイマー管理です

	名前をつけてタイマーをスタート（スタートメソッド実行毎に経過時間をリセットします）
	名前をつけたタイマーの経過時間取得
	名前をつけたタイマーを削除

	名前を指定しないとデフォルトタイマーを使います

	map をstd::stringからhashTextに変更したいかも

*/


#include <windows.h>

#include <string>
#include <map>

#include "ngl/util/types.h"
#include "ngl/util/singleton.h"
#include "ngl/text/hashText.h"

namespace ngl
{
	namespace time
	{
		typedef ngl::text::HashText<64>	TimerName;

		class Timer : public Singleton< Timer >
		{
		public:
			Timer();
			~Timer();

			// タイマー開始
			void	startTimer(const char* str = "default");
			// 経過時間[秒]取得
			double	getElapsedSec(const char* str = "default");
			// タイマー削除
			void	removeTimer(const char* str);
			// 経過時間をパフォーマンスカウンタのまま取得
			ngl::s64 getPerformanceCounter(const char* str = "default");

			// タイマーを一時停止
			void	suspendTimer(const char* str = "default");
			// タイマーを再開
			void	resumeTimer(const char* str = "default");


			// 経過時間をパフォーマンスカウンタのまま取得
			ngl::s64 getSuspendTotalPerformanceCounter(const char* str = "default");

		private:
			struct TimerEntity
			{
				struct TimerConstruct
				{
					TimerConstruct() {
						static LARGE_INTEGER freq;
						QueryPerformanceFrequency(&freq);
						friqInv_ = 1.0 / (double)(freq.QuadPart);
					}
					double			friqInv_;
				};
				static double getFriqInv() {
					static TimerConstruct c;
					return c.friqInv_;
				}
				TimerEntity() : time_begin_(), time_end_()
					//, time_stop_start_()
				{
				}

				LARGE_INTEGER time_begin_;
				LARGE_INTEGER time_end_;

				LARGE_INTEGER time_stop_start_;// 一時停止開始の時間
				LARGE_INTEGER time_suspemd_total;// 一時停止の総時間 時間の取得時にこの時間を引いたものを返すように
			};

			//std::map<std::string, TimerEntity> timers_;
			std::map<TimerName, TimerEntity> timers_;
		};
	}
}

#endif // _NGL_UTIL_TIMER_