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

	// App�����\�[�X��ێ�����ۂ͕K�����̃n���h���Ŏ�舵��. 
	//	�Q�ƃJ�E���g�Ǘ��Ńn���h���Q�Ƃ������Ȃ����ꍇ�Ƀ��\�[�X���j�������.
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
		// Resouce���ł̎Q�ƃJ�E���g.
		//	�������邱�ƂŃA�v�����h��Resource�n���h���������}�l�[�W�������n���h���œ����Ǘ�����ꍇ�����ʂ̐������Q�ƃJ�E���g�Ǘ����ł���.
		detail::RawResourceHandle raw_handle_;
	};


	// Resource�h���N���X�p�̃N���X�X�R�[�v���錾�p�}�N��.
#define NGL_RES_MEMBER_DECLARE(CLASS_NAME) \
	public:\
		static constexpr char k_resource_type_name[64] = #CLASS_NAME;\
		const char* GetResourceTypeName() const override final { return k_resource_type_name;}
	
	// Resouce���.
	//	�h���N���X�̓N���X�X�R�[�v�� NGL_RES_MEMBER_DECLARE(�N���X��) ���L�q���邱��.
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
