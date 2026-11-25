#pragma once

#include "Object3d.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace AppFastObjLoader
{
	namespace Detail
	{
		inline void SkipSpacesInLine(const char*& p, const char* end) noexcept
		{
			while (p < end && (*p == ' ' || *p == '\t'))
			{
				++p;
			}
		}

		inline float FastParseFloat(const char*& p, const char* end) noexcept
		{
			SkipSpacesInLine(p, end);
			if (p >= end) return 0.0f;

			double sign = 1.0;
			if (*p == '-') { sign = -1.0; ++p; }
			else if (*p == '+') { ++p; }

			double val = 0.0;
			while (p < end && *p >= '0' && *p <= '9')
			{
				val = val * 10.0 + static_cast<double>(*p - '0');
				++p;
			}
			if (p < end && *p == '.')
			{
				++p;
				double place = 0.1;
				while (p < end && *p >= '0' && *p <= '9')
				{
					val += static_cast<double>(*p - '0') * place;
					place *= 0.1;
					++p;
				}
			}
			if (p < end && (*p == 'e' || *p == 'E'))
			{
				++p;
				int expSign = 1;
				if (p < end && *p == '-') { expSign = -1; ++p; }
				else if (p < end && *p == '+') { ++p; }
				int expVal = 0;
				while (p < end && *p >= '0' && *p <= '9')
				{
					expVal = expVal * 10 + (*p - '0');
					++p;
				}
				val *= std::pow(10.0, static_cast<double>(expSign * expVal));
			}
			return static_cast<float>(val * sign);
		}

		inline uint32_t FastParseUint(const char*& p, const char* end) noexcept
		{
			uint32_t val = 0;
			bool any = false;
			while (p < end && *p >= '0' && *p <= '9')
			{
				val = val * 10u + static_cast<uint32_t>(*p - '0');
				any = true;
				++p;
			}
			return any ? val : 1u;
		}
	}

	// アプリケーション層専用の高速ゼロアロケーションOBJローダー＆プロセス共通キャッシュ
	// （Baziru3_Engine層には一切手を触れず、Debug時のistringstream大量生成ボトルネックを解消）
	inline const Object3d::ModelData& LoadObjCached(const std::string& directoryPath, const std::string& filename)
	{
		static std::unordered_map<std::string, Object3d::ModelData> sCache;

		const std::string fullPath = directoryPath + "/" + filename;
		if (auto it = sCache.find(fullPath); it != sCache.end())
		{
			return it->second;
		}

		std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			auto inserted = sCache.emplace(fullPath, Object3d::LoadObjFile(directoryPath, filename));
			return inserted.first->second;
		}

		const std::streamsize fileSize = file.tellg();
		file.seekg(0, std::ios::beg);
		std::string buffer(static_cast<size_t>((std::max)(fileSize, std::streamsize(0))), '\0');
		if (!buffer.empty())
		{
			file.read(buffer.data(), fileSize);
		}

		Object3d::ModelData modelData{};
		const size_t approxLines = buffer.size() / 28u + 16u;
		std::vector<Vector4> positions;
		std::vector<Vector3> normals;
		std::vector<Vector2> texcoords;
		positions.reserve(approxLines / 3u);
		texcoords.reserve(approxLines / 3u);
		normals.reserve(approxLines / 3u);
		modelData.vertices.reserve(approxLines);
		modelData.indices.reserve(approxLines);

		const char* cur = buffer.data();
		const char* bufEnd = cur + buffer.size();

		while (cur < bufEnd)
		{
			const char* lineEnd = static_cast<const char*>(
				std::memchr(cur, '\n', static_cast<size_t>(bufEnd - cur)));
			const char* nextLine = lineEnd ? (lineEnd + 1) : bufEnd;
			if (!lineEnd) lineEnd = bufEnd;
			if (lineEnd > cur && *(lineEnd - 1) == '\r') --lineEnd;

			const char* p = cur;
			cur = nextLine;

			Detail::SkipSpacesInLine(p, lineEnd);
			if (p >= lineEnd || *p == '#') continue;

			if (p[0] == 'v' && (p + 1 < lineEnd))
			{
				const char c1 = p[1];
				if (c1 == ' ' || c1 == '\t')
				{
					p += 2;
					Vector4 pos;
					pos.x = Detail::FastParseFloat(p, lineEnd);
					pos.y = Detail::FastParseFloat(p, lineEnd);
					pos.z = Detail::FastParseFloat(p, lineEnd);
					pos.w = 1.0f;
					positions.push_back(pos);
				}
				else if (c1 == 't' && (p + 2 < lineEnd) && (p[2] == ' ' || p[2] == '\t'))
				{
					p += 3;
					Vector2 uv;
					uv.x = Detail::FastParseFloat(p, lineEnd);
					uv.y = 1.0f - Detail::FastParseFloat(p, lineEnd);
					texcoords.push_back(uv);
				}
				else if (c1 == 'n' && (p + 2 < lineEnd) && (p[2] == ' ' || p[2] == '\t'))
				{
					p += 3;
					Vector3 nrm;
					nrm.x = Detail::FastParseFloat(p, lineEnd);
					nrm.y = Detail::FastParseFloat(p, lineEnd);
					nrm.z = Detail::FastParseFloat(p, lineEnd);
					normals.push_back(nrm);
				}
			}
			else if (p[0] == 'f' && (p + 1 < lineEnd) && (p[1] == ' ' || p[1] == '\t'))
			{
				p += 2;
				Sprite::VertexData triangle[3]{};
				int32_t parsedCount = 0;

				for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex)
				{
					Detail::SkipSpacesInLine(p, lineEnd);
					if (p >= lineEnd) break;

					uint32_t elementIndices[3] = { 1u, 1u, 1u };
					for (int32_t element = 0; element < 3; ++element)
					{
						if (p < lineEnd && *p != ' ' && *p != '\t' && *p != '/')
						{
							elementIndices[element] = Detail::FastParseUint(p, lineEnd);
						}
						if (p < lineEnd && *p == '/') { ++p; }
						else { break; }
					}
					while (p < lineEnd && *p != ' ' && *p != '\t') ++p;

					const Vector4 position = (elementIndices[0] > 0 && elementIndices[0] <= positions.size())
						? positions[elementIndices[0] - 1]
						: Vector4{ 0.0f, 0.0f, 0.0f, 1.0f };
					const Vector2 texcoord = (elementIndices[1] > 0 && elementIndices[1] <= texcoords.size())
						? texcoords[elementIndices[1] - 1]
						: Vector2{ 0.0f, 0.0f };
					const Vector3 normal = (elementIndices[2] > 0 && elementIndices[2] <= normals.size())
						? normals[elementIndices[2] - 1]
						: Vector3{ 0.0f, 1.0f, 0.0f };

					triangle[faceVertex] = { position, texcoord, normal };
					++parsedCount;
				}

				if (parsedCount == 3)
				{
					const uint32_t baseIdx = static_cast<uint32_t>(modelData.vertices.size());
					modelData.vertices.push_back(triangle[0]);
					modelData.vertices.push_back(triangle[1]);
					modelData.vertices.push_back(triangle[2]);
					modelData.indices.push_back(baseIdx);
					modelData.indices.push_back(baseIdx + 1u);
					modelData.indices.push_back(baseIdx + 2u);
				}
			}
			else if ((lineEnd - p) > 7 && std::memcmp(p, "mtllib", 6) == 0 && (p[6] == ' ' || p[6] == '\t'))
			{
				p += 7;
				Detail::SkipSpacesInLine(p, lineEnd);
				const char* tokEnd = p;
				while (tokEnd < lineEnd && *tokEnd != ' ' && *tokEnd != '\t') ++tokEnd;
				if (tokEnd > p)
				{
					std::string materialFilename(p, static_cast<size_t>(tokEnd - p));
					modelData.material = Object3d::LoadMaterialTemplateFile(directoryPath, materialFilename);
				}
			}
		}

		auto inserted = sCache.emplace(fullPath, std::move(modelData));
		return inserted.first->second;
	}
}

