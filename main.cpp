#include<Windows.h>

#include<chrono> //時間を扱うライブラリ
#include<cstdint>
#include<filesystem> //ファイルやディレクトリに関する操作を行うライブラリ
#include<format> //文字列のフォーマットを行うライブラリ
#include<fstream> //ファイルにかいたり読んだりするライブラリ
#include<string> //文字列を扱うライブラリ
#include<strsafe.h>

#include<d3d12.h>
#include<dxgi1_6.h>
#include<cassert>

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

//Debug用
#include<dbghelp.h>
#pragma comment(lib,"Dbghelp.lib")

//ReportLiveObjects
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")

//DXCの初期化
#include<dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")

//Textureの転送
#include"externals/DirectXTex/d3dx12.h"
#include<vector>

#include <DirectXMath.h>
#include<cmath>
#include "externals/DirectXTex/DirectXTex.h"

//Imgui使用のため
#include"externals/imgui/imgui.h"
#include"externals/imgui/imgui_impl_dx12.h"
#include"externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lParam);



struct Vector2
{
	float x;
	float y;
};

struct Vector3
{
	float x;
	float y;
	float z;
};

struct Transform
{
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct Vector4
{
	float x;
	float y;
	float z;
	float w;
};

struct Matrix4x4
{
	float m[4][4];
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
};



Matrix4x4 MakeIdentity4x4()
{
	return {
		{
			{1, 0, 0, 0},
			{0, 1, 0, 0},
			{0, 0, 1, 0},
			{0, 0, 0, 1}
		}
	};
}

Matrix4x4 Multiply(Matrix4x4 m1, Matrix4x4 m2)
{
	Matrix4x4 result;

	result.m[0][0] = m1.m[0][0] * m2.m[0][0] + m1.m[0][1] * m2.m[1][0] + m1.m[0][2] * m2.m[2][0] + m1.m[0][3] * m2.m[3][0];
	result.m[0][1] = m1.m[0][0] * m2.m[0][1] + m1.m[0][1] * m2.m[1][1] + m1.m[0][2] * m2.m[2][1] + m1.m[0][3] * m2.m[3][1];
	result.m[0][2] = m1.m[0][0] * m2.m[0][2] + m1.m[0][1] * m2.m[1][2] + m1.m[0][2] * m2.m[2][2] + m1.m[0][3] * m2.m[3][2];
	result.m[0][3] = m1.m[0][0] * m2.m[0][3] + m1.m[0][1] * m2.m[1][3] + m1.m[0][2] * m2.m[2][3] + m1.m[0][3] * m2.m[3][3];

	result.m[1][0] = m1.m[1][0] * m2.m[0][0] + m1.m[1][1] * m2.m[1][0] + m1.m[1][2] * m2.m[2][0] + m1.m[1][3] * m2.m[3][0];
	result.m[1][1] = m1.m[1][0] * m2.m[0][1] + m1.m[1][1] * m2.m[1][1] + m1.m[1][2] * m2.m[2][1] + m1.m[1][3] * m2.m[3][1];
	result.m[1][2] = m1.m[1][0] * m2.m[0][2] + m1.m[1][1] * m2.m[1][2] + m1.m[1][2] * m2.m[2][2] + m1.m[1][3] * m2.m[3][2];
	result.m[1][3] = m1.m[1][0] * m2.m[0][3] + m1.m[1][1] * m2.m[1][3] + m1.m[1][2] * m2.m[2][3] + m1.m[1][3] * m2.m[3][3];

	result.m[2][0] = m1.m[2][0] * m2.m[0][0] + m1.m[2][1] * m2.m[1][0] + m1.m[2][2] * m2.m[2][0] + m1.m[2][3] * m2.m[3][0];
	result.m[2][1] = m1.m[2][0] * m2.m[0][1] + m1.m[2][1] * m2.m[1][1] + m1.m[2][2] * m2.m[2][1] + m1.m[2][3] * m2.m[3][1];
	result.m[2][2] = m1.m[2][0] * m2.m[0][2] + m1.m[2][1] * m2.m[1][2] + m1.m[2][2] * m2.m[2][2] + m1.m[2][3] * m2.m[3][2];
	result.m[2][3] = m1.m[2][0] * m2.m[0][3] + m1.m[2][1] * m2.m[1][3] + m1.m[2][2] * m2.m[2][3] + m1.m[2][3] * m2.m[3][3];

	result.m[3][0] = m1.m[3][0] * m2.m[0][0] + m1.m[3][1] * m2.m[1][0] + m1.m[3][2] * m2.m[2][0] + m1.m[3][3] * m2.m[3][0];
	result.m[3][1] = m1.m[3][0] * m2.m[0][1] + m1.m[3][1] * m2.m[1][1] + m1.m[3][2] * m2.m[2][1] + m1.m[3][3] * m2.m[3][1];
	result.m[3][2] = m1.m[3][0] * m2.m[0][2] + m1.m[3][1] * m2.m[1][2] + m1.m[3][2] * m2.m[2][2] + m1.m[3][3] * m2.m[3][2];
	result.m[3][3] = m1.m[3][0] * m2.m[0][3] + m1.m[3][1] * m2.m[1][3] + m1.m[3][2] * m2.m[2][3] + m1.m[3][3] * m2.m[3][3];

	return result;
}

Matrix4x4 MakeScaleMatrix(const Vector3& scale)
{
	Matrix4x4 result;
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			if (i == j)
			{
				if (i == 0)
				{
					result.m[i][j] = scale.x;
				}
				else if (i == 1)
				{
					result.m[i][j] = scale.y;
				}
				else if (i == 2)
				{
					result.m[i][j] = scale.z;
				}
				else
				{
					result.m[i][j] = 1.0f;
				}
			}
			else
			{
				result.m[i][j] = 0.0f;
			}
		}
	}

	return result;
}

Matrix4x4 MakeRotateXMatrix(float radian)
{
	Matrix4x4 result;
	result.m[0][0] = 1.0f;
	result.m[0][1] = 0.0f;
	result.m[0][2] = 0.0f;
	result.m[0][3] = 0.0f;

	result.m[1][0] = 0.0f;
	result.m[1][1] = std::cos(radian);
	result.m[1][2] = std::sin(radian);
	result.m[1][3] = 0.0f;

	result.m[2][0] = 0.0f;
	result.m[2][1] = -std::sin(radian);
	result.m[2][2] = std::cos(radian);
	result.m[2][3] = 0.0f;

	result.m[3][0] = 0.0f;
	result.m[3][1] = 0.0f;
	result.m[3][2] = 0.0f;
	result.m[3][3] = 1.0f;

	return result;
}

Matrix4x4 MakeRotateYMatrix(float radian)
{
	Matrix4x4 result;
	result.m[0][0] = std::cos(radian);
	result.m[0][1] = 0.0f;
	result.m[0][2] = -std::sin(radian);
	result.m[0][3] = 0.0f;

	result.m[1][0] = 0.0f;
	result.m[1][1] = 1.0f;
	result.m[1][2] = 0.0f;
	result.m[1][3] = 0.0f;

	result.m[2][0] = std::sin(radian);
	result.m[2][1] = 0.0f;
	result.m[2][2] = std::cos(radian);
	result.m[2][3] = 0.0f;

	result.m[3][0] = 0.0f;
	result.m[3][1] = 0.0f;
	result.m[3][2] = 0.0f;
	result.m[3][3] = 1.0f;

	return result;
}

