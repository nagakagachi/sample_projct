#pragma once
#ifndef _NGL_FILE_H_
#define _NGL_FILE_H_

#include "ngl/util/types.h"
#include "ngl/util/unique_ptr.h"

namespace ngl
{
	namespace file
	{
		u32 calcFileSize(const char* filePath);

		class FileObject
		{
		public:
			FileObject();
			FileObject( const char* filePath );
			~FileObject();
			// ファイルリード
			bool ReadFile(const char* filePath);
			// 解放
			void Release();

			u32 GetFileSize() const
			{
				return fileSize_;
			}
			const u8* GetFileData() const
			{
				return fileData_.Get();
			}

		private:
			u32 fileSize_				= 0;
			UniquePtr< u8[] > fileData_ = {};
		};
	}
}

#endif //_NGL_FILE_H_