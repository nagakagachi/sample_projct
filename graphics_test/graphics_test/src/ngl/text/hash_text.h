﻿#pragma once

#ifndef _NGL_UTIL_HASH_TEXT_
#define _NGL_UTIL_HASH_TEXT_

/*
	文字列とそのhash値を保持し、等価演算を高速化する
	templateによる固定長

	サイズ違いのインスタンス同士の代入などは今後追加予定
*/


namespace ngl
{
	namespace text
	{

		template<unsigned int SIZE>
		class HashText
		{
		public:
			HashText();
			~HashText();

			HashText(const char* str);
			HashText(const HashText& obj);

			HashText& operator = (const char* str);
			HashText& operator = (const HashText& obj);

			bool operator == (const HashText& obj) const;
			bool operator == (const char* str) const;

			bool operator < (const HashText& rhs) const;
			bool operator > (const HashText& rhs) const;

			void Set( const char* str, unsigned int size );
			const char* Get() const;
			unsigned int GetHash() const;

			unsigned int GetMaxLen() const;
			static unsigned int MaxLength() { return SIZE; }

		private:
			void Reset();

		private:
			unsigned int hash_		= 0;
			char text_[SIZE + 1]	= {};
		};


	}
}

#include "hash_text.inl"

#endif // _NGL_UTIL_HASH_TEXT_