Matrix4x4 MakeRotateZMatrix(float radian)
{
	Matrix4x4 result;
	result.m[0][0] = std::cos(radian);
	result.m[0][1] = std::sin(radian);
	result.m[0][2] = 0.0f;
	result.m[0][3] = 0.0f;

	result.m[1][0] = -std::sin(radian);
	result.m[1][1] = std::cos(radian);
	result.m[1][2] = 0.0f;
	result.m[1][3] = 0.0f;

	result.m[2][0] = 0.0f;
	result.m[2][1] = 0.0f;
	result.m[2][2] = 1.0f;
	result.m[2][3] = 0.0f;

	result.m[3][0] = 0.0f;
	result.m[3][1] = 0.0f;
	result.m[3][2] = 0.0f;
	result.m[3][3] = 1.0f;

	return result;
}

// 平行移動行列を作成
Matrix4x4 MakeTranslateMatrix(const Vector3& translate)
{
	Matrix4x4 result = {};

	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			if (i == j)
			{
				result.m[i][j] = 1.0f;
			}
			else if (i == 3)
			{
				if (j == 0)
				{
					result.m[i][j] = translate.x;
				}
				else if (j == 1)
				{
					result.m[i][j] = translate.y;
				}
				else if (j == 2)
				{
					result.m[i][j] = translate.z;
				}
			}
		}
	}

	return result;
}

Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate)
{
	Matrix4x4 S = MakeScaleMatrix(scale);

	Matrix4x4 R = Multiply(MakeRotateXMatrix(rotate.x), Multiply(MakeRotateYMatrix(rotate.y), MakeRotateZMatrix(rotate.z)));

	Matrix4x4 T = MakeTranslateMatrix(translate);

	Matrix4x4 SR = Multiply(S, R);
	Matrix4x4 SRT = Multiply(SR, T);

	return SRT;
}

Matrix4x4 Inverse(Matrix4x4 m)
{
	Matrix4x4 result;
	float InverseVector =
		m.m[0][0] * m.m[1][1] * m.m[2][2] * m.m[3][3]
		+ m.m[0][0] * m.m[1][2] * m.m[2][3] * m.m[3][1]
		+ m.m[0][0] * m.m[1][3] * m.m[2][1] * m.m[3][2]

		- m.m[0][0] * m.m[1][3] * m.m[2][2] * m.m[3][1]
		- m.m[0][0] * m.m[1][2] * m.m[2][1] * m.m[3][3]
		- m.m[0][0] * m.m[1][1] * m.m[2][3] * m.m[3][2]

		- m.m[0][1] * m.m[1][0] * m.m[2][2] * m.m[3][3]
		- m.m[0][2] * m.m[1][0] * m.m[2][3] * m.m[3][1]
		- m.m[0][3] * m.m[1][0] * m.m[2][1] * m.m[3][2]

		+ m.m[0][3] * m.m[1][0] * m.m[2][2] * m.m[3][1]
		+ m.m[0][2] * m.m[1][0] * m.m[2][1] * m.m[3][3]
		+ m.m[0][1] * m.m[1][0] * m.m[2][3] * m.m[3][2]

		+ m.m[0][1] * m.m[1][2] * m.m[2][0] * m.m[3][3]
		+ m.m[0][2] * m.m[1][3] * m.m[2][0] * m.m[3][1]
		+ m.m[0][3] * m.m[1][1] * m.m[2][0] * m.m[3][2]

		- m.m[0][3] * m.m[1][2] * m.m[2][0] * m.m[3][1]
		- m.m[0][2] * m.m[1][1] * m.m[2][0] * m.m[3][3]
		- m.m[0][1] * m.m[1][3] * m.m[2][0] * m.m[3][2]

		- m.m[0][1] * m.m[1][2] * m.m[2][3] * m.m[3][0]
		- m.m[0][2] * m.m[1][3] * m.m[2][1] * m.m[3][0]
		- m.m[0][3] * m.m[1][1] * m.m[2][2] * m.m[3][0]

		+ m.m[0][3] * m.m[1][2] * m.m[2][1] * m.m[3][0]
		+ m.m[0][2] * m.m[1][1] * m.m[2][3] * m.m[3][0]
		+ m.m[0][1] * m.m[1][3] * m.m[2][2] * m.m[3][0];


	result.m[0][0] =
		1 / InverseVector
		* ((m.m[1][1] * m.m[2][2] * m.m[3][3]) + (m.m[1][2] * m.m[2][3] * m.m[3][1]) + (m.m[1][3] * m.m[2][1] * m.m[3][2])
			- (m.m[1][3] * m.m[2][2] * m.m[3][1]) - (m.m[1][2] * m.m[2][1] * m.m[3][3]) - (m.m[1][1] * m.m[2][3] * m.m[3][2]));
	result.m[0][1] =
		1 / InverseVector
		* (-(m.m[0][1] * m.m[2][2] * m.m[3][3]) - (m.m[0][2] * m.m[2][3] * m.m[3][1]) - (m.m[0][3] * m.m[2][1] * m.m[3][2])
			+ (m.m[0][3] * m.m[2][2] * m.m[3][1]) + (m.m[0][2] * m.m[2][1] * m.m[3][3]) + (m.m[0][1] * m.m[2][3] * m.m[3][2]));
	result.m[0][2] =
		1 / InverseVector
		* ((m.m[0][1] * m.m[1][2] * m.m[3][3]) + (m.m[0][2] * m.m[1][3] * m.m[3][1]) + (m.m[0][3] * m.m[1][1] * m.m[3][2])
			- (m.m[0][3] * m.m[1][2] * m.m[3][1]) - (m.m[0][2] * m.m[1][1] * m.m[3][3]) - (m.m[0][1] * m.m[1][3] * m.m[3][2]));
	result.m[0][3] =
		1 / InverseVector
		* (-(m.m[0][1] * m.m[1][2] * m.m[2][3]) - (m.m[0][2] * m.m[1][3] * m.m[2][1]) - (m.m[0][3] * m.m[1][1] * m.m[2][2])
			+ (m.m[0][3] * m.m[1][2] * m.m[2][1]) + (m.m[0][2] * m.m[1][1] * m.m[2][3]) + (m.m[0][1] * m.m[1][3] * m.m[2][2]));

	result.m[1][0] =
		1 / InverseVector
		* (-(m.m[1][0] * m.m[2][2] * m.m[3][3]) - (m.m[1][2] * m.m[2][3] * m.m[3][0]) - (m.m[1][3] * m.m[2][0] * m.m[3][2])
			+ (m.m[1][3] * m.m[2][2] * m.m[3][0]) + (m.m[1][2] * m.m[2][0] * m.m[3][3]) + (m.m[1][0] * m.m[2][3] * m.m[3][2]));
	result.m[1][1] =
		1 / InverseVector
		* ((m.m[0][0] * m.m[2][2] * m.m[3][3]) + (m.m[0][2] * m.m[2][3] * m.m[3][0]) + (m.m[0][3] * m.m[2][0] * m.m[3][2])
			- (m.m[0][3] * m.m[2][2] * m.m[3][0]) - (m.m[0][2] * m.m[2][0] * m.m[3][3]) - (m.m[0][0] * m.m[2][3] * m.m[3][2]));
	result.m[1][2] =
		1 / InverseVector
		* (-(m.m[0][0] * m.m[1][2] * m.m[3][3]) - (m.m[0][2] * m.m[1][3] * m.m[3][0]) - (m.m[0][3] * m.m[1][0] * m.m[3][2])
			+ (m.m[0][3] * m.m[1][2] * m.m[3][0]) + (m.m[0][2] * m.m[1][0] * m.m[3][3]) + (m.m[0][0] * m.m[1][3] * m.m[3][2]));
	result.m[1][3] =
		1 / InverseVector
		* ((m.m[0][0] * m.m[1][2] * m.m[2][3]) + (m.m[0][2] * m.m[1][3] * m.m[2][0]) + (m.m[0][3] * m.m[1][0] * m.m[2][2])
			- (m.m[0][3] * m.m[1][2] * m.m[2][0]) - (m.m[0][2] * m.m[1][0] * m.m[2][3]) - (m.m[0][0] * m.m[1][3] * m.m[2][2]));

	result.m[2][0] =
		1 / InverseVector
		* ((m.m[1][0] * m.m[2][1] * m.m[3][3]) + (m.m[1][1] * m.m[2][3] * m.m[3][0]) + (m.m[1][3] * m.m[2][0] * m.m[3][1])
			- (m.m[1][3] * m.m[2][1] * m.m[3][0]) - (m.m[1][1] * m.m[2][0] * m.m[3][3]) - (m.m[1][0] * m.m[2][3] * m.m[3][1]));
	result.m[2][1] =
		1 / InverseVector
		* (-(m.m[0][0] * m.m[2][1] * m.m[3][3]) - (m.m[0][1] * m.m[2][3] * m.m[3][0]) - (m.m[0][3] * m.m[2][0] * m.m[3][1])
			+ (m.m[0][3] * m.m[2][1] * m.m[3][0]) + (m.m[0][1] * m.m[2][0] * m.m[3][3]) + (m.m[0][0] * m.m[2][3] * m.m[3][1]));
	result.m[2][2] =
		1 / InverseVector
		* ((m.m[0][0] * m.m[1][1] * m.m[3][3]) + (m.m[0][1] * m.m[1][3] * m.m[3][0]) + (m.m[0][3] * m.m[1][0] * m.m[3][1])
			- (m.m[0][3] * m.m[1][1] * m.m[3][0]) - (m.m[0][1] * m.m[1][0] * m.m[3][3]) - (m.m[0][0] * m.m[1][3] * m.m[3][1]));
	result.m[2][3] =
		1 / InverseVector
		* (-(m.m[0][0] * m.m[1][1] * m.m[2][3]) - (m.m[0][1] * m.m[1][3] * m.m[2][0]) - (m.m[0][3] * m.m[1][0] * m.m[2][1])
			+ (m.m[0][3] * m.m[1][1] * m.m[2][0]) + (m.m[0][1] * m.m[1][0] * m.m[2][3]) + (m.m[0][0] * m.m[1][3] * m.m[2][1]));

	result.m[3][0] =
		1 / InverseVector
		* (-(m.m[1][0] * m.m[2][1] * m.m[3][2]) - (m.m[1][1] * m.m[2][2] * m.m[3][0]) - (m.m[1][2] * m.m[2][0] * m.m[3][1])
			+ (m.m[1][2] * m.m[2][1] * m.m[3][0]) + (m.m[1][1] * m.m[2][0] * m.m[3][2]) + (m.m[1][0] * m.m[2][2] * m.m[3][1]));
	result.m[3][1] =
		1 / InverseVector
		* ((m.m[0][0] * m.m[2][1] * m.m[3][2]) + (m.m[0][1] * m.m[2][2] * m.m[3][0]) + (m.m[0][2] * m.m[2][0] * m.m[3][1])
			- (m.m[0][2] * m.m[2][1] * m.m[3][0]) - (m.m[0][1] * m.m[2][0] * m.m[3][2]) - (m.m[0][0] * m.m[2][2] * m.m[3][1]));
	result.m[3][2] =
		1 / InverseVector
		* (-(m.m[0][0] * m.m[1][1] * m.m[3][2]) - (m.m[0][1] * m.m[1][2] * m.m[3][0]) - (m.m[0][2] * m.m[1][0] * m.m[3][1])
			+ (m.m[0][2] * m.m[1][1] * m.m[3][0]) + (m.m[0][1] * m.m[1][0] * m.m[3][2]) + (m.m[0][0] * m.m[1][2] * m.m[3][1]));
	result.m[3][3] =
		1 / InverseVector
		* ((m.m[0][0] * m.m[1][1] * m.m[2][2]) + (m.m[0][1] * m.m[1][2] * m.m[2][0]) + (m.m[0][2] * m.m[1][0] * m.m[2][1])
			- (m.m[0][2] * m.m[1][1] * m.m[2][0]) - (m.m[0][1] * m.m[1][0] * m.m[2][2]) - (m.m[0][0] * m.m[1][2] * m.m[2][1]));

	return result;
}

Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearZ, float farZ)
{
	Matrix4x4 result = {}; // 全要素を0で初期化

	float tanHalfFov = std::tan(fovY / 2.0f);

	result.m[0][0] = 1.0f / (aspectRatio * tanHalfFov);
	result.m[1][1] = 1.0f / tanHalfFov;
	result.m[2][2] = farZ / (farZ - nearZ);
	result.m[2][3] = 1.0f;
	result.m[3][2] = (-farZ * nearZ) / (farZ - nearZ);
	result.m[3][3] = 0.0f; // DirectXのパースペクティブ行列では通常0です

	return result;
}

Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearZ, float farZ)
{
	Matrix4x4 mat = {};

	float width = right - left;
	float height = bottom - top;
	float depth = farZ - nearZ;

	mat.m[0][0] = 2.0f / width;
	mat.m[1][1] = -2.0f / height;           // Y軸反転
	mat.m[2][2] = 1.0f / depth;
	mat.m[3][0] = -(right + left) / width;
	mat.m[3][1] = (top + bottom) / height;  // Y軸反転に合わせる
	mat.m[3][2] = nearZ / depth;
	mat.m[3][3] = 1.0f;

	return mat;
}


