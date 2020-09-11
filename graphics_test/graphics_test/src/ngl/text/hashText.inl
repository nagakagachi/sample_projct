#include <string>
#include <algorithm>
//#include <math.h>

namespace ngl
{
	namespace text
	{
		template< unsigned int SIZE >
		inline void HashText<SIZE>::reset()
		{
			memset(text_, 0, SIZE + 1);
		}

		template< unsigned int SIZE >
		HashText<SIZE>::HashText()
			: hash_(0)
		{
			reset();
		}

		template< unsigned int SIZE >
		HashText<SIZE>::~HashText()
		{
		}

		template< unsigned int SIZE >
		HashText<SIZE>::HashText(const char* str)
		{
			set(str, getMaxLen());
		}
		template< unsigned int SIZE >
		HashText<SIZE>::HashText(const HashText<SIZE>& obj)
		{
			set(obj.get(), getMaxLen());
		}

		template< unsigned int SIZE >
		inline HashText<SIZE>& HashText<SIZE>::operator = (const char* str)
		{
			set(str, getMaxLen());
			return *this;
		}
		template< unsigned int SIZE >
		inline HashText<SIZE>& HashText<SIZE>::operator = (const HashText<SIZE>& obj)
		{
			set(obj.get(), getMaxLen());
			return *this;
		}

		template< unsigned int SIZE >
		inline bool HashText<SIZE>::operator == (const HashText<SIZE>& obj) const
		{
			return hash_ != obj.getHash() ? false : 0==strncmp(text_, obj.get(), getMaxLen());
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
			return hash_<rhs.getHash();
		}
		template< unsigned int SIZE >
		inline bool HashText<SIZE>::operator>(const HashText& rhs) const
		{
			return !(this<rhs);
		}

		template< unsigned int SIZE >
		inline void HashText<SIZE>::set(const char* str, unsigned int size)
		{
			unsigned int minLen = size < getMaxLen() ? size : getMaxLen();
			strncpy_s(text_, getMaxLen() + 1, str, minLen);
			//strncpy(text_, str, min(size, getMaxLen()) );
			hash_ = 0;
			unsigned int L = static_cast<unsigned int>(strlen(text_));
			for (unsigned int i = 0; i < L; ++i)
			{
				//hash_ += static_cast<unsigned int>(pow(static_cast<float>(text_[i]), (1+i)%32));
				hash_ += (i+1) * 37 * text_[i];
			}
		}
		template< unsigned int SIZE >
		inline const char* HashText<SIZE>::get() const
		{
			return text_;
		}
		template< unsigned int SIZE >
		inline unsigned int HashText<SIZE>::getHash() const
		{
			return hash_;
		}

		template< unsigned int SIZE >
		unsigned int HashText<SIZE>::getMaxLen() const
		{
			return SIZE;
		}

	}
}