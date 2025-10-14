#pragma once
#include"Vector.h"

struct Matrix3x3
{
	float m[3][3];
};

struct Matrix4x4
{
	float m[4][4];

    // 新たに *= オペレータオーバーロード (Multiplyを使わないバージョン)
    Matrix4x4& operator*=(const Matrix4x4& other)
    {
        Matrix4x4 temp; // 計算結果を一時的に保持する行列

        // 現在の行列(*this)と引数の行列(other)を乗算します。
        // 結果は temp に格納され、その後 *this に代入されます。
        for (int i = 0; i < 4; ++i)
        { // 結果行列の行
            for (int j = 0; j < 4; ++j)
            { // 結果行列の列
                temp.m[i][j] = 0.0f; // 各要素を0で初期化
                for (int k = 0; k < 4; ++k)
                { // 積和演算
                    temp.m[i][j] += this->m[i][k] * other.m[k][j];
                }
            }
        }
        *this = temp; // 計算結果を現在のオブジェクトに代入

        return *this; // 自分自身への参照を返す
    }
};

struct TransformationMatrix
{
	Matrix4x4 WVP;
	Matrix4x4 World;

};

Matrix4x4 MakeScaleMatrix(const Vector3& scale);

Matrix4x4 MakeRotateXMatrix(float radian);

Matrix4x4 Multiply(Matrix4x4 m1, Matrix4x4 m2);

Matrix4x4 Inverse(Matrix4x4 m);

Matrix4x4 MakeIdentity4x4();

Matrix4x4 MakeRotateYMatrix(float radian);

Matrix4x4 MakeRotateZMatrix(float radian);

// 平行移動行列を作成
Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Matrix4x4& rotateMatrix, const Vector3& translate);


Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearZ, float farZ);

Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearZ, float farZ);