std::wstring ConvertString(const std::string& str)
{
	if (str.empty())
	{
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0)
	{
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

std::string ConvertString(const std::wstring& str)
{
	if (str.empty())
	{
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if (sizeNeeded == 0)
	{
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
	return result;
}

void Log(std::ostream& os, const std::string& message)
{
	os << message << std::endl;
	OutputDebugStringA(message.c_str()); //出力ウィンドウに文字を出力
}



//ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
	{
		return true;
	}

	//メッセージに応じてゲーム固有の処理を行う
	switch (msg)
	{
		//ウィンドウが破棄された
	case WM_DESTROY:
		//アプリケーションを終了する
		PostQuitMessage(0);
		return 0;
	}

	//標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception)
{
	//時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリ以下に出力
	SYSTEMTIME time;
	GetSystemTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

	//processId(このexeのId)とクラッシュ(例外)の発生したthreadIdを取得
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();

	//設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{};
	minidumpInformation.ThreadId = threadId; //クラッシュしたスレッドのID
	minidumpInformation.ExceptionPointers = exception; //例外情報
	minidumpInformation.ClientPointers = TRUE; //クライアントポインタを使用する

	//Dumpの出力を行う。MiniDumpNormalは最小限の情報を出力する
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

	//ほかに関連付けられているSEH例外ハンドラがあったら実行。通常時はプロセスを終了
	return EXCEPTION_EXECUTE_HANDLER;
}

IDxcBlob* CompileShader(
	//CompilerするShaderファイルへのパス
	const std::wstring& filePath,
	//Compilerに使用するProfile
	const wchar_t* profile,
	//初期化で生成したもの3つ
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler,
	std::ofstream* logStream)
{
	//これからシェーダーをコンパイルする旨をログに出す
	Log(*logStream, ConvertString(std::format(L"Begin CompileShader, path{},profile:{}\n", filePath, profile)));
	//hlslファイルを読み込む
	IDxcBlobEncoding* shaderScore = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderScore);
	//ファイルの読み込みに失敗した場合はエラー
	assert(SUCCEEDED(hr));
	//読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderScore->GetBufferPointer();
	shaderSourceBuffer.Size = shaderScore->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;//UTF8の文字コードである事を通知する

	LPCWSTR arguments[] = {
		filePath.c_str(), //コンパイルするファイルのパス
		L"-E", L"main", //エントリーポイントの指定。基本的にmain以外にはしない
		L"-T", profile, //ShaderProfileの設定
		L"-Zi",L"Qembed_debug",
		L"-Od", //最適化を行わない
		L"-Zpr", //メモリレイアウトは行優先
	};

	//実際にShaderをコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer, //コンパイルするシェーダーの内容
		arguments, //コンパイル時の引数
		_countof(arguments), //引数の数
		includeHandler, //includeハンドラ
		IID_PPV_ARGS(&shaderResult) //結果を受け取るポインタ
	);

	//警告やエラーがあった場合はログに出力し停止する
	IDxcBlobUtf8* shaderError = nullptr;

#pragma warning(push)
#pragma warning(disable: 6387) // C6387 警告を抑制
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
#pragma warning(pop)

	if (shaderError != nullptr && shaderError->GetStringLength() != 0)
	{
		Log(*logStream, shaderError->GetStringPointer());

		//警告やエラーがあった場合は、Shaderのコンパイルに失敗したとする
		assert(SUCCEEDED(hr));
	}

	//コンパイルの結果から実行用のバイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	//Shaderのコンパイルに失敗した場合はエラー
	assert(SUCCEEDED(hr));

	//Shaderのコンパイルに成功したので、ログに出力する
	Log(*logStream, ConvertString(std::format(L"Complete CompileShader, path{},profile:{}\n", filePath, profile)));

	//リソースの解放
	shaderScore->Release(); //読み込んだファイルの解放
	shaderResult->Release(); //コンパイル結果の解放

	return shaderBlob; //コンパイルしたShaderのバイナリを返す
}

ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes)
{
	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // アップロード用のヒープ

	// 頂点リソースの設定（今回は汎用的なバッファとして設定）
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; // バッファ
	resourceDesc.Width = sizeInBytes; // 指定されたサイズ
	// バッファの場合はこれらを1にする決まり
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; // 行優先

	ID3D12Resource* bufferResource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, // データ書き込み用なのでREAD
		nullptr,
		IID_PPV_ARGS(&bufferResource));
	assert(SUCCEEDED(hr)); // 失敗したらassertで止める

	return bufferResource;
}

