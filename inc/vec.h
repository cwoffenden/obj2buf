/**
 * \file vec.h
 * Very basic 2-, 3- and 4-component vectors.
 */
#pragma once

template<typename T>
struct Vec2
{
	T x;
	T y;
	operator T* () {
		return &x;
	}
};

template<typename T>
struct Vec3
{
	T x;
	T y;
	T z;
	operator T* () {
		return &x;
	}
};

template<typename T>
struct Vec4
{
	T x;
	T y;
	T z;
	T w;
	operator T* () {
		return &x;
	}
};

typedef Vec2<float> vec2;
typedef Vec3<float> vec3;
typedef Vec4<float> vec4;
