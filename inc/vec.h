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
};

template<typename T>
struct Vec3
{
	T x;
	T y;
	T z;
};

template<typename T>
struct Vec4
{
	T x;
	T y;
	T z;
	T w;
};

typedef Vec2<float> vec2;
typedef Vec3<float> vec3;
typedef Vec4<float> vec4;
