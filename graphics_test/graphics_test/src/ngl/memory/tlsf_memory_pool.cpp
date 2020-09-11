
#include "tlsf_memory_pool.h"

namespace ngl
{
	namespace memory
	{
		TlsfMemoryPool::TlsfMemoryPool()
			: isOuterManageMemory_(false)
			, manageMemory_(NULL)
		{
		}
		TlsfMemoryPool::~TlsfMemoryPool()
		{
		}

		bool TlsfMemoryPool::initialize(void* manageMemory, u32 size)
		{
			destroy();
			bool success = allocator_.Initialize(manageMemory, size);
			if (success)
			{
				manageMemory_ = manageMemory;
				isOuterManageMemory_ = true;
			}
			return success;
		}
		// 内部で管理用メモリ確保
		// 解放責任は内部
		bool TlsfMemoryPool::initialize(u32 size)
		{
			destroy();
			void* manageMemory = new u8[size];
			if (NULL == manageMemory)
				return false;

			bool success = allocator_.Initialize(manageMemory, size);
			if (success)
			{
				manageMemory_ = manageMemory;
				isOuterManageMemory_ = false;
			}
			else
			{
				delete manageMemory;
				manageMemory = NULL;

			}
			return success;
		}
		void TlsfMemoryPool::destroy()
		{
			if (!isOuterManageMemory_)
			{
				delete manageMemory_;
				manageMemory_ = NULL;
			}
			isOuterManageMemory_ = false;

			allocator_.destroy();
		}

		void TlsfMemoryPool::deallocate(void* ptr)
		{
			allocator_.Deallocate(ptr);
		}
	}
}
