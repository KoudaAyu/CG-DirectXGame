#pragma once

struct Vector2
{
	float x;
	float y;

	Vector2& operator+=(const Vector2& other)
	{

		this->x += other.x;
		this->y += other.y;
		return *this;
	}


		
	

};

struct Vector3
{
	float x;
	float y;
	float z;

	Vector3& operator+=(const Vector3& other)
	{
		this->x += other.x;
		this->y += other.y;
		this->z += other.z;
		return *this;
	}
};

struct Vector4
{
	float x;
	float y;
	float z;
	float w;
};

// Vector3とfloatの乗算演算子オーバーロード
inline Vector3 operator*(const Vector3& v, float s)
{
    return { v.x * s, v.y * s, v.z * s };
}

// floatとVector3の乗算演算子オーバーロード
inline Vector3 operator*(float s, const Vector3& v)
{
    return v * s;
}

inline Vector3 operator+(const Vector3& lhs, const Vector3& rhs)
{
    return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

inline Vector3 operator-(const Vector3& lhs, const Vector3& rhs)
{
    return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}