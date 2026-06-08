#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "PKStringUtilities.h"
#include "PKMeshUtilities.h"
#include "PKTextureUtilities.h"
#include "PKAssetWriter.h"
#include "PKFileVersionUtilities.h"

namespace PKAssets::IES
{
    // https://github.com/ray-cast/ies/blob/master/ies_loader.cpp
    struct IESProfile
    {
        std::string version;
        std::vector<float> anglesH;
        std::vector<float> anglesV;
        std::vector<float> candelaValues;

        int32_t lightCount;
        float totalLumens;
        float candelaScale;
        int32_t angleCountV;
        int32_t angleCountH;
        int32_t photometricType;
        int32_t unitType;
        float width;
        float length;
        float height;
        float ballast;
        float reserved;
        float watts;
    };

    static float ComputeMaxCandela(const std::vector<float>& candelaValues)
    {
        auto maxValue = candelaValues.at(0);

        for (auto value : candelaValues)
        {
            if (value > maxValue)
            {
                maxValue = value;
            }
        }

        return maxValue;
    }

    static float ComputeFilterPos(float value, const std::vector<float>& angles)
    {
        std::size_t start = 0;
        std::size_t end = angles.size() - 1;

        if (value < angles[start]) return 0.0f;
        if (value > angles[end]) return (float)end;

        while (start < end)
        {
            std::size_t index = (start + end + 1) / 2;

            float angle = angles[index];
            if (value >= angle)
            {
                start = index;
            }
            else
            {
                end = index - 1;
            }
        }

        float leftValue = angles[start];
        float fraction = 0.0f;

        if (start + 1 < (std::uint32_t)angles.size())
        {
            float rightValue = angles[start + 1];
            float deltaValue = rightValue - leftValue;

            if (deltaValue > 0.0001f)
            {
                fraction = (value - leftValue) / deltaValue;
            }
        }

        return start + fraction;
    }

    static float InterpolatePoint(const IESProfile& info, uint32_t x, uint32_t y) 
    {
        x %= info.anglesH.size();
        y %= info.anglesV.size();
        return info.candelaValues[y + info.anglesV.size() * x];
    }

    static float InterpolateBilinear(const IESProfile& info, float x, float y)
    {
        auto ix = (int)std::floor(x);
        auto iy = (int)std::floor(y);
        auto fracX = x - ix;
        auto fracY = y - iy;
        auto p00 = InterpolatePoint(info, ix + 0, iy + 0);
        auto p10 = InterpolatePoint(info, ix + 1, iy + 0);
        auto p01 = InterpolatePoint(info, ix + 0, iy + 1);
        auto p11 = InterpolatePoint(info, ix + 1, iy + 1);
        auto p0 = p00 + (p01 - p00) * fracY;
        auto p1 = p10 + (p11 - p10) * fracY;
        return p0 + (p1 - p0) * fracX;
    }

    static float Interpolate2D(const IESProfile& info, float angleV, float angleH)
    {
        float u = ComputeFilterPos(angleH, info.anglesH);
        float v = ComputeFilterPos(angleV, info.anglesV);
        return InterpolateBilinear(info, u, v);
    }

    static float RadicalInverse_VdC(uint32_t bits)
    {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return float(bits) * 2.3283064365386963e-10;
    }

    static float HammersleyX(uint32_t i, uint32_t N)
    {
        return float(i % N) / float(N);
    }

    static float HammersleyY(uint32_t i)
    {
        return RadicalInverse_VdC(i);
    }

    static void SkipWhiteSpace(const uint8_t*& head)
    {
        while (*head)
        {
            if (*head == 13 && *(head + 1) == 10)
            {
                head += 2;
                continue;
            }
            if (*head == 10)
            {
                head++;
                continue;
            }
            if (*head <= ' ')
            {
                head++;
                continue;
            }

            break;
        }
    }

