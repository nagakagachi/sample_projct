#pragma once

#include <memory>

#include "ngl/util/noncopyable.h"

#include "ngl/text/hash_text.h"

namespace ngl
{
namespace res
{
	class Resource;

	namespace detail
	{
		using RawResourceHandle = std::shared_ptr<const Resource>;
	}

	// Appがリソースを保持する際は必ずこのハンドルで取り扱う. 
	//	参照カウント管理でハンドル参照が無くなった場合にリソースが破棄される.
	template<typename ResType>
	class ResourceHandle
	{
	public:
		ResourceHandle()
		{}
		ResourceHandle(const ResType* p)
		{
			raw_handle_.reset(p);
		}
		ResourceHandle(ResourceHandle& p)
		{
			raw_handle_ = p.raw_handle_;
		}
		ResourceHandle(const ResourceHandle& p)
		{
			raw_handle_ = p.raw_handle_;
		}
		~ResourceHandle() {}


		bool IsValid() const
		{
			return nullptr != raw_handle_.get();
		}
		const ResType* Get() const
		{
			return static_cast<const ResType*>(raw_handle_.get());
		}
		const ResType* operator->() const
		{
			return static_cast<const ResType*>(raw_handle_.operator->());
		}
		const ResType& operator*() const
		{
			return *static_cast<const ResType*>(&raw_handle_.operator*());
		}
		

	private:
		// Resouce基底での参照カウント.
		//	こうすることでアプリが派生Resourceハンドルを持ちつつマネージャが基底ハンドルで統括管理する場合も共通の正しい参照カウント管理ができる.
		detail::RawResourceHandle raw_handle_;
	};


	// Resource派生クラス用のクラススコープ内宣言用マクロ.
#define NGL_RES_MEMBER_DECLARE(CLASS_NAME) \
	public:\
		static constexpr char k_resource_type_name[64] = #CLASS_NAME;\
		const char* GetResourceTypeName() const override final { return k_resource_type_name;}
	
	// Resouce基底.
	//	派生クラスはクラススコープで NGL_RES_MEMBER_DECLARE(クラス名) を記述すること.
	class Resource : public NonCopyableTp<Resource>
	{
	public:
		Resource()
		{
		}
		virtual ~Resource()
		{
		}


		virtual const char* GetResourceTypeName() const = 0;
	};

}
}
