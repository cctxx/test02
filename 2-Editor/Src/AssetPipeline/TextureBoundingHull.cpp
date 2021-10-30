#include "UnityPrefix.h"
#include "TextureBoundingHull.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Graphics/Texture.h"

#if UNITY_LINUX
#include <values.h>    // FLT_MAX
#endif

// Finds tight bounding hull based on alpha channel of the texture.
// Code originally from Particle Trimmer by Emil "Humus" Persson
// http://humus.name/index.php?page=News&ID=266
// http://humus.name/index.php?page=Cool&ID=8


unsigned FindOptimalRotation(Vector2f *vertices, unsigned vertex_count, unsigned *indices);

struct CHNode
{
	CHNode *Prev;
	CHNode *Next;
	Vector2f Point;
};

class SimpleConvexHull
{
public:
	SimpleConvexHull();
	~SimpleConvexHull();

	void Clear();
	bool InsertPoint(const Vector2f &point);
	bool RemoveLeastRelevantEdge();
	unsigned FindOptimalPolygon (float maxX, float maxY, Vector2f *dest, unsigned vertex_count, float *area = NULL);

	unsigned GetCount() const { return m_Count; }
	float GetArea() const;

protected:
	CHNode *m_Root;
	unsigned m_Count;
};


void ComputeTextureBoundingHull (Texture* texture, int vertexCount, Vector2f* output)
{
	const int threshold = 2; // <threshold 0-255>
	const float threshold_f = (float) threshold;

	Assert (threshold >= 0 && threshold < 256);
	Assert (vertexCount >= 3 && vertexCount <= 6);

	unsigned max_hull_size = 50;
	int sub_pixel = 16;

	unsigned indices[(6 - 2) * 3];
	bool use_index_buffer = false;

	Image img (texture->GetDataWidth(), texture->GetDataHeight(), kTexFormatAlpha8);
	texture->ExtractImage (&img, 0);

	const int w = img.GetWidth();
	const int h = img.GetHeight();
	UInt8 *pixels = img.GetRowPtr(0);


	// Set up convex hull
	SimpleConvexHull hull;
	Vector2f hullPolygon[6];
	float hArea;
	float hHullArea;
	unsigned hRotation;

	const int end_x = w - 1;
	const int end_y = h - 1;

	const float off_x = 0.5f * (w - 1);
	const float off_y = 0.5f * (h - 1);
	const float corner_off_x = 0.5f * w;
	const float corner_off_y = 0.5f * h;

	// Corner cases
	if (pixels[0] > threshold)
	{
		hull.InsertPoint(Vector2f(-corner_off_x, -corner_off_y));
	}

	if (pixels[end_x] > threshold)
	{
		hull.InsertPoint(Vector2f(corner_off_x, -corner_off_y));
	}

	if (pixels[end_y * w] > threshold)
	{
		hull.InsertPoint(Vector2f(-corner_off_x, corner_off_y));
	}

	if (pixels[end_y * w + end_x] > threshold)
	{
		hull.InsertPoint(Vector2f(corner_off_x, corner_off_y));
	}

	// Edge cases
	UInt8 *row = pixels;
	for (int x = 0; x < end_x; x++)
	{
		int c0 = row[x + 0];
		int c1 = row[x + 1];

		if ((c0 > threshold) != (c1 > threshold))
		{
			float d0 = (float) c0;
			float d1 = (float) c1;

			float sub_pixel_x = (threshold_f - d0) / (d1 - d0);
			hull.InsertPoint(Vector2f(x - off_x + sub_pixel_x, -corner_off_y));
		}
	}

	row = pixels + end_y * w;
	for (int x = 0; x < end_x; x++)
	{
		int c0 = row[x + 0];
		int c1 = row[x + 1];

		if ((c0 > threshold) != (c1 > threshold))
		{
			float d0 = (float) c0;
			float d1 = (float) c1;

			float sub_pixel_x = (threshold_f - d0) / (d1 - d0);
			hull.InsertPoint(Vector2f(x - off_x + sub_pixel_x, corner_off_y));
		}
	}

	UInt8 *col = pixels;
	for (int y = 0; y < end_y; y++)
	{
		int c0 = col[(y + 0) * w];
		int c1 = col[(y + 1) * w];

		if ((c0 > threshold) != (c1 > threshold))
		{
			float d0 = (float) c0;
			float d1 = (float) c1;

			float sub_pixel_y = (threshold_f - d0) / (d1 - d0);
			hull.InsertPoint(Vector2f(-corner_off_x, y - off_y + sub_pixel_y));
		}
	}

	col = pixels + end_x;
	for (int y = 0; y < end_y; y++)
	{
		int c0 = col[(y + 0) * w];
		int c1 = col[(y + 1) * w];

		if ((c0 > threshold) != (c1 > threshold))
		{
			float d0 = (float) c0;
			float d1 = (float) c1;

			float sub_pixel_y = (threshold_f - d0) / (d1 - d0);
			hull.InsertPoint(Vector2f(corner_off_x, y - off_y + sub_pixel_y));
		}
	}




	// The interior pixels
	for (int y = 0; y < end_y; y++)
	{
		UInt8 *row0 = pixels + (y + 0) * w;
		UInt8 *row1 = pixels + (y + 1) * w;

		for (int x = 0; x < end_x; x++)
		{
			int c00 = row0[x + 0];
			int c01 = row0[x + 1];
			int c10 = row1[x + 0];
			int c11 = row1[x + 1];

			int count = 0;
			if (c00 > threshold) ++count;
			if (c01 > threshold) ++count;
			if (c10 > threshold) ++count;
			if (c11 > threshold) ++count;

			if (count > 0 && count < 4)
			{
				float d00 = (float) c00;
				float d01 = (float) c01;
				float d10 = (float) c10;
				float d11 = (float) c11;

				for (int n = 0; n <= sub_pixel; n++)
				{
					// Lerping factors
					float f0 = float(n) / float(sub_pixel);
					float f1 = 1.0f - f0;

					float x0 = d00 * f1 + d10 * f0;
					float x1 = d01 * f1 + d11 * f0;

					if ((x0 > threshold_f) != (x1 > threshold_f))
					{
						float sub_pixel_x = (threshold_f - x0) / (x1 - x0);
						hull.InsertPoint(Vector2f(x - off_x + sub_pixel_x, y - off_y + f0));
					}

					float y0 = d00 * f1 + d01 * f0;
					float y1 = d10 * f1 + d11 * f0;

					if ((y0 > threshold_f) != (y1 > threshold_f))
					{
						float sub_pixel_y = (threshold_f - y0) / (y1 - y0);
						hull.InsertPoint(Vector2f(x - off_x + f0, y - off_y + sub_pixel_y));
					}
				}
			}

		}

	}

	if (hull.GetCount() > max_hull_size)
	{
		do
		{
			if (!hull.RemoveLeastRelevantEdge())
			{
				break;
			}
		} while (hull.GetCount() > max_hull_size);
	}

	// Do the heavy work
	const unsigned count = hull.FindOptimalPolygon (w*0.5f, h*0.5f, hullPolygon, vertexCount, &hArea);
	hHullArea = hull.GetArea();

	// Scale-bias the results
	float invW = 1.0f / w;
	float invH = 1.0f / h;
	for (unsigned i = 0; i < count; i++)
	{
		hullPolygon[i].x = hullPolygon[i].x * invW + 0.5f;
		hullPolygon[i].y = hullPolygon[i].y * invH + 0.5f;
	}

	// If fewer vertices were returned than asked for, just repeat the last vertex
	for (unsigned i = count; i < vertexCount; i++)
	{
		hullPolygon[i] = hullPolygon[count - 1];
	}

	// Optimize vertex ordering
	if (use_index_buffer)
		hRotation = FindOptimalRotation(hullPolygon, vertexCount, indices);
	else
		hRotation = 0;


	// Output the results
	Vector2f *polygon = hullPolygon;

	printf_console("\t// Area reduced to %.2f%% (optimal convex area is %.2f%%)\n", 100 * (hArea / (w * h)), 100 * (hHullArea / (w * h)));
	for (unsigned i = 0; i < vertexCount; i++)
	{
		unsigned index = (i + hRotation) % vertexCount;
		output[i] = polygon[index];
	}
}

