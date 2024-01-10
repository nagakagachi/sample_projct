//  rtg_command_list_pool.h

#pragma once

#include "ngl/math/math.h"

#include "ngl/rhi/d3d12/device.d3d12.h"
#include "ngl/rhi/d3d12/command_list.d3d12.h"

namespace ngl::rtg
{
    namespace pool
    {
        using GraphicsCommandListType = rhi::GraphicsCommandListDep;
        using ComputeCommandListType = rhi::ComputeCommandListDep;
        
        template<typename T>
        struct CommandListTypeTraits;
        template<> struct CommandListTypeTraits<GraphicsCommandListType>
        {
            static constexpr int TypeIndex = 0; // GraphicsのIndex定義.
        };
        template<> struct CommandListTypeTraits<ComputeCommandListType>
        {
            static constexpr int TypeIndex = 1; // ComputeのIndex定義.
        };

        template<typename T>
        struct PooledCommandListElem
        {
            static_assert(0 <= CommandListTypeTraits<T>::TypeIndex);// Indexが定義されているか一応チェック.
            
            rhi::RhiRef<T>  ref_commandlist  = {};      // rhi::RhiRef<CommandListType>.
            int             reusable_frame_count = 0;   // Poolから貸出可能になるまでのフレーム数. 0>=x で貸出可能.
        };

        // CommandListPoolの管理.
        //  Graphics, Compute等のタイプ別に内部にPoolしたCommandListを貸し出す, フレームカウントで再利用管理する.
        class CommandListPool
        {
        public:
            CommandListPool() = default;
            ~CommandListPool() = default;

            template<typename TCommandList>
            void GetCommandList(rhi::RhiRef<TCommandList>& ref_out);
            
        private:
            using GraphicsCommandListPoolBuffer = std::vector<PooledCommandListElem<GraphicsCommandListType>>;
            using ComputeCommandListPoolBuffer = std::vector<PooledCommandListElem<ComputeCommandListType>>;

            // Tupleでタイプ毎のBufferまとめて管理.
            std::tuple<GraphicsCommandListPoolBuffer, ComputeCommandListPoolBuffer> typed_pool_;
        };

        // Type毎にTraitsで定義したIndexでPoolの適切な要素から空きを探索.
        template<typename TCommandList>
        inline void CommandListPool::GetCommandList(rhi::RhiRef<TCommandList>& ref_out)
        {
            constexpr  int type_index = CommandListTypeTraits<TCommandList>::TypeIndex;

            auto cur_size = std::get<type_index>(typed_pool_).size();

            // TODO.
        }
    
    }
}