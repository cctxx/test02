#pragma once

#include "Runtime/Math/Matrix4x4.h"


class MatrixStack
{
public:
	enum { kStackDepth = 16 };

public:
	MatrixStack() { Reset(); }

	void	Reset();

	void	SetMatrix( const float matrix[16] );
	void	SetCurrentIdentity();
	void	MultMatrix( const float matrix[16] );
	const Matrix4x4f& GetMatrix() const;

	void	Push(const float* matrix);
	void	Push();
	void	Pop();

	int		GetCurrentDepth() const { return m_Depth; }

private:
	Matrix4x4f& GetMatrix4x4f (int index);

	Matrix4x4f	m_Matrices[kStackDepth];
	int		m_Depth;
};
