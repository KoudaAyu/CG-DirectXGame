#ifndef COPYIMAGE_HLSLI
#define COPYIMAGE_HLSLI

struct VertexShaderOutput {
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
};

#endif
