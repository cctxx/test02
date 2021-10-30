#include "UnityPrefix.h"
#include "TangentSpaceCalculation.h"
#include "Runtime/Math/Matrix3x3.h"
#include "Runtime/Math/Vector2.h"
#include "Plane.h"
#include <vector>

using std::vector;
/*
void CreateTangentSpaceTangents(const Vector3f* vertex, const Vector2f* texcoord, const Vector3f* normals, Matrix3x3f* avgOrthonormalBases, int vertexCount, const UInt16* indices, int triangleCount)
{
   Vector3f *tan1 = new Vector3f[vertexCount * 2];
   Vector3f *tan2 = tan1 + vertexCount;
   memset(tan1, 0, vertexCount * sizeof(Vector3f) * 2);

   for (int a = 0; a < triangleCount; a++)
   {
      int i1 = indices[0];
      int i2 = indices[1];
      int i3 = indices[2];

      const Vector3f& v1 = vertex[i1];
      const Vector3f& v2 = vertex[i2];
      const Vector3f& v3 = vertex[i3];

      const Vector2f& w1 = texcoord[i1];
      const Vector2f& w2 = texcoord[i2];
      const Vector2f& w3 = texcoord[i3];

      float x1 = v2.x - v1.x;
      float x2 = v3.x - v1.x;
      float y1 = v2.y - v1.y;
      float y2 = v3.y - v1.y;
      float z1 = v2.z - v1.z;
      float z2 = v3.z - v1.z;

      float s1 = w2.x - w1.x;
      float s2 = w3.x - w1.x;
      float t1 = w2.y - w1.y;
      float t2 = w3.y - w1.y;

      float r = 1.0F / (s1 * t2 - s2 * t1);
      Vector3f sdir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r);
      Vector3f tdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r, (s1 * z2 - s2 * z1) * r);

      tan1[i1] += sdir;
      tan1[i2] += sdir;
      tan1[i3] += sdir;

      tan2[i1] += tdir;
      tan2[i2] += tdir;
      tan2[i3] += tdir;

      indices += 3;
   }


   for (long a = 0; a < vertexCount; a++)
   {
      Vector3f normal = normals[a];
      Vector3f tangent = tan1[a];

      
      // Gram-Schmidt orthogonalize
      (tangent - normal * (normal * tangent)));
                
      // Calculate handedness
      //tangent[a].w = (normal % tangent * tan2[a] < 0.0F) ? -1.0F : 1.0F;
     
      avgOrthonormalBases[a].SetOrthoNormalBasis (tangent, Cross (normal, tangent), normal);
   }

   delete[] tan1;
}*/

