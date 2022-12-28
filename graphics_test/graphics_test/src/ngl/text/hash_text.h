#pragma once

#ifndef _NGL_UTIL_HASH_TEXT_
#define _NGL_UTIL_HASH_TEXT_

#include <algorithm>
#include <cassert>

/*
	文字列とそのhash値を保持し、等価演算を高速化する
	templateによる固定長

	サイズ違いのインスタンス同士の代入などは今後追加予定
*/


namespace ngl
{
	namespace text
	{
		// FNV-1a UTILITIES
		// These functions are extremely performance sensitive, check examples like
		// that in VSO-653642 before making changes.
#if defined(_WIN64)
		constexpr size_t NGL_FNV_offset_basis = 14695981039346656037ULL;
		constexpr size_t NGL_FNV_prime = 1099511628211ULL;
#else // defined(_WIN64)
		constexpr size_t NGL_FNV_offset_basis = 2166136261U;
		constexpr size_t NGL_FNV_prime = 16777619U;
#endif // defined(_WIN64)

		// 配列から Fnv1aハッシュを計算
		// accumulate range [_First, _First + _Count) into partial FNV-1a hash _Val
		inline size_t Fnv1a_append_bytes(size_t _Val, const unsigned char* const _First, const size_t _Count) noexcept
		{
			for (size_t _Idx = 0; _Idx < _Count; ++_Idx) {
				_Val ^= static_cast<size_t>(_First[_Idx]);
				_Val *= NGL_FNV_prime;
			}

			return _Val;
		}
		// 配列から Fnv1aハッシュを計算
		// FUNCTION TEMPLATE _Hash_array_representation
		// bitwise hashes the representation of an array
		template <class _Kty>
		size_t Fnv1a_Hash_array_representation(
			const _Kty* const _First, const size_t _Count) noexcept 
		{
			return Fnv1a_append_bytes(NGL_FNV_offset_basis, reinterpret_cast<const unsigned char*>(_First), _Count * sizeof(_Kty));
		}

		/*
			バッファの末尾が常に0で埋められていることを保証する固定長文字列.
		*/
		template<unsigned int SIZE>
		class FixedString
		{
		public:
			FixedString()
			{
#ifdef _DEBUG
				// デバッグ用に不正な初期値書き込み
				memset(text_, 0xcd, std::size(text_));
#endif

				valid_len_ = 0;
				std::memset(text_, 0, std::size(text_));
			}
			~FixedString()
			{
			}

			FixedString(const char* str)
			{
				Set(str, std::min(static_cast<unsigned int>(std::strlen(str)), SIZE));
			}
			FixedString(const FixedString& obj)
			{
				Set(obj.text_, obj.valid_len_);
			}

			FixedString& operator = (const char* str)
			{
				Set(str, std::min(static_cast<unsigned int>(std::strlen(str)), SIZE));
				return *this;
			}
			FixedString& operator = (const FixedString& obj)
			{
				Set(obj.text_, obj.valid_len_);
				return *this;
			}

			bool operator == (const FixedString& obj) const
			{
				if (obj.valid_len_ != valid_len_)
					return false;
				return 0 == std::memcmp(text_, obj.text_, valid_len_);
			}

			operator const char*() const
			{
				return Get();
			}

			void Set(const char* str, unsigned int size)
			{
				// このメソッドの呼び出しは必ず適正なsizeが渡される
				assert(SIZE >= size);

				std::copy_n(str, size, text_);
				// 有効文字列長より後ろのバッファは必ずゼロで埋まっていることを保証.
				if (valid_len_ > size)
				{
					// 既存の有効文字列長より短い場合は差分をゼロ埋め
					std::memset(text_ + size, 0, valid_len_ - size);
				}
				// 有効文字列長更新
				valid_len_ = size;
			}

			const char* Get() const
			{
				return text_;
			}
			unsigned int GetValidLen() const
			{
				return valid_len_;
			}


			constexpr unsigned int GetMaxLen() const { return SIZE; }
			static constexpr unsigned int MaxLength() { return SIZE; }
		private:

		private:
			unsigned int		valid_len_ = 0;
			char				text_[SIZE + 1] = {};
		};


		/*
			旧ハッシュテキスト
			TODO. FixedStringを利用した実装に置き換えたい.
		*/
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

			void Set(const char* str, unsigned int size);
			const char* Get() const;
			unsigned int GetHash() const;

			unsigned int GetMaxLen() const;
			static unsigned int MaxLength() { return SIZE; }

		private:
			void Reset();

		private:
			unsigned int hash_ = 0;
			char text_[SIZE + 1] = {};
		};
	}
}

/*
	std::hash 向けの特殊化
*/
template<unsigned int SIZE>
bool operator==(const ngl::text::FixedString<SIZE>& lhs, const ngl::text::FixedString<SIZE>& rhs) 
{
	return lhs == rhs.d;
}
namespace std
{
	template <unsigned int SIZE>
	struct hash<ngl::text::FixedString<SIZE>>
	{
		size_t operator()(const ngl::text::FixedString<SIZE>& v) const
		{
			return ngl::text::Fnv1a_Hash_array_representation(v.Get(), v.GetValidLen());
		}
	};
}

#include "hash_text.inl"

#endif // _NGL_UTIL_HASH_TEXT_