unsigned FindOptimalRotation(Vector2f *vertices, unsigned vertex_count, unsigned *indices)
{
	const unsigned index_count = (vertex_count - 2) * 3;

	unsigned optimal = 0;
	float min_length = FLT_MAX;

	for (unsigned i = 0; i < vertex_count; i++)
	{
		float sum = 0;
		for (unsigned k = 0; k < index_count; k += 3)
		{
			unsigned i0 = (indices[k + 0] + i) % vertex_count;
			unsigned i1 = (indices[k + 1] + i) % vertex_count;
			unsigned i2 = (indices[k + 2] + i) % vertex_count;

			const Vector2f &v0 = vertices[i0];
			const Vector2f &v1 = vertices[i1];
			const Vector2f &v2 = vertices[i2];

			sum += Magnitude(v0-v1);
			sum += Magnitude(v1-v2);
			sum += Magnitude(v2-v0);
		}

		if (sum < min_length)
		{
			optimal = i;
			min_length = sum;
		}
	}

	return optimal;
}


// --------------------------------------------------------------------------
// Convex hull stuff



SimpleConvexHull::SimpleConvexHull()
{
	m_Root = NULL;
	m_Count = 0;
}

SimpleConvexHull::~SimpleConvexHull()
{
	Clear();
}

void SimpleConvexHull::Clear()
{
	if (m_Root)
	{
		CHNode *node = m_Root;
		CHNode *next;
		do
		{
			next = node->Next;

			delete node;
			node = next;
		} while (node != m_Root);

		m_Root = NULL;
		m_Count = 0;
	}
}