/*
void CreateTangentSpaceTangents (const Vector3f* pPositions, const Vector2f* tex, const Vector3f* normals, Matrix3x3f* avgOrthonormalBases, int vertexCount, const UInt16* indices, int faceCount)
{
	vector<Vector3f> 		sVector, tVector;
	vector<Vector3f> 		avgS, avgT;
	
	avgS.resize (vertexCount);
	avgT.resize (vertexCount);

	sVector.reserve (faceCount * 3);
	tVector.reserve (faceCount * 3);

	// for each face, calculate its S,T & SxT vector, & store its edges
	for (int f = 0; f < faceCount * 3; f += 3 )
	{
		Vector3f edge0;
		Vector3f edge1;
		Vector3f s;
		Vector3f t;
		
		// create an edge out of x, s and t
		edge0.x = pPositions[ indices[ f + 1 ] ].x - pPositions[ indices[ f ] ].x;
		edge0.y = tex[ indices[ f + 1 ] ].x - tex[ indices[ f ] ].x;
		edge0.z = tex[ indices[ f + 1 ] ].y - tex[ indices[ f ] ].y;

		// create an edge out of x, s and t
		edge1.x = pPositions[ indices[ f + 2 ] ].x - pPositions[ indices[ f ] ].x;
		edge1.y = tex[ indices[ f + 2 ] ].x - tex[ indices[ f ] ].x;
		edge1.z = tex[ indices[ f + 2 ] ].y - tex[ indices[ f ] ].y;

		Vector3f sxt = Cross (edge0, edge1);

		float a = sxt.x;
		float b = sxt.y;
		float c = sxt.z;

		float ds_dx = 0.0F;
		if ( Abs( a ) > Vector3f::epsilon )
			ds_dx =  -b / a;

		float dt_dx = 0.0F;
		if ( Abs( a ) > Vector3f::epsilon )
			dt_dx = -c / a;

		// create an edge out of y, s and t
		edge0.x = pPositions[ indices[ f + 1 ] ].y - pPositions[ indices[ f ] ].y;
		// create an edge out of y, s and t
		edge1.x = pPositions[ indices[ f + 2 ] ].y - pPositions[ indices[ f ] ].y;

		sxt = Cross (edge0, edge1);

		a = sxt.x;
		b = sxt.y;
		c = sxt.z;

		float ds_dy = 0.0F;
		if ( Abs( a ) > Vector3f::epsilon )
			ds_dy = -b / a;
	
		float dt_dy = 0.0F;
		if ( Abs( a ) > Vector3f::epsilon )
			dt_dy = -c / a;
	
		// create an edge out of z, s and t
		edge0.x = pPositions[ indices[ f + 1 ] ].z - pPositions[ indices[ f ] ].z;
		// create an edge out of z, s and t
		edge1.x = pPositions[ indices[ f + 2 ] ].z - pPositions[ indices[ f ] ].z;

		sxt = Cross (edge0, edge1);

		a = sxt.x;
		b = sxt.y;
		c = sxt.z;

		float ds_dz = 0.0F;
		if ( Abs( a ) > Vector3f::epsilon )
			ds_dz = -b / a;

		float dt_dz = 0.0F;
		if ( Abs( a ) > Vector3f::epsilon )
			dt_dz = -c / a;
	
		// generate coordinate frame from the gradients
		s = Vector3f( ds_dx, ds_dy, ds_dz );
		t = Vector3f( dt_dx, dt_dy, dt_dz );

		s = NormalizeSafe (s);
		t = NormalizeSafe (t);

		// save vectors for this face
		sVector.push_back( s );
		tVector.push_back( t );
	}


	for ( int p = 0; p < vertexCount; p ++ )
	{
		avgS[p] = Vector3f::zero;
		avgT[p] = Vector3f::zero;
	}
	
	//  go through faces and add up the bases for each vertex 
	for ( int face = 0; face < faceCount; ++face )
	{
		// sum bases, so we smooth the tangent space across edges
		avgS[ indices[ face * 3 ] ] +=   sVector[ face ];
		avgT[ indices[ face * 3 ] ] +=   tVector[ face ];

		avgS[ indices[ face * 3 + 1 ] ] +=   sVector[ face ];
		avgT[ indices[ face * 3 + 1 ] ] +=   tVector[ face ];

		avgS[ indices[ face * 3 + 2 ] ] +=   sVector[ face ];
		avgT[ indices[ face * 3 + 2 ] ] +=   tVector[ face ];
	}

	// now renormalize
	for ( int p = 0; p < vertexCount; p ++ ) 
	{
		Vector3f normal = normals[p];

		
		//OrthoNormalize (&normal, &avgS[p], &avgT[p]);
		
		avgOrthonormalBases[p].SetOrthoNormalBasis (avgS[p], avgT[p], normal);
	
		#if DEBUGMODE
		float det = avgOrthonormalBases[p].GetDeterminant ();
		AssertIf (!CompareApproximately (det, 1.0F,0.001) && !CompareApproximately (det, -1.0F,0.001));
		#endif
//		AssertIf (!CompareApproximately (avgOrthonormalBases[p].MultiplyPoint3Transpose (Vector3f (0,0,1)), normal));
	}
}*/

