struct Vertex {
    float32_t4 position;
    float32_t2 texcoord;
    float32_t3 normal;
};

struct VertexInfluence {
    float32_t4 weight;
    int32_t4 index;
};

struct SkinningInformation {
    uint32_t numVertices;
};

struct Well
{
    float32_t4x4 skeletonSpaceMatrix;
    float32_t4x4 skeletonSpaceInverseTransposeMatrix;
};

// resources
StructuredBuffer<Well> gMatrixPalette : register(t0);
StructuredBuffer<Vertex> gInputVertices : register(t1);
StructuredBuffer<VertexInfluence> gVertexInfluences : register(t2);
RWStructuredBuffer<Vertex> gOutputVertices : register(u0);
ConstantBuffer<SkinningInformation> gSkinningInformation : register(b0);

[numthreads(1024, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint32_t vertexIndex = DTid.x;

    if (vertexIndex < gSkinningInformation.numVertices)
    {
        // 必要なデータをStructuredBufferから取得
        Vertex input = gInputVertices[vertexIndex];
        VertexInfluence influence = gVertexInfluences[vertexIndex];

        // Skinning後の頂点を計算
        Vertex skinned;
        skinned.texcoord = input.texcoord;
        
        float32_t4 skinnedPosition = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
        float32_t3 skinnedNormal = float32_t3(0.0f, 0.0f, 0.0f);
        
        for (int32_t i = 0; i < 4; ++i)
        {
            float32_t weight = influence.weight[i];
            int32_t index = influence.index[i];
            if (weight > 0.0f)
            {
                // 行列パレットの適用とウェイトの加算
                skinnedPosition += mul(input.position, gMatrixPalette[index].skeletonSpaceMatrix) * weight;
                skinnedNormal += mul(input.normal, (float32_t3x3)gMatrixPalette[index].skeletonSpaceInverseTransposeMatrix) * weight;
            }
        }
        
        skinnedPosition.w = 1.0f;
        skinned.position = skinnedPosition;
        skinned.normal = normalize(skinnedNormal);

        // Skinning後の頂点データをUAVに書き込む
        gOutputVertices[vertexIndex] = skinned;
    }
}