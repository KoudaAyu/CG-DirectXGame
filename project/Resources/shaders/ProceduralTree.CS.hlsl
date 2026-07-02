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

    // 風の揺れアニメーションを start / end それぞれ元の座標の高さから計算（ちぎれを防止）
    float startSway = max(0.0f, seg.startPos.y * 0.15f);
    float3 startWind = gTreeCB.windDirection * sin(gTreeCB.time * 2.5f + seg.startPos.y * 0.5f) * gTreeCB.windStrength * startSway;

    float endSway = max(0.0f, seg.endPos.y * 0.15f);
    float3 endWind = gTreeCB.windDirection * sin(gTreeCB.time * 2.5f + seg.endPos.y * 0.5f) * gTreeCB.windStrength * endSway;

    start += startWind;
    end += endWind;

    float3 dir = normalize(end - start);
    
    // 変形後の dir に基づいて right と up を再算出（法線のねじれとしなりを完全に同期）
    float3 right = normalize(seg.right);
    float3 up = normalize(seg.up);
    // Gram-Schmidt で dir に直交する面を構築
    float3 deformRight = normalize(right - dir * dot(right, dir));
    float3 deformUp = normalize(cross(dir, deformRight));

    // 8角形円柱の頂点を生成
    const uint radialSegments = 8;
    for (uint i = 0; i <= radialSegments; ++i)
    {
        float angle = (float)i * 2.0f * 3.14159265f / (float)radialSegments;
        float c = cos(angle);
        float s = sin(angle);

        // 変形後の直交座標系から法線と周囲頂点を計算
        float3 ringDir = deformRight * c + deformUp * s;

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
    float branchLen = length(seg.endPos - seg.startPos);
    float leafSize = branchLen * 0.35f;

    for (uint leafIdx = 0; leafIdx < 4; ++leafIdx)
    {
        float rollAngleRad = (float)leafIdx * 3.14159265f / 4.0f;
        float3 rightVec = normalize(deformRight);
        float3 upVec = normalize(deformUp);

        // Rodrigues' rotation formula to rotate right/up vectors around dir axis
        float cRoll = cos(rollAngleRad);
        float sRoll = sin(rollAngleRad);
        float3 rotRight = rightVec * cRoll + cross(dir, rightVec) * sRoll + dir * dot(dir, rightVec) * (1.0f - cRoll);
        float3 rotUp = upVec * cRoll + cross(dir, upVec) * sRoll + dir * dot(dir, upVec) * (1.0f - cRoll);

        uint baseV = leafVertexOffset + leafIdx * 8;

        for (uint quadIdx = 0; quadIdx < 2; ++quadIdx)
        {
            float rotAngleRad = (float)quadIdx * 3.14159265f / 2.0f;
            float cQ = cos(rotAngleRad);
            float sQ = sin(rotAngleRad);
            float3 qRight = rotRight * cQ + cross(dir, rotRight) * sQ + dir * dot(dir, rotRight) * (1.0f - cQ);
            float3 qUp = rotUp * cQ + cross(dir, rotUp) * sQ + dir * dot(dir, rotUp) * (1.0f - cQ);

            // Compute precise quad vertices matching CPU layout (left-bottom, right-bottom, right-top, left-top)
            float3 p0 = leafCenter - qRight * leafSize - qUp * leafSize;
            float3 p1 = leafCenter + qRight * leafSize - qUp * leafSize;
            float3 p2 = leafCenter + qRight * leafSize + qUp * leafSize;
            float3 p3 = leafCenter - qRight * leafSize + qUp * leafSize;

            float3 normal = normalize(cross(qRight, qUp));

            uint qOffset = baseV + quadIdx * 4;

            gOutVertices[qOffset + 0].position = float4(p0, 1.0f);
            gOutVertices[qOffset + 0].texcoord = float2(0.0f, 0.0f);
            gOutVertices[qOffset + 0].normal = normal;

            gOutVertices[qOffset + 1].position = float4(p1, 1.0f);
            gOutVertices[qOffset + 1].texcoord = float2(0.1f, 0.0f);
            gOutVertices[qOffset + 1].normal = normal;

            gOutVertices[qOffset + 2].position = float4(p2, 1.0f);
            gOutVertices[qOffset + 2].texcoord = float2(0.1f, 0.1f);
            gOutVertices[qOffset + 2].normal = normal;

            gOutVertices[qOffset + 3].position = float4(p3, 1.0f);
            gOutVertices[qOffset + 3].texcoord = float2(0.0f, 0.1f);
            gOutVertices[qOffset + 3].normal = normal;
        }
    }
}