bool SimpleConvexHull::InsertPoint(const Vector2f &point)
{
	if (m_Count < 2)
	{
		CHNode *node = new CHNode;
		node->Point = point;

		if (m_Root == NULL)
		{
			m_Root = node;
		}
		else
		{
			node->Prev = m_Root;
			node->Next = m_Root;
		}

		m_Root->Next = node;
		m_Root->Prev = node;
		++m_Count;
		return true;
	}

	CHNode *node = m_Root;

	const Vector2f &v0 = node->Prev->Point;
	const Vector2f &v1 = node->Point;

	Vector2f dir = v1 - v0;
	Vector2f nrm(-dir.y, dir.x);

	if (Dot(point - v0, nrm) > 0)
	{
		do
		{
			node = node->Prev;
			const Vector2f &v0 = node->Prev->Point;
			const Vector2f &v1 = node->Point;

			Vector2f dir = v1 - v0;
			Vector2f nrm(-dir.y, dir.x);

			if (Dot(point - v0, nrm) <= 0)
			{
				node = node->Next;
				break;
			}

		} while (true);
	}
	else
	{
		do
		{
			const Vector2f &v0 = node->Point;
			node = node->Next;
			const Vector2f &v1 = node->Point;

			Vector2f dir = v1 - v0;
			Vector2f nrm(-dir.y, dir.x);

			if (Dot(point - v0, nrm) > 0)
				break;
			if (node == m_Root)
				return false;

		} while (true);
	}

	
	do
	{
		const Vector2f &v0 = node->Point;
		const Vector2f &v1 = node->Next->Point;

		Vector2f dir = v1 - v0;
		Vector2f nrm(-dir.y, dir.x);

		if (Dot(point - v0, nrm) <= 0)
		{
			break;
		}

		// Delete this node
		node->Prev->Next = node->Next;
		node->Next->Prev = node->Prev;

		CHNode *del = node;
		node = node->Next;
		delete del;
		--m_Count;

	} while (true);

	CHNode *new_node = new CHNode;
	new_node->Point = point;
	++m_Count;

	new_node->Prev = node->Prev;
	new_node->Next = node;

	node->Prev->Next = new_node;
	node->Prev = new_node;

	m_Root = new_node;

	return true;
}

struct SimpleLine
{
	Vector2f v;
	Vector2f d;
};

#define perp(u,v) ((u).x * (v).y - (u).y * (v).x)

bool Intersect(Vector2f &point, const SimpleLine &line0, const SimpleLine &line1)
{
	float d = perp(line0.d, line1.d);
	if (fabsf(d) < 0.000000000001f) // Parallel lines
		return false;

	float t = perp(line1.d, line0.v - line1.v) / d;

	if (t < 0.5f) // Intersects on the wrong side
		return false;

	point = line0.v + t * line0.d;
	return true;
}

bool IntersectNoParallelCheck(Vector2f &point, const SimpleLine &line0, const SimpleLine &line1)
{
	float d = perp(line0.d, line1.d);
	float t = perp(line1.d, line0.v - line1.v) / d;

	if (t < 0.5f) // Intersects on the wrong side
		return false;

	point = line0.v + t * line0.d;
	return true;
}

float AreaX2Of(const Vector2f &v0, const Vector2f &v1, const Vector2f &v2)
{
	Vector2f u = v1 - v0;
	Vector2f v = v2 - v0;

	return /*fabsf*/(u.y * v.x - u.x * v.y);
}

