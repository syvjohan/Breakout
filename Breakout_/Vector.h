#pragma once

#include <cmath>

struct Vec2
{
	Vec2()
	{
		x = 0.0f;
		y = 0.0f;
	}

	Vec2(float x, float y)
	{
		this->x = x;
		this->y = y;
	}

	void operator=(const Vec2 &v)
	{
		x = v.x;
		y = v.y;
	}

	void operator+=(const Vec2 &v)
	{
		x += v.x;
		y += v.y;
	}

	Vec2 operator+(const Vec2 &v)
	{
		return Vec2(v.x + x, v.y + y);
	}

	void operator-=(const Vec2 &v)
	{
		x -= v.x;
		y -= v.y;
	}

	Vec2 operator-(const Vec2 &v)
	{
		return Vec2(x - v.x, y - v.y);
	}

	//Scalars
	Vec2 operator*(float val)
	{
		return Vec2(x * val, y * val);
	}

	//Scalars
	void operator*=(float val)
	{
		x *= val;
		y *= val;
	}

	Vec2 operator/(float val)
	{
		return Vec2(x * (1.0 / val), y * (1.0f / val));
	}

	void operator/=(Vec2 &v)
	{
		x = x * (1.0f / v.x);
		y = y * (1.0f / v.y);
	}

	void Normalize()
	{
		float length = sqrtf(x * x + y * y);
		x /= length;
		y /= length;
	}

	float DotProduct(const Vec2 &v) const
	{
		return ((x * v.x) + (y * v.y));
	}

	//Magnitude
	float Length()
	{
		return sqrtf( sqrtf(x) + sqrtf(y) );
	}

	//Round and convert to int.
	int ToIntX()
	{
		int val;
		x > 0 ? val = (int)(x + 0.5f) : val = (int)(x - 0.5f);
		return val;
	}

	//Round and convert to int.
	int ToIntY()
	{
		int val;
		y > 0 ? val = (int)(y + 0.5f) : val = (int)(y - 0.5f);
		return val;
	}

	float x;
	float y;
};