//DescriptorHeapの作成関数
ID3D12DescriptorHeap* CreateDescriptorHeap(
	ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
{
	ID3D12DescriptorHeap* descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	return descriptorHeap;
}

//Textireデータを読む
DirectX::ScratchImage LoadTexture(const std::string& filePath)
{
	//テクスチャファイルを読み込んでプログラムで使えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_DEFAULT_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	//ミニマップの作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
		DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	//ミニマップ付きのデータを返す
	return mipImages;
}

//TextureResourceを作る
ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata)
{
	//1. metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);//Textureの幅
	resourceDesc.Height = UINT(metadata.height);//Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);//mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);//奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format;//TextureのFormat
	resourceDesc.SampleDesc.Count = 1;//サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);//Textureの次元数。普段使っているのは2次元

	//2. 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;//細かい設定を行う
	//heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;//WriteBackポリシーでCPUアクセス可能
	//heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;//プロセッサの近くに配列

	//3. Resourceを生成する
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,//Heapの設定
		D3D12_HEAP_FLAG_NONE,//Heapの特殊な設定
		&resourceDesc,//Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST,//初回のResourceState。Textureは基本読むだけ
		nullptr,//Clear最適値。使わないのでnullptr
		IID_PPV_ARGS(&resource));//作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}

ID3D12Resource* CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height)
{
	//生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;//Textureの幅
	resourceDesc.Height = height;//textureの高さ
	resourceDesc.MipLevels = 1;//mipmapの数
	resourceDesc.DepthOrArraySize = 1;//奥行き or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//TextureのFormat
	resourceDesc.SampleDesc.Count = 1;//サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//Textureの次元数。普段使っているのは2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;//DepthStencilとして使う通知

	//2. 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;//VRAM上に作る

	//深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;//1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//フォーマット。Resourceと合わせる

	//3. Resourceを生成する
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,//Heapの設定
		D3D12_HEAP_FLAG_NONE,//Heapの特殊な設定
		&resourceDesc,//Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE,//深度値を書き込む状態にしておく
		&depthClearValue,//Clear最適値。
		IID_PPV_ARGS(&resource));//作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}

//void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages)
//{
//	//Meta情報を所得
//	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
//	//全MipMapについて
//	for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel)
//	{
//		//MipMapLevelを指定して各Imageを取得
//		const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);
//		//Textureに転送
//		HRESULT hr = texture->WriteToSubresource(
//			UINT(mipLevel),
//			nullptr,//全領域へコピー
//			img->pixels,//元データアドレス
//			UINT(img->rowPitch),//1ラインサイズ
//			UINT(img->slicePitch)//1枚サイズ
//		);
//		assert(SUCCEEDED(hr));
//	}
//}

[[nodiscard]]
ID3D12Resource* UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));
	ID3D12Resource* intermediateResource = CreateBufferResource(device, intermediateSize);
	UpdateSubresources(commandList, texture, intermediateResource, 0, 0, UINT(subresources.size()), subresources.data());
	//textureへの転送後は利用できるよう、D3D12_RESOURCE_STATE_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier(1, &barrier);
	return intermediateResource;
}

D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescroptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	CoInitializeEx(0, COINIT_MULTITHREADED);

	//誰も補足しなかった場合に(Unhandled)、補足する関数を登録
	SetUnhandledExceptionFilter(ExportDump);

	//ログファイル関係
	//ログのディレクトリを用意
	std::filesystem::create_directories("logs");

	//現在時刻を取得(UTC時刻)
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

	//ログファイルの名前にコンマ何秒はいらないため、削って秒にする
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSecound = std::chrono::time_point_cast<std::chrono::seconds>(now);

	//日本時間(PCの設定時間に変換)
	std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSecound };

	//formatを使って年月日_時分秒の形式にする
	std::string datString = std::format("{:%Y%m%d_%H%M%S}", localTime);

	//時刻を使ってファイル名を決定
	std::string logFilePath = std::string("logs/") + datString + ".log";

	//ファイルを作って書き込み準備
	std::ofstream logStream(logFilePath);



	//ウィンドウ関係
	WNDCLASS wc{};

	//ウィンドウプロシージャ
	wc.lpfnWndProc = WindowProc;

	//ウィンドウクラス名
	wc.lpszClassName = L"MyWindowClass";

	//インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);

	//カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	//ウィンドウクラスを登録する
	RegisterClass(&wc);

	//クライアント領域のサイズ
	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;

	//ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };

	//クライアント領域をもとに実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

	//ウィンドウの生成
	HWND hwnd = CreateWindow(
		wc.lpszClassName, //ウィンドウクラス名
		L"DirectX Window", //ウィンドウタイトル
		WS_OVERLAPPEDWINDOW, //ウィンドウスタイル
		CW_USEDEFAULT, CW_USEDEFAULT, //位置
		wrc.right - wrc.left, wrc.bottom - wrc.top, //サイズ
		nullptr, //親ウィンドウハンドル
		nullptr, //メニューハンドル
		wc.hInstance, //インスタンスハンドル
		nullptr //追加のパラメータ
	);

