struct Vertex {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
    float4 color : COLOR;
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
        // 描画されない未使用領域の頂点はスケールを0にして見えなくする
        if (segmentIndex < maxSegments)
        {
            uint branchVertexOffset = segmentIndex * 18;
            for (uint i = 0; i < 18; ++i)
            {
                gOutVertices[branchVertexOffset + i].position = float3(0, 0, 0);
            }
            uint leafVertexOffset = maxSegments * 18 + segmentIndex * 32;
            for (uint j = 0; j < 32; ++j)
            {
                gOutVertices[leafVertexOffset + j].position = float3(0, 0, 0);
            }
        }
        return;
    }

    BranchSegment seg = gSegments[segmentIndex];

    // --- 1. 枝（シリンダー）の頂点生成 ---
    uint branchVertexOffset = segmentIndex * 18;
    float3 start = seg.startPos;
    float3 end = seg.endPos;

    // 簡単な風の揺れ（Wind Sway）を計算（高さに応じて揺らす）
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
        gOutVertices[vIndexStart].position = start + ringDir * seg.startRadius;
        gOutVertices[vIndexStart].normal = ringDir;
        gOutVertices[vIndexStart].texcoord = float2((float)i / (float)radialSegments, 0.0f);
        gOutVertices[vIndexStart].color = float4(0.0f, 0.0f, 0.0f, 1.0f); // 苔/風なし

        // endPos側 (頂点 9〜17)
        uint vIndexEnd = branchVertexOffset + radialSegments + 1 + i;
        gOutVertices[vIndexEnd].position = end + ringDir * seg.endRadius;
        gOutVertices[vIndexEnd].normal = ringDir;
        gOutVertices[vIndexEnd].texcoord = float2((float)i / (float)radialSegments, 1.0f);
        gOutVertices[vIndexEnd].color = float4(0.0f, 0.0f, 0.0f, 1.0f); // 苔/風なし
    }

    // --- 2. 葉（Crossed Quad）の頂点生成 ---
    uint leafVertexOffset = maxSegments * 18 + segmentIndex * 32;

    if (seg.isLeafEmitter == 0)
    {
        // 葉がないセグメントはスケール0で潰す
        for (uint j = 0; j < 32; ++j)
        {
            gOutVertices[leafVertexOffset + j].position = float3(0, 0, 0);
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
        gOutVertices[baseV + 0].position = leafCenter + (-leafRight - leafUp) * leafSize * 0.5f;
        gOutVertices[baseV + 0].texcoord = float2(0.0f, 1.0f);
        
        gOutVertices[baseV + 1].position = leafCenter + (-leafRight + leafUp) * leafSize * 0.5f;
        gOutVertices[baseV + 1].texcoord = float2(0.0f, 0.0f);
        
        gOutVertices[baseV + 2].position = leafCenter + (leafRight - leafUp) * leafSize * 0.5f;
        gOutVertices[baseV + 2].texcoord = float2(1.0f, 1.0f);
        
        gOutVertices[baseV + 3].position = leafCenter + (leafRight + leafUp) * leafSize * 0.5f;
        gOutVertices[baseV + 3].texcoord = float2(1.0f, 0.0f);

        // Crossed Quad 2 (直交、4頂点)
        float3 leafRightOrtho = cross(leafRight, leafUp);
        
        gOutVertices[baseV + 4].position = leafCenter + (-leafRightOrtho - leafUp) * leafSize * 0.5f;
        gOutVertices[baseV + 4].texcoord = float2(0.0f, 1.0f);
        
        gOutVertices[baseV + 5].position = leafCenter + (-leafRightOrtho + leafUp) * leafSize * 0.5f;
        gOutVertices[baseV + 5].texcoord = float2(0.0f, 0.0f);
        
        gOutVertices[baseV + 6].position = leafCenter + (leafRightOrtho - leafUp) * leafSize * 0.5f;
        gOutVertices[baseV + 6].texcoord = float2(1.0f, 1.0f);
        
        gOutVertices[baseV + 7].position = leafCenter + (leafRightOrtho + leafUp) * leafSize * 0.5f;
        gOutVertices[baseV + 7].texcoord = float2(1.0f, 0.0f);

        // 共通設定 (法線・緑色の風揺れウェイトを設定)
        for (uint k = 0; k < 8; ++k)
        {
            gOutVertices[baseV + k].normal = norm;
            gOutVertices[baseV + k].color = float4(0.0f, 1.0f, 0.0f, 1.0f); // 風ウェイト緑 = 1.0f
        }
    }
}
