#pragma once
#include<string>
#include"Vector.h"
#include<vector>
#include"Matrix4x4.h"
struct Transform
{
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct VertexData
{
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

struct Material
{
	Vector4 color;
	int32_t enableLighting;
	float padding[3]; // パディングを追加して16バイト境界に揃える
	Matrix4x4 uvTransform; // UV変換行列
};

struct DirectionalLight
{
	Vector4 color;
	Vector3 direction;
	float intensity;
};

struct MaterialData
{
	std::string textureFilePath; // テクスチャファイルのパス
};

//objファイル関係
struct ModelData
{
	std::vector<VertexData> vertices; // 頂点データ
	MaterialData material; // マテリアルデータ
};

struct TransformationMatrix
{
	Matrix4x4 WVP;
	Matrix4x4 World;

};