#ifdef _DEBUG
	ID3D12Debug1* debugController = nullptr;

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		//デバックレイヤーを有効化する
		debugController->EnableDebugLayer();
		//更にGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif
	//ウィンドウを表示する
	ShowWindow(hwnd, SW_SHOW);

	//DXGIファクトリーの生成
	IDXGIFactory7* dxgiFactory = nullptr;
	//HRESULTはWindoes系のエラーコード
	//関数が成功したか同課をSUCCEEDEDマクロで判断する
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

	assert(SUCCEEDED(hr));

	//使用するアダプタ用の変数。最初にnullptrを入れる
	IDXGIAdapter4* useAdapter = nullptr;

	//いい順にアダプタを頼む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i)
	{

		//アダプターの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));//ここで止まった場合一大事

		//ソフトウェアアダプタでなければ採用する
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
		{
			Log(logStream, std::format("Using adapter: {}\n", ConvertString(adapterDesc.Description)));
			break;
		}
		useAdapter = nullptr; //ソフトウェアアダプタの場合は見なかったことにするためしないのでnullptr
	}

	//アダプターが見つからなかった場合はエラー
	assert(useAdapter != nullptr);

	ID3D12Device* device = nullptr;

	//機能レベルとログの出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
	};

	const char* featureLevelNames[] = {
		"12.2",
		"12.1",
		"12.0",
	};

	//機能レベルを順に試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i)
	{
		//採用したアダプタでデバイスを作成
		hr = D3D12CreateDevice(
			useAdapter, //アダプタ
			featureLevels[i], //機能レベル
			IID_PPV_ARGS(&device) //デバイスのポインタ
		);

		//指定した昨日レベルでデバイスが生成できたか確認
		if (SUCCEEDED(hr))
		{
			//生成出来たのでログ出力を行う
			Log(logStream, std::format("Feature Level: {}\n", featureLevelNames[i]));
			break; //ループを抜ける
		}


	}

	//デバイスの生成に失敗し起動できない
	assert(device != nullptr);
	Log(logStream, std::format("Complete create D3D12Device!"));//初期起動完了のLogを出す

#ifdef _DEBUG
	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
	{
		//重大なエラーの時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);

		//エラーの時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

		//警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);



		//抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] =
		{
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		D3D12_MESSAGE_SEVERITY serverities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds); //抑制するメッセージの数
		filter.DenyList.pIDList = denyIds; //抑制するメッセージのID
		filter.DenyList.NumSeverities = _countof(serverities); //抑制するメッセージの重要度の数
		filter.DenyList.pSeverityList = serverities; //抑制するメッセージの重要度

		infoQueue->PushStorageFilter(&filter); //フィルターを適用する

		//解放
		infoQueue->Release();
	}
