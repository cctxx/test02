#include "UnityPrefix.h"
#include "MatrixStack.h"
#include "Runtime/Math/Matrix4x4.h"

void MatrixStack::Reset()
{
	m_Depth = 1;
	m_Matrices[0].SetIdentity();
}

Matrix4x4f& MatrixStack::GetMatrix4x4f( int index )
{
	Assert(index >= 0 && index < m_Depth);
	return m_Matrices[index];
}

void MatrixStack::Push()
{
	if( m_Depth >= kStackDepth )
	{
		ErrorString( "Matrix stack full depth reached" );
		return;
	}
	++m_Depth;
	CopyMatrix (m_Matrices[m_Depth-2].GetPtr(), m_Matrices[m_Depth-1].GetPtr());
}

void MatrixStack::Push (const float matrix[16])
{
	if( m_Depth >= kStackDepth )
	{
		ErrorString( "Matrix stack full depth reached" );
		return;
	}
	++m_Depth;
	CopyMatrix (matrix, m_Matrices[m_Depth-2].GetPtr());
}

void MatrixStack::Pop()
{
	if( m_Depth < 2 )
	{
		ErrorString( "Matrix stack empty" );
		return;
	}
	--m_Depth;
}

void MatrixStack::SetMatrix (const float matrix[16])
{
	CopyMatrix (matrix, m_Matrices[m_Depth-1].GetPtr());
}

void MatrixStack::SetCurrentIdentity()
{
	m_Matrices[m_Depth-1].SetIdentity();
}

void MatrixStack::MultMatrix( const float matrix[16] )
{
	const Matrix4x4f& a = *reinterpret_cast<const Matrix4x4f*>(matrix);
	Matrix4x4f& b = GetMatrix4x4f( m_Depth-1 );
	Matrix4x4f c;
	MultiplyMatrices4x4 (&b, &a, &c);
	CopyMatrix (c.GetPtr(), b.GetPtr());
}

const Matrix4x4f& MatrixStack::GetMatrix() const
{
	Assert(m_Depth >= 1 && m_Depth <= kStackDepth);
	return m_Matrices[m_Depth-1];
}
