#pragma once
#include "PKAssetWriter.h"
#include <stdio.h>
#include <stdlib.h>
#include <filesystem>
#include <queue>
#include <map>
#if PK_DEBUG
#include "PKAssets/PKAssetLoader.h"
#endif

namespace PK::Assets
{
    constexpr static const size_t MIN_COMPRESSABLE_SIZE = 5000;

    struct PKTempNode
    {
        std::shared_ptr<PKTempNode> left = nullptr;
        std::shared_ptr<PKTempNode> right = nullptr;
        char value = 0;
        size_t freq = 0ull;

        constexpr bool operator < (PKTempNode const& other) const { return other.freq < freq; }

        PKTempNode(char v, size_t w)
        {
            value = v;
            left = nullptr;
            right = nullptr;
            freq = w;
        }

        PKTempNode(const PKTempNode& tree)
        {
            value = tree.value;
            left = tree.left;
            right = tree.right;
            freq = tree.freq;
        }

        PKTempNode(const PKTempNode& h1, const PKTempNode& h2)
        {
            right = std::make_shared<PKTempNode>(h1);
            left = std::make_shared<PKTempNode>(h2);
            freq = left->freq + right->freq;
            value = 0;
        }
    };

    struct PKBinaryKey
    {
        size_t count;
        size_t sequence;
        size_t length;
    };

    WritePtr<PKEncNode> WriteNodeTree(PKAssetBuffer& buffer, std::map<char, PKBinaryKey>& vtable, size_t* sequence, size_t* depth, const PKTempNode* node)
    {
        auto pNode = buffer.Allocate<PKEncNode>();
        pNode->isLeaf = node->left == nullptr && node->right == nullptr;
        pNode->value = node->value;

        if (node->left == nullptr && node->right == nullptr)
        {
            vtable[node->value].sequence = *sequence;
            vtable[node->value].length = *depth;
            return pNode;
        }

        if (node->left != nullptr)
        {
            (*sequence) &= ~(1ull << (*depth));
            (*depth)++;
            auto newNode = WriteNodeTree(buffer, vtable, sequence, depth, node->left.get());
            pNode->left.Set(buffer.data(), newNode.get());
            (*depth)--;
        }

        if (node->right != nullptr)
        {
            (*sequence) |= 1ull << (*depth);
            (*depth)++;
            auto newNode = WriteNodeTree(buffer, vtable, sequence, depth, node->right.get());
            pNode->right.Set(buffer.data(), newNode.get());
            (*depth)--;
            (*sequence) &= ~(1ull << (*depth));
        }

        return pNode;
    }

    PKAssetBuffer CompressBuffer(PKAssetBuffer src)
    {
        // Write some padding to the end so that decompression wont reinterpret wrong bytes
        // @TODO there is something wrong going on here as this should be unecessary. Investigate later.
        uint32_t padding = 0u;
        src.Write(&padding, 1);

        auto* charData = src.data() + sizeof(PKAssetHeader);
        auto srcSize = src.size() - sizeof(PKAssetHeader);

        std::map<char, PKBinaryKey> vtable;
        std::priority_queue<PKTempNode> minHeap;

        for (auto i = 0u; i < srcSize; ++i)
        {
            vtable[charData[i]].count++;
        }

        for (auto& kv : vtable)
        {
            minHeap.emplace(kv.first, kv.second.count);
        }

        while (minHeap.size() > 1)
        {
            auto n0 = minHeap.top();
            minHeap.pop();
            auto n1 = minHeap.top();
            minHeap.pop();
            minHeap.emplace(n0, n1);
        }

        PKAssetBuffer buffer;
        buffer.header->type = src.header->type;
        buffer.header->isCompressed = true;
        WriteName(buffer.header->name, src.header->name);

        *buffer.Allocate<uint32_t>() = (uint32_t)src.size();
        auto binOffset = buffer.Allocate<uint32_t>();
        auto binSize = buffer.Allocate<size_t>();
        auto sequence = 0ull;
        auto depth = 0ull;
        auto binLength = 0ull;

        auto rootNode = minHeap.top();
        auto pRootNode = WriteNodeTree(buffer, vtable, &sequence, &depth, &rootNode);
        *binOffset = (uint32_t)buffer.size();

        for (auto& kv : vtable)
        {
            binLength += kv.second.count * kv.second.length;
        }

        *binSize = binLength;
        auto pData = buffer.Allocate<char>((binLength + 7) / 8);
        auto pHead = reinterpret_cast<char*>(pData.get());

        for (auto i = 0ull, b = 0ull; i < srcSize; ++i)
        {
            auto& k = vtable[charData[i]];

            for (auto j = 0; j < k.length; ++j, ++b)
            {
                if (k.sequence & (1ull << j))
                {
                    pHead[b / 8] |= 1 << (b % 8);
                }
            }
        }

        return buffer;
    }

    void WriteName(char* dst, const char* src)
    {
        auto c = strlen(src);
        
        if (c >= PK_ASSET_NAME_MAX_LENGTH)
        {
            c = PK_ASSET_NAME_MAX_LENGTH - 1;
        }

        memcpy(dst, src, c);
        memset(dst + c, '\0', PK_ASSET_NAME_MAX_LENGTH - c);
    }

    int WriteAsset(const char* filepath, const PKAssetBuffer& buffer)
    {
        printf("writing file: %s \n", filepath);

        FILE* file = nullptr;

        auto path = std::filesystem::path(filepath).remove_filename().string();
        path = path.substr(0, path.length() - 1);

        if (!std::filesystem::exists(path))
        {
            try
            {
                std::filesystem::create_directories(path);
            }
            catch (std::exception& e)
            {
                printf(e.what());
            }
        }

#if _WIN32
        auto error = fopen_s(&file, filepath, "wb");

        if (error != 0)
        {
            printf("Failed to open/create file! \n");
            return -1;
        }
#else
        file = fopen(filepath, "wb");
#endif

        if (file == nullptr)
        {
            printf("Failed to open/create file! \n");
            return -1;
        }

        if (buffer.size() > MIN_COMPRESSABLE_SIZE)
        {
            auto compact = CompressBuffer(buffer);
            fwrite(compact.data(), sizeof(char), compact.size(), file);
        }
        else
        {
            fwrite(buffer.data(), sizeof(char), buffer.size(), file);
        }

        if (fclose(file) != 0)
        {
            printf("Failed to close file! \n");
            return -1;
        }

        #if PK_DEBUG
        PKAsset asset;
        Assets::OpenAsset(filepath, &asset);

        auto charData = reinterpret_cast<char*>(asset.rawData);

        for (auto i = 0ull; i < buffer.size(); ++i)
        {
            if (charData[i] != buffer.data()[i])
            {
                printf("Compression missmatch at byte index: %lli", i);
            }
        }

        auto compare = memcmp(asset.rawData, buffer.data(), buffer.size());
        Assets::CloseAsset(&asset);
        #endif

        return 0;
    }
}