bool SimpleConvexHull::RemoveLeastRelevantEdge()
{
	CHNode *min_node = NULL;
	Vector2f min_pos;
	float min_area = 1e10f;


	CHNode *node = m_Root;
	do
	{
		const Vector2f &v0 = node->Prev->Point;
		const Vector2f &v1 = node->Point;
		const Vector2f &v2 = node->Next->Point;
		const Vector2f &v3 = node->Next->Next->Point;

		SimpleLine line0 = { v0, v1 - v0 };
		SimpleLine line1 = { v2, v3 - v2 };

		Vector2f v;
		if (IntersectNoParallelCheck(v, line0, line1))
		{
			float area = AreaX2Of(v1, v, v2);
			if (area < min_area)
			{
				min_node = node;
				min_pos = v;
				min_area = area;
			}
		}

		node = node->Next;
	} while (node != m_Root);

	if (min_node)
	{
		min_node->Point = min_pos;

		CHNode *del = min_node->Next;
		min_node->Next->Next->Prev = min_node;
		min_node->Next = min_node->Next->Next;

		if (del == m_Root)
			m_Root = min_node;

		delete del;

		--m_Count;

		return true;
	}

	return false;
}

unsigned SimpleConvexHull::FindOptimalPolygon (float maxX, float maxY, Vector2f *dest, unsigned vertex_count, float *area)
{
	if (vertex_count > m_Count)
		vertex_count = m_Count;

	if (vertex_count < 3)
	{
		if (area)
			*area = 0.0f;
		return 0;
	}

	if (vertex_count > 6)
		vertex_count = 6;

	// Allocate memory on stack
	SimpleLine *lines = (SimpleLine *) alloca(m_Count * sizeof(SimpleLine));
	//SimpleLine *lines = (SimpleLine *) (intptr_t(alloca(m_Count * sizeof(SimpleLine) + 64)) & ~intptr_t(63));

	CHNode *node = m_Root;

	// Precompute lines
	Vector2f bmin = node->Point;
	Vector2f bmax = node->Point;
	unsigned n = 0;
	do
	{
		bmin = min(bmin, node->Point);
		bmax = max(bmax, node->Point);
		lines[n].v = node->Point;
		lines[n].d = node->Next->Point - node->Point;

		// Move origin to center of line
		//lines[n].v += 0.5f * lines[n].d;

		node = node->Next;
		++n;
	} while (node != m_Root);

	Assert(n == m_Count);




	float min_area = 1e10f;

	Vector2f v[6];
	Vector2f &v0 = v[0];
	Vector2f &v1 = v[1];
	Vector2f &v2 = v[2];
	Vector2f &v3 = v[3];
	Vector2f &v4 = v[4];
	Vector2f &v5 = v[5];

	ANALYSIS_ASSUME(n > 0);

	#define CHECK_POINT(vv) if (vv.x < -maxX || vv.y < -maxY || vv.x >= maxX || vv.y >= maxY) continue;

	// This can probably be made a lot prettier and generic
	switch (vertex_count)
	{
	case 3:
		for (unsigned x = 0; x < n; x++)
		{
			for (unsigned y = x + 1; y < n; y++)
			{
				if (Intersect(v0, lines[x], lines[y]))
				{
					CHECK_POINT (v0);
					for (unsigned z = y + 1; z < n; z++)
					{
						if (Intersect(v1, lines[y], lines[z]))
						{
							CHECK_POINT (v1);
							if (Intersect(v2, lines[z], lines[x]))
							{
								CHECK_POINT (v2);
								Vector2f u0 = v1 - v0;
								Vector2f u1 = v2 - v0;

								float area = (u0.y * u1.x - u0.x * u1.y);

								if (area < min_area)
								{
									min_area = area;
									dest[0] = v0;
									dest[1] = v1;
									dest[2] = v2;
								}
							}
						}
					}
				}
			}
		}
		break;
	case 4:
		for (unsigned x = 0; x < n; x++)
		{
			for (unsigned y = x + 1; y < n; y++)
			{
				if (Intersect(v0, lines[x], lines[y]))
				{
					CHECK_POINT (v0);
					for (unsigned z = y + 1; z < n; z++)
					{
						if (Intersect(v1, lines[y], lines[z]))
						{
							CHECK_POINT (v1);
							for (unsigned w = z + 1; w < n; w++)
							{
								if (Intersect(v2, lines[z], lines[w]))
								{
									CHECK_POINT (v2);
									if (Intersect(v3, lines[w], lines[x]))
									{
										CHECK_POINT (v3);
										Vector2f u0 = v1 - v0;
										Vector2f u1 = v2 - v0;
										Vector2f u2 = v3 - v0;

										float area = 
											(u0.y * u1.x - u0.x * u1.y) +
											(u1.y * u2.x - u1.x * u2.y);

										if (area < min_area)
										{
											min_area = area;
											dest[0] = v0;
											dest[1] = v1;
											dest[2] = v2;
											dest[3] = v3;
										}
									}
								}
							}
						}
					}
				}
			}
		}
		break;
	case 5:
		for (unsigned x = 0; x < n; x++)
		{
			for (unsigned y = x + 1; y < n; y++)
			{
				if (Intersect(v0, lines[x], lines[y]))
				{
					for (unsigned z = y + 1; z < n; z++)
					{
						if (Intersect(v1, lines[y], lines[z]))
						{
							for (unsigned w = z + 1; w < n; w++)
							{
								if (Intersect(v2, lines[z], lines[w]))
								{
									for (unsigned r = w + 1; r < n; r++)
									{
										if (Intersect(v3, lines[w], lines[r]))
										{
											if (Intersect(v4, lines[r], lines[x]))
											{
												Vector2f u0 = v1 - v0;
												Vector2f u1 = v2 - v0;
												Vector2f u2 = v3 - v0;
												Vector2f u3 = v4 - v0;

												float area = 
													(u0.y * u1.x - u0.x * u1.y) +
													(u1.y * u2.x - u1.x * u2.y) +
													(u2.y * u3.x - u2.x * u3.y);

												if (area < min_area)
												{
													min_area = area;
													dest[0] = v0;
													dest[1] = v1;
													dest[2] = v2;
													dest[3] = v3;
													dest[4] = v4;
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		break;
	case 6:
		for (unsigned x = 0; x < n; x++)
		{
			for (unsigned y = x + 1; y < n; y++)
			{
				if (Intersect(v0, lines[x], lines[y]))
				{
					for (unsigned z = y + 1; z < n; z++)
					{
						if (Intersect(v1, lines[y], lines[z]))
						{
							for (unsigned w = z + 1; w < n; w++)
							{
								if (Intersect(v2, lines[z], lines[w]))
								{
									for (unsigned r = w + 1; r < n; r++)
									{
										if (Intersect(v3, lines[w], lines[r]))
										{
											for (unsigned s = r + 1; s < n; s++)
											{
												if (Intersect(v4, lines[r], lines[s]))
												{
													if (Intersect(v5, lines[s], lines[x]))
													{
														Vector2f u0 = v1 - v0;
														Vector2f u1 = v2 - v0;
														Vector2f u2 = v3 - v0;
														Vector2f u3 = v4 - v0;
														Vector2f u4 = v5 - v0;

														float area = 
															(u0.y * u1.x - u0.x * u1.y) +
															(u1.y * u2.x - u1.x * u2.y) +
															(u2.y * u3.x - u2.x * u3.y) +
															(u3.y * u4.x - u3.x * u4.y);

														if (area < min_area)
														{
															min_area = area;
															dest[0] = v0;
															dest[1] = v1;
															dest[2] = v2;
															dest[3] = v3;
															dest[4] = v4;
															dest[5] = v5;
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		break;
	}

	if (min_area == 1e10f && vertex_count >= 4)
	{
		min_area = (bmax.x-bmin.x) * (bmax.y-bmin.y);
		dest[0] = Vector2f (bmin.x, bmax.y);
		dest[1] = Vector2f (bmax.x, bmax.y);
		dest[2] = Vector2f (bmax.x, bmin.y);
		dest[3] = Vector2f (bmin.x, bmin.y);
		vertex_count = 4;
	}


	if (area != NULL)
	{
		*area = 0.5f * min_area;
	}

	return vertex_count;
}

float SimpleConvexHull::GetArea() const
{
	if (m_Count < 3)
		return 0.0f;

	float area = 0.0f;

	const Vector2f &v0 = m_Root->Point;

	CHNode *node = m_Root->Next;
	do
	{
		const Vector2f &v1 = node->Point;
		node = node->Next;
		const Vector2f &v2 = node->Point;

		area += AreaX2Of(v0, v1, v2);

	} while (node != m_Root);

	return 0.5f * area;
}