/*
void CreateTangentSpaceTangents (const Vector3f* pPositions, const Vector2f* tex, const Vector3f* normals, Matrix3x3f* avgOrthonormalBases, int vertexCount, const UInt16* indices, int faceCount)
{


	Vector3f v0,v1,v2, tanu, tanv;

	Vector3f p0,p1,p2;

	Vector3f d1,d2;

	float uv[3][2],det,u,v,l1,l2;

	int i,j,k;
	vector<Vector3f> 		sVector, tVector;
	vector<Vector3f> 		avgS, avgT;
	
	avgS.resize (vertexCount);
	avgT.resize (vertexCount);

	sVector.reserve (faceCount * 3);
	tVector.reserve (faceCount * 3);

	for( i=0;i<vertexCount;i++ )

	{

		sVector[i]=Vector3f(0.0, 0.0, 0.0);

		tVector[i]=Vector3f(0.0, 0.0, 0.0);

	}

	k=0;
	for( i=0;i<faceCount;i++,k+=3 )

	{

		v0=*((Vector3f *)&pPositions[indices[k]]);

		v1=*((Vector3f *)&pPositions[indices[k+1]])-v0;

		v2=*((Vector3f *)&pPositions[indices[k+2]])-v0;

		uv[0][0]=-tex[indices[k]].x;

		uv[0][1]=tex[indices[k]].y;

		uv[1][0]=-tex[indices[k+1]].x-uv[0][0];

		uv[1][1]=tex[indices[k+1]].y-uv[0][1];

		uv[2][0]=-tex[indices[k+2]].x-uv[0][0];

		uv[2][1]=tex[indices[k+2]].y-uv[0][1];

		det=(uv[1][0]*uv[2][1])-(uv[2][0]*uv[1][1]);


		
		if (fabsf(det)<0.000000001f){ 		
				continue;
		}

		u=0; v=0;

		u-=uv[0][0]; v-=uv[0][1];

		p0=v0+v1*((u*uv[2][1]-uv[2][0]*v)/det)+v2*((uv[1][0]*v-u*uv[1][1])/det);

		

		u=1; v=0;

		u-=uv[0][0]; v-=uv[0][1];

		p1=v0+v1*((u*uv[2][1]-uv[2][0]*v)/det)+v2*((uv[1][0]*v-u*uv[1][1])/det);



		u=0; v=1;

		u-=uv[0][0]; v-=uv[0][1];

		p2=v0+v1*((u*uv[2][1]-uv[2][0]*v)/det)+v2*((uv[1][0]*v-u*uv[1][1])/det);



		d1=p2-p0;

		d2=p1-p0;

		l1=Magnitude(d1);

		l2=Magnitude(d2);

		d1*=1.0f/l1;

		d2*=1.0f/l2;



		j=indices[k];

		sVector[j].x+=d1.x;	sVector[j].y+=d1.y;	sVector[j].z+=d1.z;

		tVector[j].x+=d2.x;	tVector[j].y+=d2.y;	tVector[j].z+=d2.z;



		j=indices[k+1];

		sVector[j].x+=d1.x;	sVector[j].y+=d1.y;	sVector[j].z+=d1.z;

		tVector[j].x+=d2.x;	tVector[j].y+=d2.y;	tVector[j].z+=d2.z;



		j=indices[k+2];

		sVector[j].x+=d1.x;	sVector[j].y+=d1.y;	sVector[j].z+=d1.z;

		tVector[j].x+=d2.x;	tVector[j].y+=d2.y;	tVector[j].z+=d2.z;

	}



	for( i=0;i<vertexCount;i++ )

	{

		//v0.vec(vert[i].tanu[0],vert[i].tanu[1],vert[i].tanu[2]);
		v0 = Vector3f(sVector[i].x,sVector[i].y,sVector[i].z);
		//v0.normalize();
		v0 = NormalizeRobust(v0);
		
		//v1.vec(vert[i].tanv[0],vert[i].tanv[1],vert[i].tanv[2]);
		v1 = Vector3f(tVector[i].x,tVector[i].y,tVector[i].z);
		//v1 = NormalizeRobust(v1);

		Vector3f n(normals[i].x,normals[i].y,normals[i].z);
		
		if (SqrMagnitude(v1)<0.0001f)
		{
			v1 = Cross(n,v0);
		}

		v1 = NormalizeRobust(v1);


		sVector[i].x=v0.x;

		sVector[i].y=v0.y;

		sVector[i].z=v0.z;

		tVector[i].x=v1.x;

		tVector[i].y=v1.y;

		tVector[i].z=v1.z;

		avgOrthonormalBases[i].SetOrthoNormalBasis (tVector[i], sVector[i], n);
	}

}*/

