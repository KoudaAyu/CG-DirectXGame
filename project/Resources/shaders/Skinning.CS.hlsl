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

    
    if(vertexIndex < gSkinningInformation.numVertices)
    {
        //Skinningの計算

        //必要なデータをStructuredBufferから取得
        //SkinningObject3D.VSでは入力頂点として受け取っていた
        Vertex input = gInputVertices[vertexIndex];
        VertexInfluence influence = gVertexInfluences[vertexIndex];

        //Skinning後の頂点を計算
        Vertex skinned;
        skinned.texcoord = input.texcoord;
        
        //計算の方法はSkinningObject3d.VSと同じ
        //データの取得方法が変わるだけで、原理は同じ
        skinned.position = float32_t4(0.0f, 0.0f, 0.0f, 1.0f);
        skinned.normal = float32_t3(0.0f, 0.0f, 0.0f);

        //Skinning後の頂点データを格納、つまり書き込む
        gOutputVertices[vertexIndex] = skinned;
    }
}