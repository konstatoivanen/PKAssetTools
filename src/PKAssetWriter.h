#pragma once
#include "PKAssets/PKAsset.h"
#include <vector>

namespace PK::Assets
{
    template<typename T>
    struct WritePtr
    {
        std::vector<char>* buffer;
        uint_t offset;

        WritePtr(std::vector<char>* _buffer, uint_t _offset)
        {
            buffer = _buffer;
            offset = _offset;
        }

        T* get()
        {
            return reinterpret_cast<T*>(buffer->data() + offset);
        }

        operator T* () { return get(); }
    };

    struct PKAssetBuffer : std::vector<char>
    {
        PKAssetBuffer() : header(Allocate<PKAssetHeader>())
        {    
            header.get()->magicNumber = PK_ASSET_MAGIC_NUMBER;
        }

        template<typename T>
        WritePtr<T> Allocate(size_t count = 1)
        {
            auto offs = size();
            resize(offs + sizeof(T) * count);
            return WritePtr<T>(this, (uint_t)offs);
        }

        template<typename T>
        WritePtr<T> Write(const T* src, size_t count)
        {
            auto ptr = Allocate<T>(count);
            memcpy(ptr, src, sizeof(T) * count);
            return ptr;
        }

        inline void* GeatHead()
        {
            return data() + size();
        }

        WritePtr<PKAssetHeader> header;
    };

    void WriteName(char* dst, const char* src);
    
    int WriteAsset(const char* filepath, const PKAssetBuffer& buffer);

    PKAssetBuffer CompressBuffer(const void* src, size_t srcSize, size_t* outSize);
}