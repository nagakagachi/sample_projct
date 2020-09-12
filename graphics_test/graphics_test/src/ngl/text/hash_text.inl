#include <string>
#include <algorithm>
//#include <math.h>

namespace ngl
{
	namespace text
	{
		template< unsigned int SIZE >
		inline void HashText<SIZE>::Reset()
		{
			memset(text_, 0, SIZE + 1);
		}

		template< unsigned int SIZE >
		HashText<SIZE>::HashText()
			: hash_(0)
		{
			Reset();
		}

		template< unsigned int SIZE >
		HashText<SIZE>::~HashText()
		{
		}

		template< unsigned int SIZE >
		HashText<SIZE>::HashText(const char* str)
		{
			Set(str, GetMaxLen());
		}
		template< unsigned int SIZE >
		HashText<SIZE>::HashText(const HashText<SIZE>& obj)
		{
			Set(obj.Get(), GetMaxLen());
		}

		template< unsigned int SIZE >
		inline HashText<SIZE>& HashText<SIZE>::operator = (const char* str)
		{
			Set(str, GetMaxLen());
			return *this;
		}
		template< unsigned int SIZE >
		inline HashText<SIZE>& HashText<SIZE>::operator = (const HashText<SIZE>& obj)
		{
			Set(obj.Get(), GetMaxLen());
			return *this;
		}

		template< unsigned int SIZE >
		inline bool HashText<SIZE>::operator == (const HashText<SIZE>& obj) const
		{
			return hash_ != obj.GetHash() ? false : 0==strncmp(text_, obj.get(), GetMaxLen());
		}
		template< unsigned int SIZE >
		inline bool HashText<SIZE>::operator == (const char* str) const
		{
			// わざわざハッシュ化してからチェックしてるので遅い 文字列チェックだけにするかも
			return *this == HashText<SIZE>(str);
		}

		template< unsigned int SIZE >
		inline bool HashText<SIZE>::operator<(const HashText& rhs) const
		{
			return hash_<rhs.GetHash();
		}
		template< unsigned int SIZE >
		inline bool HashText<SIZE>::operator>(const HashText& rhs) const
		{
			return !(this<rhs);
		}

		template< unsigned int SIZE >
		inline void HashText<SIZE>::Set(const char* str, unsigned int size)
		{
			unsigned int minLen = size < GetMaxLen() ? size : GetMaxLen();
			strncpy_s(text_, GetMaxLen() + 1, str, minLen);
			hash_ = 0;
			unsigned int L = static_cast<unsigned int>(strlen(text_));
			for (unsigned int i = 0; i < L; ++i)
			{
				hash_ += (i+1) * 37 * text_[i];
			}
		}
		template< unsigned int SIZE >
		inline const char* HashText<SIZE>::Get() const
		{
			return text_;
		}
		template< unsigned int SIZE >
		inline unsigned int HashText<SIZE>::GetHash() const
		{
			return hash_;
		}

		template< unsigned int SIZE >
		unsigned int HashText<SIZE>::GetMaxLen() const
		{
			return SIZE;
		}

	}
}