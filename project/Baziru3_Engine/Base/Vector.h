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