#endif


	//コマンドキューの生成
	ID3D12CommandQueue* commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));

	//コマンドキューの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));

	//コマンドアロケーターを生成する
	ID3D12CommandAllocator* commandAllocator = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));

	//コマンドアロケーターの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));

	//コマンドリストの生成
	ID3D12GraphicsCommandList* commandList = nullptr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList));

	//コマンドリストの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));

	//スワップチェーンを生成する
	IDXGISwapChain4* swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = kClientWidth; //ウィンドウの幅
	swapChainDesc.Height = kClientHeight; //ウィンドウの高さ
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; //色の形式
	swapChainDesc.SampleDesc.Count = 1; //マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; //レンダリングターゲットとして使用
	swapChainDesc.BufferCount = 2; //ダブルバッファリング
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; //モニターに映ったら描画を破棄

	//コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain));
	//スワップチェーンの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));

	//RTV用のヒープでディスクリプタの数は2。RTVはShader内でふれるものではないため、ShaderVisibleはfalse
	ID3D12DescriptorHeap* rtvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	//SRV用のヒープでディスクリプタの数は128。SRTはShader内で触れるものなので、ShaderVisibleはtrue
	ID3D12DescriptorHeap* srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

	//DSV用のヒープでディスクリプタの数は1。DSVはShader内で触れるものではないため、ShaderVisibleはfalse
	ID3D12DescriptorHeap* dsvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);


	//SwapChainからResorrceを取得する
	ID3D12Resource* swapChainResources[2] = { nullptr, nullptr };
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	//うまく取得できなければエラー
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	//うまく取得できなければエラー
	assert(SUCCEEDED(hr));

	//RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; //出力結果をSRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; //2Dテクスチャとして書き込む
	//ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	//RTVを2つ作るのでディスクリプタを2つ用意する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	//まず1つ目を作る。1つ目は最初のところ。こちらで場所指定する必要あり
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandles[0]);
	//2つ目は1つ目の後ろに作る
	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	//2つ目を作る
	device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandles[1]);


	MSG msg{};

	//初期値0でFenceを作る
	ID3D12Fence* fence = nullptr;
	uint64_t fenceValue = 0;
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	//フェンスの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));

	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	//フェンスイベントの生成に失敗した場合はエラー
	assert(fenceEvent != nullptr);

	//dxcCompilerを初期化
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	//現時点ではincludeしないが、includeに対応する為の設定を行う
	IDxcIncludeHandler* includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));

	//RootSignatureの作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; //入力アセンブラーでの使用を許可


	D3D12_DESCRIPTOR_RANGE descriptorRange[2] = {};

	// SRV: t3, t4
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].NumDescriptors = 2;
	descriptorRange[0].BaseShaderRegister = 3;
	descriptorRange[0].RegisterSpace = 0;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// CBV: b0, b1, b2
	descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descriptorRange[1].NumDescriptors = 3;
	descriptorRange[1].BaseShaderRegister = 2;
	descriptorRange[1].RegisterSpace = 1;
	descriptorRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


	//RootParemeter生成PuxelShaderのMaterialとVertexShaderのTransform
	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; //PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; //レジスタ番号0とバインド。b0の0と一致



	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;//VertexShaderで使える
	rootParameters[1].Descriptor.ShaderRegister = 0;//レジスタ番号0を使用

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;//PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;//Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);//Tableで管理する数

	descriptionRootSignature.pParameters = rootParameters; //ルートパラメーター配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);//配列の長さ


	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//バイアリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;//ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;//レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);


	//シリアライズしてバイナリにする
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);

	if (FAILED(hr))
	{
		Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	//バイナリをもとに生成
	ID3D12RootSignature* rootSignature = nullptr;
	hr = device->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));


	//InputLayer
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION"; //セマンティック名
	inputElementDescs[0].SemanticIndex = 0; //セマンティックインデックス
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT; //頂点のフォーマット
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs; //入力要素の配列
	inputLayoutDesc.NumElements = _countof(inputElementDescs); //入力要素の数

	//BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	//すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	//裏面(時計回り)を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//Shaderをコンパイルする
	IDxcBlob* vertexShaderBlob = CompileShader(L"Object3D.VS.hlsl",
		L"vs_6_0", dxcUtils, dxcCompiler, includeHandler, &logStream);
	assert(vertexShaderBlob != nullptr);

	IDxcBlob* pixelShaderBlob = CompileShader(L"Object3D.PS.hlsl",
		L"ps_6_0", dxcUtils, dxcCompiler, includeHandler, &logStream);
	assert(pixelShaderBlob != nullptr);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicPipelineStateDesc{};
	graphicPipelineStateDesc.pRootSignature = rootSignature; //ルートシグネチャ
	graphicPipelineStateDesc.InputLayout = inputLayoutDesc; //入力レイアウト
	graphicPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() }; //頂点シェーダーの設定
	graphicPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
		pixelShaderBlob->GetBufferSize() }; //ピクセルシェーダーの設定
	graphicPipelineStateDesc.BlendState = blendDesc; //ブレンドステートの設定
	graphicPipelineStateDesc.RasterizerState = rasterizerDesc; //ラスタライザーステートの設定
	//書き込むRTVの情報
	graphicPipelineStateDesc.NumRenderTargets = 1;
	graphicPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; //RTVのフォーマット
	//利用するトロポジ(形状)のタイプ。三角形
	graphicPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//どのように画面に色を打ち込むか設定(気にしなくていい？)
	graphicPipelineStateDesc.SampleDesc.Count = 1; //マルチサンプルしない
	graphicPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; //サンプルマスクはデフォルト

	//DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	//書き込み
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqua。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	//DepthStencilの設定
	graphicPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;



	//実際に生成
	ID3D12PipelineState* graphicPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicPipelineStateDesc,
		IID_PPV_ARGS(&graphicPipelineState));

	assert(vertexShaderBlob && "頂点シェーダーの読み込み失敗！");
	assert(pixelShaderBlob && "ピクセルシェーダーの読み込み失敗！");

	//パイプラインステートの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));

	VertexData* vertexData = nullptr;

	//球体
	const uint32_t kSubdivision = 16;//16分割
	//経度分割1つ分の角度
	const float kLonEvery = DirectX::XM_2PI / float(kSubdivision);
	// 緯度分割1つ分の角度
	const float kLatEvery = DirectX::XM_PI / float(kSubdivision);
	vertexData = new VertexData[kSubdivision * kSubdivision * 6];



	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex)
	{
		float lat = -DirectX::XM_PIDIV2 + kLatEvery * latIndex;
		float latStep = kLatEvery;

		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex)
		{
			uint32_t index = (latIndex * kSubdivision + lonIndex) * 6;
			float lon = lonIndex * kLonEvery;
			float lonStep = kLonEvery;
			//A
			vertexData[index].position.x = cos(lat) * cos(lon);
			vertexData[index].position.y = sin(lat);
			vertexData[index].position.z = cos(lat) * sin(lon);
			vertexData[index].position.w = 1.0f;
			vertexData[index].texcoord.x = float(lonIndex) / float(kSubdivision);
			vertexData[index].texcoord.y = 1.0f - float(latIndex) / float(kSubdivision);

			vertexData[index].normal.x = vertexData[index].position.x;
			vertexData[index].normal.y = vertexData[index].position.y;
			vertexData[index].normal.z = vertexData[index].position.z;

			//B
			vertexData[index + 1].position.x = cos(lat + latStep) * cos(lon);
			vertexData[index + 1].position.y = sin(lat + latStep);
			vertexData[index + 1].position.z = cos(lat + latStep) * sin(lon);
			vertexData[index + 1].position.w = 1.0f;
			vertexData[index + 1].texcoord.x = float(lonIndex) / float(kSubdivision);
			vertexData[index + 1].texcoord.y = 1.0f - float(latIndex + 1) / float(kSubdivision);

			//C
			vertexData[index + 2].position.x = cos(lat) * cos(lon + lonStep);
			vertexData[index + 2].position.y = sin(lat);
			vertexData[index + 2].position.z = cos(lat) * sin(lon + lonStep);
			vertexData[index + 2].position.w = 1.0f;
			vertexData[index + 2].texcoord.x = float(lonIndex + 1) / float(kSubdivision);
			vertexData[index + 2].texcoord.y = 1.0f - float(latIndex) / float(kSubdivision);


			//D
			vertexData[index + 5].position.x = cos(lat + latStep) * cos(lon + lonStep);
			vertexData[index + 5].position.y = sin(lat + latStep);
			vertexData[index + 5].position.z = cos(lat + latStep) * sin(lon + lonStep);
			vertexData[index + 5].position.w = 1.0f;
			vertexData[index + 5].texcoord.x = float(lonIndex + 1) / float(kSubdivision);
			vertexData[index + 5].texcoord.y = 1.0f - float(latIndex + 1) / float(kSubdivision);


			//E = B
			vertexData[index + 4].position.x = cos(lat + latStep) * cos(lon);
			vertexData[index + 4].position.y = sin(lat + latStep);
			vertexData[index + 4].position.z = cos(lat + latStep) * sin(lon);
			vertexData[index + 4].position.w = 1.0f;
			vertexData[index + 4].texcoord.x = float(lonIndex) / float(kSubdivision);
			vertexData[index + 4].texcoord.y = 1.0f - float(latIndex + 1) / float(kSubdivision);

			//F = C
			vertexData[index + 3].position.x = cos(lat) * cos(lon + lonStep);
			vertexData[index + 3].position.y = sin(lat);
			vertexData[index + 3].position.z = cos(lat) * sin(lon + lonStep);
			vertexData[index + 3].position.w = 1.0f;
			vertexData[index + 3].texcoord.x = float(lonIndex + 1) / float(kSubdivision);
			vertexData[index + 3].texcoord.y = 1.0f - float(latIndex) / float(kSubdivision);

		}
	}

	//経度のほうに分割
	//for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex)
	//{
	//	// 緯度の計算
	//	// lat と currentLat が同じ値を計算しているので、どちらか一方を使用します。
	//	// ここでは currentLat に統一します。
	//	float currentLat = -DirectX::XM_PIDIV2 + kLatEvery * latIndex;
	//	float nextLat = -DirectX::XM_PIDIV2 + kLatEvery * (latIndex + 1); // latIndex + 1 の緯度

	//	//経度の方向に分割しながら線を描く
	//	for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex)
	//	{
	//		// 経度の計算
	//		// lon と currentLon が同じ値を計算しているので、どちらか一方を使用します。
	//		// ここでは currentLon に統一します。
	//		float currentLon = lonIndex * kLonEvery;
	//		float nextLon = (lonIndex + 1) * kLonEvery; // lonIndex + 1 の経度

	//		// start インデックスは資料の通りそのまま使用
	//		uint32_t start = (latIndex * kSubdivision + lonIndex) * 6;

	//		// テクスチャ座標の正規化された値 (UとV) を事前に計算
	//		float u_current = static_cast<float>(lonIndex) / static_cast<float>(kSubdivision);
	//		float v_current = 1.0f - static_cast<float>(latIndex) / static_cast<float>(kSubdivision);
	//		float u_next = static_cast<float>(lonIndex + 1) / static_cast<float>(kSubdivision);
	//		float v_next = 1.0f - static_cast<float>(latIndex + 1) / static_cast<float>(kSubdivision);


	//		// -----------------------------------------------------------
	//		// 頂点0 (P0: 左下) 
	//		vertexData[start].position.x = cos(currentLat) * cos(currentLon);
	//		vertexData[start].position.y = sin(currentLat);
	//		vertexData[start].position.z = cos(currentLat) * sin(currentLon);
	//		vertexData[start].position.w = 1.0f;
	//		vertexData[start].texcoord.x = u_current;
	//		vertexData[start].texcoord.y = v_current;

	//		// -----------------------------------------------------------
	//		// 残りの5頂点 (P1, P2, P1_shared, P3, P2_shared)
	//		// 三角形1の2番目の頂点 (P1: 右下)
	//		vertexData[start + 1].position.x = cos(currentLat) * cos(nextLon);
	//		vertexData[start + 1].position.y = sin(currentLat);
	//		vertexData[start + 1].position.z = cos(currentLat) * sin(nextLon);
	//		vertexData[start + 1].position.w = 1.0f;
	//		vertexData[start + 1].texcoord.x = u_next;
	//		vertexData[start + 1].texcoord.y = v_current;

	//		// 三角形1の3番目の頂点 (P2: 左上)
	//		vertexData[start + 2].position.x = cos(nextLat) * cos(currentLon);
	//		vertexData[start + 2].position.y = sin(nextLat);
	//		vertexData[start + 2].position.z = cos(nextLat) * sin(currentLon);
	//		vertexData[start + 2].position.w = 1.0f;
	//		vertexData[start + 2].texcoord.x = u_current;
	//		vertexData[start + 2].texcoord.y = v_next;

	//		// 2番目の三角形の1番目の頂点 (P1: 右下) - 頂点1と共有
	//		vertexData[start + 3].position.x = cos(currentLat) * cos(nextLon);
	//		vertexData[start + 3].position.y = sin(currentLat);
	//		vertexData[start + 3].position.z = cos(currentLat) * sin(nextLon);
	//		vertexData[start + 3].position.w = 1.0f;
	//		vertexData[start + 3].texcoord.x = u_next;
	//		vertexData[start + 3].texcoord.y = v_current;

	//		// 2番目の三角形の2番目の頂点 (P3: 右上)
	//		vertexData[start + 4].position.x = cos(nextLat) * cos(nextLon);
	//		vertexData[start + 4].position.y = sin(nextLat);
	//		vertexData[start + 4].position.z = cos(nextLat) * sin(nextLon);
	//		vertexData[start + 4].position.w = 1.0f;
	//		vertexData[start + 4].texcoord.x = u_next;
	//		vertexData[start + 4].texcoord.y = v_next;

	//		// 2番目の三角形の3番目の頂点 (P2: 左上) - 頂点2と共有
	//		vertexData[start + 5].position.x = cos(nextLat) * cos(currentLon);
	//		vertexData[start + 5].position.y = sin(nextLat);
	//		vertexData[start + 5].position.z = cos(nextLat) * sin(currentLon);
	//		vertexData[start + 5].position.w = 1.0f;
	//		vertexData[start + 5].texcoord.x = u_current;
	//		vertexData[start + 5].texcoord.y = v_next;
	//	}
	//}

	// --- GPUバッファに転送 ---
	const uint32_t kVertexCount = kSubdivision * kSubdivision * 6;
	ID3D12Resource* vertexResourceSphere = CreateBufferResource(device, sizeof(VertexData) * kVertexCount);

	VertexData* mapped = nullptr;
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	memcpy(mapped, vertexData, sizeof(VertexData) * kVertexCount);
	vertexResourceSphere->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere{};
	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();
	vertexBufferViewSphere.SizeInBytes = sizeof(VertexData) * kVertexCount;
	vertexBufferViewSphere.StrideInBytes = sizeof(VertexData);

	//Sprite用の頂点Resource
	ID3D12Resource* vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 6);
	//頂点バッファビューを生成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	//リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
	//1頂点当たりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	//Sprite用
	VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));
	//一枚目の三角形
	vertexDataSprite[0].position = { 0.0f,360.0f,0.0f,1.0f };//左下
	vertexDataSprite[0].texcoord = { 0.0f,1.0f };
	vertexDataSprite[0].normal = { 0.0f,0.0f,-1.0f };
	vertexDataSprite[1].position = { 0.0f,0.0f,0.0f,1.0f };//左上
	vertexDataSprite[1].texcoord = { 0.0f,0.0f };
	vertexDataSprite[2].position = { 640.0f,360.0f,0.0f,1.0f };//右下
	vertexDataSprite[2].texcoord = { 1.0f,1.0f };

	//二枚目の三角形
	vertexDataSprite[3].position = { 0.0f,0.0f,0.0f,1.0f };//左上
	vertexDataSprite[3].texcoord = { 0.0f,0.0f };
	vertexDataSprite[4].position = { 640.0f,0.0f,0.0f,1.0f };//右上
	vertexDataSprite[4].texcoord = { 1.0f,0.0f };
	vertexDataSprite[5].position = { 640.0f,360.0f,0.0f,1.0f };//右下
	vertexDataSprite[5].texcoord = { 1.0f,1.0f };

	//ビューポート
	D3D12_VIEWPORT viewport{};
	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = kClientWidth;
	viewport.Height = kClientHeight;
	viewport.TopLeftX = 0.0f; //左上のX座標
	viewport.TopLeftY = 0.0f; //左上のY座標
	viewport.MinDepth = 0.0f; //最小の深度
	viewport.MaxDepth = 1.0f; //最大の深度

	//シザー矩形
	D3D12_RECT scissorRect{};
	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0; //左上のX座標
	scissorRect.right = kClientWidth; //右下のX座標
	scissorRect.top = 0; //左上のY座標
	scissorRect.bottom = kClientHeight; //右下のY座標


	//マテリアル用のリソースを作る
	ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Vector4));
	//マテリアルにデータを書き込む
	Vector4* materialDate = nullptr;
	//書き込む為のアドレス取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialDate));
	// データを設定（赤色 RGBA: 1,0,0,1）
	Vector4 temp{};
	temp.x = 1.0f;
	temp.y = 1.0f;
	temp.z = 1.0f;
	temp.w = 1.0f;
	*materialDate = temp;

	materialResource->Unmap(0, nullptr);

	ID3D12Resource* materialResourceSprite = CreateBufferResource(device, sizeof(Material));
	Material* materialDataSprite = nullptr;
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	materialDataSprite->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白（テクスチャ色をそのまま出す用）
	materialDataSprite->enableLighting = false;
	materialResourceSprite->Unmap(0, nullptr);


	//WVP用のリソースを作る。　Matrix4x4 1つのサイズを用意する
	ID3D12Resource* wvpResource = CreateBufferResource(device, sizeof(Matrix4x4));
	//データを書き込む
	Matrix4x4* wvpData = nullptr;
	//書き込む為のアドレス取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	//単位行列を書き込む
	*wvpData = MakeIdentity4x4();
	wvpResource->Unmap(0, nullptr);

	ID3D12Resource* transformationMatrixResourceSphere = CreateBufferResource(device, sizeof(Matrix4x4));

	// データを書き込むためのポインタを取得
	Matrix4x4* transformationMatrixDataSphere = nullptr;
	transformationMatrixResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSphere));
	*transformationMatrixDataSphere = MakeIdentity4x4();
	// 書き込みが完了したので、マップを解除
	transformationMatrixResourceSphere->Unmap(0, nullptr);

	//Sprite用のTransformationMatrix用のリソースを作る
	ID3D12Resource* transformationMatrixResourceSprite = CreateBufferResource(device, sizeof(Matrix4x4));
	//データを書き込む
	Matrix4x4* transformationMatrixDataSprite = nullptr;
	//書き込むためのアドレス取得
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
	//単位行列を書き込んでおく
	*transformationMatrixDataSprite = MakeIdentity4x4();
	transformationMatrixResourceSprite->Unmap(0, nullptr);

	//Transform変数を作る
	Transform transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	//Sprite用
	Transform transformSprite{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	//Sphere用
	Transform transformSphere{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };


	Transform cameraTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} };

	float fovY = 0.45f;  // 資料通り
	float aspectRatio = static_cast<float>(kClientWidth) / static_cast<float>(kClientHeight);
	float nearZ = 0.1f;
	float farZ = 100.0f;

	//Textureを読んで転送する
	DirectX::ScratchImage mipImages = LoadTexture("./Resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	ID3D12Resource* textureResource = CreateTextureResource(device, metadata);
	//DepthStecilTextureをウィンドウのサイズで生成
	ID3D12Resource* depthStencilResource = CreateDepthStencilTextureResource(device, kClientWidth, kClientHeight);

	//2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = LoadTexture("./Resources/monsterBall.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	ID3D12Resource* textureResource2 = CreateTextureResource(device, metadata2);

	//DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//Format。基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;//2dTexture
	//DSVHeapの先頭にDSVを作る
	device->CreateDepthStencilView(depthStencilResource, &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	/*UploadTextureData(textureResource, mipImages);*/
	ID3D12Resource* intermediateResource = UploadTextureData(textureResource, mipImages, device, commandList);
	ID3D12Resource* intermediateResource2 = UploadTextureData(textureResource2, mipImages2, device, commandList);

	//metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	//二つ目。metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	//DescriptorSizeを取得しておく
	const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	//SRVを生成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescroptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);


	//先頭はImGuiに使用しているためその次を使う
	textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//SRVの生成
	device->CreateShaderResourceView(textureResource, &srvDesc, textureSrvHandleCPU);
	//2つ目
	device->CreateShaderResourceView(textureResource2, &srvDesc2, textureSrvHandleCPU2);

	//SRVの切り替え
	bool useMonsterBall = true;


	//Imguiの初期化
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(
		device,
		swapChainDesc.BufferCount,
		rtvDesc.Format,
		srvDescriptorHeap,
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	//ウィンドウのxボタンが押されるまでループ
	while (msg.message != WM_QUIT)
	{
		//Windowに目セージが来ていたら最優先で処理される
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg); //メッセージを変換
			DispatchMessage(&msg); //メッセージをウィンドウプロシージャに送る
		}
		else
		{
			//Imguiにここからフレームが始まる趣旨をつたえる
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			//ゲームの処理
			transformSphere.rotate.y += 0.01f;
			Matrix4x4 worldMatrix = MakeAffineMatrix(transformSphere.scale, transformSphere.rotate, transformSphere.translate);
			Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
			Matrix4x4 viewMatrix = Inverse(cameraMatrix);
			Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(fovY, aspectRatio, nearZ, farZ);
			//WVPMatrixを作る
			Matrix4x4 worldViewProjectMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
			*transformationMatrixDataSphere = worldViewProjectMatrix;

			//Sprite用のworldViewProjectMatrix
			Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
			Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
			Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
			Matrix4x4 worldViewProjectionmatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));
			*transformationMatrixDataSprite = worldViewProjectionmatrixSprite;


			//開発用UIの処理、実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換え
			ImGui::ShowDemoWindow();

			ImGui::Begin("Windows");

			ImGui::Checkbox("useMonsterBall", &useMonsterBall);

			ImGui::End();

			//ImGui内部コマンドを生成する
			ImGui::Render();



			//これから書き込むバックバッファのインデックスを取得する
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

			//TransitionBarrierの設定
			D3D12_RESOURCE_BARRIER barrier{};
			//今回のバリアはTransition
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			//Noneにしておく
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			//バリアを張る対象のリソース。現在のバックバッファに対し行う
			barrier.Transition.pResource = swapChainResources[backBufferIndex];
			//遷移前(現在)のResourceState
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			//遷移後のResourceState
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			//TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);



			//描画先のRTVとDSVを設定する
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
			float clearColor[] = { 0.1f,0.25f,0.5f,1.0f };//RGBAの値。青っぽい色
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			//描画用のDescriptorHeapの設定
			ID3D12DescriptorHeap* descriptorHeap[] = { srvDescriptorHeap };
			commandList->SetDescriptorHeaps(1, descriptorHeap);




			//コマンドを積む
			commandList->RSSetViewports(1, &viewport); //ビューポートを設定
			commandList->RSSetScissorRects(1, &scissorRect); //シザー矩形を設定
			//RootSignatureを設定。PSOに設定しているけれど別途設定が必要
			commandList->SetGraphicsRootSignature(rootSignature);
			commandList->SetPipelineState(graphicPipelineState); //パイプラインステートを設定
			//Spriteの描画
			commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSphere);//VBVを設定
			//形状を設定。PSOに設定しているものとは異なるが、同じものを設定と考えれば良い
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			//マテリアルCBufferの場所を指定
			commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
			//TransformationMatrixCBufferの場所を特定
			commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSphere->GetGPUVirtualAddress());
			//SRVのDescriptorTableの先頭を設定
			commandList->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
			//描画(DrawCall/ドローコール)。3頂点で1つのインスタンス。
			commandList->DrawInstanced(1536, 1, 0, 0);


			//コマンドを積む
			commandList->RSSetViewports(1, &viewport); //ビューポートを設定
			commandList->RSSetScissorRects(1, &scissorRect); //シザー矩形を設定
			//RootSignatureを設定。PSOに設定しているけれど別途設定が必要
			commandList->SetGraphicsRootSignature(rootSignature);
			commandList->SetPipelineState(graphicPipelineState); //パイプラインステートを設定
			//Spriteの描画
			commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);//VBVを設定
			//形状を設定。PSOに設定しているものとは異なるが、同じものを設定と考えれば良い
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			//マテリアルCBufferの場所を指定
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			//TransformationMatrixCBufferの場所を特定
			commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());
			//SRVのDescriptorTableの先頭を設定
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
			//描画(DrawCall/ドローコール)。3頂点で1つのインスタンス。
			commandList->DrawInstanced(6, 1, 0, 0);

			//実際のcommandListのImGuiの描画コマンドを積む
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

			//画面に描く処理は終わり画面に映すので、状態を遷移
			//RenderTargetからPresentにする
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			//TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);



			//コマンドリストの内容を下記率させる。すべてのコマンドを積んでからCloseする
			hr = commandList->Close();
			//コマンドリストのCloseに失敗した場合はエラー
			assert(SUCCEEDED(hr));

			//GUPにコマンドリストの実行を行わせる
			ID3D12CommandList* commandLists[] = { commandList };
			commandQueue->ExecuteCommandLists(1, commandLists);
			//GUPとOSに画面の交換を要求する
			swapChain->Present(1, 0);

			//Fenceの値を更新
			fenceValue++;
			//GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
			commandQueue->Signal(fence, fenceValue);

			//Fenceの値が指定したSignalの値にたどり着いているか確認する
			//GetCompletedValueの初期値はFence作成時に渡した初期値
			if (fence->GetCompletedValue() < fenceValue)
			{
				//指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
				fence->SetEventOnCompletion(fenceValue, fenceEvent);

				//イベントを待つ
				WaitForSingleObject(fenceEvent, INFINITE);
			}


			//次フレーム用のコマンドリストを用意
			hr = commandAllocator->Reset();
			//コマンドアロケーターのリセットに失敗した場合はエラー
			assert(SUCCEEDED(hr));
			//コマンドリストをリセットする
			hr = commandList->Reset(commandAllocator, nullptr);
			//コマンドリストのリセットに失敗した場合はエラー
			assert(SUCCEEDED(hr));


		}
	}

	//ImGui終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	//COMの終了処理
	CoUninitialize();

	Log(logStream, "Application terminating.");

	std::wstring wstringValue = L"Hello, DirectX!";
	Log(logStream, ConvertString(std::format(L"WSTRING{}\n", wstringValue)));


	//出力ウィンドウへの文字出力
	OutputDebugStringA("Hello, DirextX!\n");

	//解放処理


	graphicPipelineState->Release(); //パイプラインステートの解放
	signatureBlob->Release(); //ルートシグネチャのシリアライズの解放
	if (errorBlob)
	{
		errorBlob->Release(); //エラーメッセージの解放
	}
	rootSignature->Release(); //ルートシグネチャの解放
	pixelShaderBlob->Release(); //ピクセルシェーダーの解放
	vertexShaderBlob->Release(); //頂点シェーダーの解放
	materialResource->Release();
	wvpResource->Release();


	if (dxcUtils) { dxcUtils->Release(); dxcUtils = nullptr; }
	if (dxcCompiler) { dxcCompiler->Release(); dxcCompiler = nullptr; }
	if (includeHandler) { includeHandler->Release(); includeHandler = nullptr; }

	textureResource->Release();
	textureResource2->Release();

	vertexResourceSprite->Release();
	vertexResourceSphere->Release();

	materialResourceSprite->Release();

	transformationMatrixResourceSprite->Release();
	transformationMatrixResourceSphere->Release();
	intermediateResource->Release();
	intermediateResource2->Release();

	depthStencilResource->Release();
	srvDescriptorHeap->Release();
	dsvDescriptorHeap->Release();


	CloseHandle(fenceEvent); //フェンスイベントの解放
	fence->Release(); //フェンスの解放
	rtvDescriptorHeap->Release(); //ディスクリプタヒープの解放
	swapChainResources[0]->Release(); //スワップチェーンのリソースの解放
	swapChainResources[1]->Release(); //スワップチェーンのリソースの解放
	swapChain->Release(); //スワップチェーンの解放
	commandList->Release(); //コマンドリストの解放
	commandAllocator->Release(); //コマンドアロケーターの解放
	commandQueue->Release(); //コマンドキューの解放

	useAdapter->Release(); //アダプターの解放
	dxgiFactory->Release(); //DXGIファクトリーの解放

	device->Release(); //デバイスの解放
#ifdef _DEBUG
	debugController->Release(); //デバッグコントローラーの解放
#endif

	CloseWindow(hwnd); //ウィンドウの解放


	//リソースリークチェック
	IDXGIDebug1* debug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
	{
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		debug->Release();
	}

	return 0;
}