    static std::string ReadLine(const uint8_t*& head, bool splitWhiteSpace)
    {
        SkipWhiteSpace(head);

        std::string line;
        line.reserve(256);

        for (auto i = 0u; i < 255u; ++i)
        {
            if (*head == '\0')
            {
                break;
            }
            
            if (*head == '\r' || *(head) == '\n')
            {
                head++;
                break;
            }
            
            if (splitWhiteSpace && *head <= ' ')
            {
                head++;
                break;
            }

            line.append(1ull, *head++);
        }

        return line;
    }

    static float ReadFloat(const uint8_t*& head) 
    { 
        return static_cast<float>(atof(ReadLine(head, true).c_str())); 
    }

    static float ReadInt(const uint8_t*& head) 
    { 
        return atoi(ReadLine(head, true).c_str()); 
    }

    static std::string ReadFile(const char* path)
    {
        auto size = std::filesystem::file_size(path);
        std::string source(size, '\0');
        std::ifstream stream(path);
        stream.read(&source[0], size);
        return source;
    }

    int WriteIESProfile(const char* pathSrc, const char* pathDst, const size_t pathStemOffset)
    {
        if (!PKVersionUtilities::IsFileOutOfDate(pathSrc, pathDst))
        {
            return 1;
        }

        auto filename = StringUtilities::ReadFileName(pathSrc);
        printf("Preprocessing IES profile: %s \n", filename.c_str());
        
        auto source = ReadFile(pathSrc);
        auto head = reinterpret_cast<const uint8_t*>(source.data());

        IESProfile profile{};
        profile.version = ReadLine(head, true);

        if (profile.version.empty())
        {
            printf("Unknown IES profile version!\n");
            return -1;
        }

        printf("IES profile version: %s\n", profile.version.c_str());

        while (*head)
        {
            auto line = ReadLine(head, false);
            
            if (strncmp(line.c_str(), "TILT", 4) != 0)
            {
                continue;
            }

            if (strncmp(line.c_str(), "TILT=NONE", 9) != 0 &&
                strncmp(line.c_str(), "TILT =NONE", 10) != 0 &&
                strncmp(line.c_str(), "TILT= NONE", 10) != 0 &&
                strncmp(line.c_str(), "TILT = NONE", 11) != 0)
            {
                printf("Unsupported 'TILT' configuration. Only 'NONE' is supported!\n");
                return -1;
            }

            break;
        }

        profile.lightCount = ReadInt(head);
        profile.totalLumens = ReadFloat(head);
        profile.candelaScale = ReadFloat(head);
        profile.angleCountV = ReadInt(head);
        profile.angleCountH = ReadInt(head);
        profile.photometricType = ReadInt(head);
        profile.unitType = ReadInt(head);
        profile.width = ReadFloat(head);
        profile.length = ReadFloat(head);
        profile.height = ReadFloat(head);
        profile.ballast = ReadFloat(head);
        profile.reserved = ReadFloat(head);
        profile.watts = ReadFloat(head);

        if (profile.lightCount < 1 || 
            profile.candelaScale < 0.0f || 
            profile.angleCountV < 0 || 
            profile.angleCountH < 0)
        {
            printf("Unsupported IES profile: Light Count: %i\nTotal Lumens: %4.2f\nCandela Scale: %4.2f\nAngle Count V: %i\nAngle Count H: %i\n", 
                profile.lightCount, 
                profile.totalLumens, 
                profile.candelaScale,
                profile.angleCountV,
                profile.angleCountH);

            return -1;
        }

        auto minValueV = -3.402823466e+38f;
        auto minValueH = -3.402823466e+38f;
        profile.anglesV.reserve(profile.angleCountV);
        profile.anglesH.reserve(profile.angleCountH);
        profile.candelaValues.reserve(profile.angleCountV * profile.angleCountH);

        for (auto yy = 0; yy < profile.angleCountV; ++yy)
        {
            auto value = ReadFloat(head);

            if (value < minValueV)
            {
                printf("Unsorted input values for angle values are not supported!\n");
                return -1;
            }

            minValueV = value;
            profile.anglesV.push_back(value);
        }

        for (auto xx = 0; xx < profile.angleCountH; ++xx)
        {
            auto value = ReadFloat(head);

            if (value < minValueH)
            {
                printf("Unsorted input values for angle values are not supported!\n");
                return -1;
            }

            minValueH = value;
            profile.anglesH.push_back(value);
        }

        for (auto xx = 0; xx < profile.angleCountH; ++xx)
        for (auto yy = 0; yy < profile.angleCountV; ++yy)
        {
            profile.candelaValues.push_back(ReadFloat(head) * profile.candelaScale);
        }

        SkipWhiteSpace(head);

        if (*head)
        {
            auto line = ReadLine(head, true);

            if (strcmp(line.c_str(), "END") == 0u)
            {
                SkipWhiteSpace(head);
            }
        }

        if (*head)
        {
            printf("Unexpected file content after 'END' signal!\n");
            return -1;
        }

        constexpr const auto PI = 3.1415926535f;
        constexpr const auto sampleCount = 500000u;
        auto candelaAverage = 0.0;

        for (auto i = 0u; i < sampleCount; ++i)
        {
            auto Ex = 2.0f * HammersleyX(i, sampleCount) - 1.0f;
            auto Ey = 2.0f * HammersleyY(i) - 1.0f;
            const auto d = 1.0f - (fabs(Ex) + fabs(Ey));
            const auto r = 1.0f - fabs(d);
            const auto phi = r == 0.0f ? 0.0f : PI / 4.0f * ((fabs(Ey) - fabs(Ex)) / r + 1.0f);
            const auto f = r * sqrtf(2.0f - r * r);
            const auto vx = f * (Ex > -0.0f ? 1.0f : -1.0f) * cosf(phi);
            const auto vy = f * (Ey > -0.0f ? 1.0f : -1.0f) * sinf(phi);
            const auto vz = (d > -0.0f ? 1.0f : -1.0f) * (1.0f - r * r);
            const auto angleH = acosf(vz) / PI * 180.0f;
            const auto angleV = atan2f(vy, vx) / PI * 180.0f + 180.0f;
            candelaAverage += Interpolate2D(profile, angleV, angleH);
        }

        candelaAverage /= sampleCount;

        auto maxValue = ComputeMaxCandela(profile.candelaValues);
        auto invMaxValue = 1.0f / maxValue;

        auto buffer = PKAssetBuffer();
        buffer.header->type = PKAssetType::IESProfile;
        WriteName(buffer.header->name, filename.c_str());

        auto pkIESProfile = buffer.Allocate<PKIESProfile>();
        pkIESProfile->lumens = profile.totalLumens < 0.0f ? 1000.0f : profile.totalLumens / profile.lightCount;
        pkIESProfile->candelaMax = maxValue;
        pkIESProfile->candelaAverage = candelaAverage;

        {
            std::vector<float> values;
            values.resize(PK_IES_PROFILE_WIDTH * PK_IES_PROFILE_HEIGHT);
            auto pValuesf32 = values.data();

            for (auto yy = 0u; yy < PK_IES_PROFILE_HEIGHT; ++yy)
            for (auto xx = 0u; xx < PK_IES_PROFILE_WIDTH; ++xx)
            {
                auto angleV = xx * 180.0f / PK_IES_PROFILE_WIDTH;
                auto angleH = yy * 180.0f / PK_IES_PROFILE_HEIGHT;
                *pValuesf32++ = invMaxValue * Interpolate2D(profile, angleV, angleH);
            }

            auto compressedSize = 0ull;
            Texture::BlockCompressBC4(values.data(), PK_IES_PROFILE_WIDTH, PK_IES_PROFILE_HEIGHT, nullptr, &compressedSize);

            auto pData = buffer.Allocate<uint8_t>(compressedSize);
            pkIESProfile->data.Set(buffer.data(), pData.get());
            Texture::BlockCompressBC4(values.data(), PK_IES_PROFILE_WIDTH, PK_IES_PROFILE_HEIGHT, pData.get(), &compressedSize);
        }

        return WriteAsset(pathDst, pathStemOffset, buffer, false);
    }
}