void CreateTangentSpaceTangents (const Vector3f* pPositions, const Vector2f* tex, const Vector3f* normals, Matrix3x3f* avgOrthonormalBases, int vertexCount, const int* indices, int faceCount)
{
	

	Vector3f v0,v1,v2, tanu, tanv;

	Vector3f p0,p1,p2;

	Vector3f d1,d2;

	float uv[3][2],det,u,v,l1,l2;

	int i,j,k;
	vector<Vector3f> 		sVector, tVector;
	vector<Vector3f> 		avgS, avgT;
	
	avgS.resize (vertexCount);
	avgT.resize (vertexCount);

	sVector.reserve (faceCount * 3);
	tVector.reserve (faceCount * 3);

	for( i=0;i<vertexCount;i++ )

	{

		sVector[i]=Vector3f(0.0, 0.0, 0.0);

		tVector[i]=Vector3f(0.0, 0.0, 0.0);

	}

	k=0;
	for( i=0;i<faceCount;i++,k+=3 )

	{

		v0=*((Vector3f *)&pPositions[indices[k]]);

		v1=*((Vector3f *)&pPositions[indices[k+1]])-v0;

		v2=*((Vector3f *)&pPositions[indices[k+2]])-v0;

		uv[0][0]=tex[indices[k]].x;

		uv[0][1]=tex[indices[k]].y;

		uv[1][0]=tex[indices[k+1]].x-uv[0][0];

		uv[1][1]=tex[indices[k+1]].y-uv[0][1];

		uv[2][0]=tex[indices[k+2]].x-uv[0][0];

		uv[2][1]=tex[indices[k+2]].y-uv[0][1];

		det=(uv[1][0]*uv[2][1])-(uv[2][0]*uv[1][1]);


		
		if (fabsf(det)<0.000001f){ 
			continue;
		}

		u=0; v=0;

		u-=uv[0][0]; v-=uv[0][1];

		p0=v0+v1*((u*uv[2][1]-uv[2][0]*v)/det)+v2*((uv[1][0]*v-u*uv[1][1])/det);

		

		u=1; v=0;

		u-=uv[0][0]; v-=uv[0][1];

		p1=v0+v1*((u*uv[2][1]-uv[2][0]*v)/det)+v2*((uv[1][0]*v-u*uv[1][1])/det);



		u=0; v=1;

		u-=uv[0][0]; v-=uv[0][1];

		p2=v0+v1*((u*uv[2][1]-uv[2][0]*v)/det)+v2*((uv[1][0]*v-u*uv[1][1])/det);



		d1=p2-p0;

		d2=p1-p0;

		l1=Magnitude(d1);

		l2=Magnitude(d2);

		d1*=1.0f/l1;

		d2*=1.0f/l2;



		j=indices[k];

		sVector[j] += d1;

		tVector[j].x+=d2.x;	tVector[j].y+=d2.y;	tVector[j].z+=d2.z;



		j=indices[k+1];

		sVector[j].x+=d1.x;	sVector[j].y+=d1.y;	sVector[j].z+=d1.z;

		tVector[j].x+=d2.x;	tVector[j].y+=d2.y;	tVector[j].z+=d2.z;



		j=indices[k+2];

		sVector[j].x+=d1.x;	sVector[j].y+=d1.y;	sVector[j].z+=d1.z;

		tVector[j].x+=d2.x;	tVector[j].y+=d2.y;	tVector[j].z+=d2.z;

	}



	for( i=0;i<vertexCount;i++ )

	{

		v0 = Vector3f(sVector[i].x,sVector[i].y,sVector[i].z);	
		v0 = NormalizeRobust(v0);
		
		v1 = Vector3f(tVector[i].x,tVector[i].y,tVector[i].z);

		Vector3f n(normals[i].x,normals[i].y,normals[i].z);
		
		if (SqrMagnitude(v1)<0.0001f)
		{
			v1 = Cross(v0,n);
		}	
		v1 = NormalizeRobust(v1);
			
	
		sVector[i]=v0;

		tVector[i].x=v1.x;

		tVector[i].y=v1.y;

		tVector[i].z=v1.z;

		avgOrthonormalBases[i].SetOrthoNormalBasis (tVector[i], sVector[i], n);
	}

}

