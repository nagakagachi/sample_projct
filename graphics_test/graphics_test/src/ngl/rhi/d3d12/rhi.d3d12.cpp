
#include "rhi.d3d12.h"

namespace ngl
{
	namespace rhi
	{

		GraphicsCommandListDep::GraphicsCommandListDep()
		{
		}
		GraphicsCommandListDep::~GraphicsCommandListDep()
		{
			Finalize();
		}
		bool GraphicsCommandListDep::Initialize(DeviceDep* p_device)
		{
			if (!p_device)
				return false;

			// Command Allocator
			if (FAILED(p_device->GetD3D12Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&p_command_allocator_))))
			{
				std::cout << "ERROR: Create Command Allocator" << std::endl;
				return false;
			}

			// Command List
			if (FAILED(p_device->GetD3D12Device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, p_command_allocator_, nullptr, IID_PPV_ARGS(&p_command_list_))))
			{
				std::cout << "ERROR: Create Command List" << std::endl;
				return false;
			}

			// 初回クローズ. これがないと初回フレームの開始時ResetでComError発生.
			p_command_list_->Close();

			return true;
		}
		void GraphicsCommandListDep::Finalize()
		{
			if (p_command_list_)
			{
				p_command_list_->Release();
				p_command_list_ = nullptr;
			}
			if (p_command_allocator_)
			{
				p_command_allocator_->Release();
				p_command_allocator_ = nullptr;
			}
		}

		void GraphicsCommandListDep::Begin()
		{
			p_command_allocator_->Reset();

			// 初回フレームのここでComError
			p_command_list_->Reset(p_command_allocator_, nullptr);
		}
		void GraphicsCommandListDep::End()
		{
			p_command_list_->Close();
		}

		void GraphicsCommandListDep::SetRenderTargetSingle(RenderTargetView* p_rtv)
		{
			auto rtv = p_rtv->GetD3D12DescriptorHandle();
			p_command_list_->OMSetRenderTargets(1, &rtv, true, nullptr);
		}
		void GraphicsCommandListDep::ClearRenderTarget(RenderTargetView* p_rtv, float(color)[4])
		{
			auto rtv = p_rtv->GetD3D12DescriptorHandle();
			p_command_list_->ClearRenderTargetView(rtv, color, 0u, nullptr);
		}
	}
}