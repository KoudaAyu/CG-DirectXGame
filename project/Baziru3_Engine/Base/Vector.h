#pragma once

#include <cmath>

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

	float Length() const
	{
		return std::sqrt(x * x + y * y);
	}

	float LengthSq() const
	{
		return x * x + y * y;
	}

	Vector2& Normalize()
	{
		float len = Length();
		if (len > 0.0f)
		{
			x /= len;
			y /= len;
		}
		return *this;
	}

	Vector2 Normalized() const
	{
		Vector2 v = *this;
		v.Normalize();
		return v;
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

	float Length() const
	{
		return std::sqrt(x * x + y * y + z * z);
	}

	float LengthSq() const
	{
		return x * x + y * y + z * z;
	}

	Vector3& Normalize()
	{
		float len = Length();
		if (len > 0.0f)
		{
			x /= len;
			y /= len;
			z /= len;
		}
		return *this;
	}

	Vector3 Normalized() const
	{
		Vector3 v = *this;
		v.Normalize();
		return v;
	}
};

struct Vector4
{
	float x;
	float y;
	float z;
	float w;

	float Length() const
	{
		return std::sqrt(x * x + y * y + z * z + w * w);
	}

	float LengthSq() const
	{
		return x * x + y * y + z * z + w * w;
	}

	Vector4& Normalize()
	{
		float len = Length();
		if (len > 0.0f)
		{
			x /= len;
			y /= len;
			z /= len;
			w /= len;
		}
		return *this;
	}

	Vector4 Normalized() const
	{
		Vector4 v = *this;
		v.Normalize();
		return v;
	}
};