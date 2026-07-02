struct Vertex {
    float4 position; // Offset 0
    float2 texcoord; // Offset 16
    float3 normal;   // Offset 24
};

struct BranchSegment {
    float3 startPos;
    float startRadius;
    float3 endPos;
    float endRadius;
    float3 right;
    uint isLeafEmitter;
    float3 up;
    float padding[3];
};

struct TreeCB {
    float3 windDirection;
    float windStrength;
    float time;
    uint maxSegments;
    uint currentSegments;
    float padding;
};

ConstantBuffer<TreeCB> gTreeCB : register(b0);
StructuredBuffer<BranchSegment> gSegments : register(t0);
RWStructuredBuffer<Vertex> gOutVertices : register(u0);

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint segmentIndex = DTid.x;
    uint maxSegments = gTreeCB.maxSegments;

    if (segmentIndex >= gTreeCB.currentSegments || segmentIndex >= maxSegments)
    {
        // 未使用頂点を0に潰す
        if (segmentIndex < maxSegments)
        {
            uint branchVertexOffset = segmentIndex * 18;
            for (uint i = 0; i < 18; ++i)
            {
                gOutVertices[branchVertexOffset + i].position = float4(0, 0, 0, 1.0f);
            }
            uint leafVertexOffset = maxSegments * 18 + segmentIndex * 32;
            for (uint j = 0; j < 32; ++j)
            {
                gOutVertices[leafVertexOffset + j].position = float4(0, 0, 0, 1.0f);
            }
        }
        return;
    }

    BranchSegment seg = gSegments[segmentIndex];

    // --- 1. 枝（シリンダー）の頂点生成 ---
    uint branchVertexOffset = segmentIndex * 18;
    float3 start = seg.startPos;
    float3 end = seg.endPos;

    // 風の揺れアニメーション
    float swayFactor = max(0.0f, end.y * 0.15f);
    float3 windOffset = gTreeCB.windDirection * sin(gTreeCB.time * 2.5f + end.y * 0.5f) * gTreeCB.windStrength * swayFactor;
    end += windOffset;
    start += windOffset * 0.7f;

    float3 dir = normalize(end - start);
    float3 right = normalize(seg.right);
    float3 up = normalize(seg.up);

    // 8角形円柱の頂点を生成
    const uint radialSegments = 8;
    for (uint i = 0; i <= radialSegments; ++i)
    {
        float angle = (float)i * 2.0f * 3.14159265f / (float)radialSegments;
        float c = cos(angle);
        float s = sin(angle);

        float3 ringDir = right * c + up * s;

        // startPos側 (頂点 0〜8)
        uint vIndexStart = branchVertexOffset + i;
        gOutVertices[vIndexStart].position = float4(start + ringDir * seg.startRadius, 1.0f);
        gOutVertices[vIndexStart].normal = ringDir;
        gOutVertices[vIndexStart].texcoord = float2((float)i / (float)radialSegments, 0.0f);

        // endPos側 (頂点 9〜17)
        uint vIndexEnd = branchVertexOffset + radialSegments + 1 + i;
        gOutVertices[vIndexEnd].position = float4(end + ringDir * seg.endRadius, 1.0f);
        gOutVertices[vIndexEnd].normal = ringDir;
        gOutVertices[vIndexEnd].texcoord = float2((float)i / (float)radialSegments, 1.0f);
    }

    // --- 2. 葉（Crossed Quad）の頂点生成 ---
    uint leafVertexOffset = maxSegments * 18 + segmentIndex * 32;

    if (seg.isLeafEmitter == 0)
    {
        // 葉がないセグメントはスケール0で潰す
        for (uint j = 0; j < 32; ++j)
        {
            gOutVertices[leafVertexOffset + j].position = float4(0, 0, 0, 1.0f);
        }
        return;
    }

    // 葉を4枚配置（4枚の Crossed Quad = 4枚 * 2Quad (8頂点) = 32頂点）
    float3 leafCenter = end;
    float leafSize = 0.5f;

    float3 leafNormals[4] = {
        float3(1, 0, 0),
        float3(0, 0, 1),
        float3(0.707, 0, 0.707),
        float3(-0.707, 0, 0.707)
    };

    for (uint leafIdx = 0; leafIdx < 4; ++leafIdx)
    {
        float3 norm = leafNormals[leafIdx];
        float angleOffset = (float)leafIdx * 3.14159265f / 4.0f;
        float3 leafRight = right * cos(angleOffset) + up * sin(angleOffset);
        float3 leafUp = dir; // 枝の伸びる方向

        uint baseV = leafVertexOffset + leafIdx * 8;
        
        // Crossed Quad 1 (4頂点)
        gOutVertices[baseV + 0].position = float4(leafCenter + (-leafRight - leafUp) * leafSize * 0.5f, 1.0f);
        gOutVertices[baseV + 0].texcoord = float2(0.0f, 1.0f);
        
        gOutVertices[baseV + 1].position = float4(leafCenter + (-leafRight + leafUp) * leafSize * 0.5f, 1.0f);
        gOutVertices[baseV + 1].texcoord = float2(0.0f, 0.0f);
        
        gOutVertices[baseV + 2].position = float4(leafCenter + (leafRight - leafUp) * leafSize * 0.5f, 1.0f);
        gOutVertices[baseV + 2].texcoord = float2(1.0f, 1.0f);
        
        gOutVertices[baseV + 3].position = float4(leafCenter + (leafRight + leafUp) * leafSize * 0.5f, 1.0f);
        gOutVertices[baseV + 3].texcoord = float2(1.0f, 0.0f);

        // Crossed Quad 2 (直交、4頂点)
        float3 leafRightOrtho = cross(leafRight, leafUp);
        
        gOutVertices[baseV + 4].position = float4(leafCenter + (-leafRightOrtho - leafUp) * leafSize * 0.5f, 1.0f);
        gOutVertices[baseV + 4].texcoord = float2(0.0f, 1.0f);
        
        gOutVertices[baseV + 5].position = float4(leafCenter + (-leafRightOrtho + leafUp) * leafSize * 0.5f, 1.0f);
        gOutVertices[baseV + 5].texcoord = float2(0.0f, 0.0f);
        
        gOutVertices[baseV + 6].position = float4(leafCenter + (leafRightOrtho - leafUp) * leafSize * 0.5f, 1.0f);
        gOutVertices[baseV + 6].texcoord = float2(1.0f, 1.0f);
        
        gOutVertices[baseV + 7].position = float4(leafCenter + (leafRightOrtho + leafUp) * leafSize * 0.5f, 1.0f);
        gOutVertices[baseV + 7].texcoord = float2(1.0f, 0.0f);

        // 共通設定
        for (uint k = 0; k < 8; ++k)
        {
            gOutVertices[baseV + k].normal = norm;
        }
    }
}
