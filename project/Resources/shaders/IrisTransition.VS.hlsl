struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;
    // 画面全体を覆うフルスクリーントライアングル（頂点バッファ不要）
    // vertexId 0: UV(0, 0), Pos(-1,  1)
    // vertexId 1: UV(2, 0), Pos( 3,  1)
    // vertexId 2: UV(0, 2), Pos(-1, -3)
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.uv = uv;
    output.position = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}
