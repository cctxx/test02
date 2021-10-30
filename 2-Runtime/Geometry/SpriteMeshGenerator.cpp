#include "UnityPrefix.h"
#include "SpriteMeshGenerator.h"

#if ENABLE_SPRITES
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Graphics/SpriteUtility.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/FloatConversion.h"
#include "Runtime/Math/Polynomials.h"

#include "External/libtess2/libtess2/tesselator.h"
#include <queue>

PROFILER_INFORMATION (gProfileDecompose, "SpriteMeshGenerator.Decompose", kProfilerRender);
PROFILER_INFORMATION (gProfileTraceShape, "SpriteMeshGenerator.TraceShape", kProfilerRender);
PROFILER_INFORMATION (gProfileSimplify, "SpriteMeshGenerator.Simplify", kProfilerRender);

static const float kHoleAreaLimit = 0.25f;
static const float kMaxTriangles  = 1000.0f;
static const float kMaxOverdraw   = 4.0f;
static const float kResolution    = 960*640*kMaxOverdraw;

struct edge {
    float m_a;
    float m_b;
    float m_c;
    bool  m_apos;
    bool  m_bpos;
    edge(){};
    edge(Vector2f p0, Vector2f p1) {
        m_a = (p0.y - p1.y);
        m_b = (p1.x - p0.x);
        m_c = -p0.x*m_a - p0.y*m_b;
        
        bool aez = (m_a == 0);
        bool bez = (m_b == 0);
        bool agz = (m_a <  0);
        bool bgz = (m_b <  0);
        m_apos = aez ? bgz : agz;
        m_bpos = bez ? agz : bgz;
    }
    
    float grad(Vector2f p) { return (m_a*p.x + m_b*p.y + m_c); }
    int   test(Vector2f p) {
        float g = grad(p);
        return ((g > 0) || ((g == 0) && m_apos)) ? 1 : -1;
    }
};

static inline int mod(int a, int n)
{
    return a>=n ? a%n : a>=0 ? a : n-1-(-1-a)%n;
}


inline float det(const Vector2f& a, const Vector2f& b, const Vector2f& c)
{
    float bax = b.x - a.x;
    float acx = a.x - c.x;
    float aby = a.y - b.y;
    float cay = c.y - a.y;
    return (bax * cay) - ( acx * aby );
}

inline Vector2f ortho(const Vector2f& v)
{
    return Vector2f(v.y, -v.x);
}

inline float distance_point_line(Vector2f pq, Vector2f p0, Vector2f p1)
{
	Vector2f v = p1 - p0;
	Vector2f w = pq - p0;
	
	float a = Dot(w, v);
	if (a <= 0)
		return Magnitude(p0-pq);
	
	float b = Dot(v, v);
	if (b <= a)
		return Magnitude(p1-pq);
	
	float c = a/b;
	Vector2f  p = p0 + v*c;
	return Magnitude(p-pq);
}

void SpriteMeshGenerator::Decompose(std::vector<Vector2f>* vertices, std::vector<int>* indices)
{
	if (m_paths.size() == 0)
		return;
	
	vertices->clear();
	indices->clear();

	const int kVertexSize = 2;
	const int kPolygonVertices = 3;
	
	PROFILER_BEGIN(gProfileDecompose, NULL);
	TESStesselator* tess = tessNewTess(NULL);
	for (std::vector<path>::const_iterator it = m_paths.begin(); it != m_paths.end(); ++it)
	{
		const path& p = *it;
		const std::vector<vertex>& vertices = p.m_path;
		if (vertices.size() == 0)
			continue;
		
		tessAddContour(tess, kVertexSize, &vertices[0].p, sizeof(vertex), vertices.size());
	}
	int tessError = tessTesselate(tess, TESS_WINDING_NONZERO, TESS_POLYGONS, kPolygonVertices, kVertexSize, NULL);
	AssertBreak(tessError == 1);

	const int elemCount = tessGetElementCount(tess);
	const TESSindex* elements = tessGetElements(tess);
	const TESSreal* real = tessGetVertices(tess);

	for (int e = 0; e < elemCount; ++e)
	{
		const int* idx = &elements[e * kPolygonVertices];
				
		// Extract vertices
		for (int i = 0; i < kPolygonVertices; ++i)
		{
			Assert(idx[i] != TESS_UNDEF);

			float x = real[idx[i]*kVertexSize];
			float y = real[idx[i]*kVertexSize + 1];

#define SNAP_VERTEX_POSITION 1
#if SNAP_VERTEX_POSITION
			x = floor(x + 0.5f);
			y = floor(y + 0.5f);
#endif
#undef SNAP_VERTEX_POSITION

			Vector2f newVertex(x, y);

			// Reuse vertex
			bool reused = false;
			for (int v = 0; v < vertices->size(); ++v)
			{
				const Vector2f& vertex = (*vertices)[v];
				if ((std::abs(vertex.x - x) <= Vector2f::epsilon) && (std::abs(vertex.y - y) <= Vector2f::epsilon))
				{
					indices->push_back(v);
					reused = true;
					break;
				}
			}

			// New vertex
			if (!reused)
			{
				indices->push_back(vertices->size());
				vertices->push_back(newVertex);
			}
		}
    
		// Push polygon
	}
#ifdef __MESHGEN_STATS
	{
		int n=(int)m_paths.size();
		
		double mesh_area = 0.0;
		double rect_area = 0.0;
		for(int i=0; i<n; i++) {
			path *p = &m_paths[i];
			int m = p->m_path.size();
			for(int j=0; j<m; j++) {
				Vector2f p0 = p->m_path[j].p;
				Vector2f p1 = p->m_path[mod(j+1, m)].p;
				mesh_area += (p0.x*p1.y)-(p0.y*p1.x);
			}
		}
		mesh_area *= 0.5;
		rect_area = m_mask_org.w*m_mask_org.h;
		
		LogString(Format("Sprite mesh triangle count     : %d",  ((indices!=NULL) ? (int)(indices->size()/3) : 0)));
		LogString(Format("Sprite mesh area (image space) : %.0f", mesh_area));
		LogString(Format("Sprite rect area (image space) : %.0f", rect_area));
		LogString(Format("Sprite diff area (image space) : %.0f", rect_area-mesh_area));
		
	}
#endif

	tessDeleteTess(tess);
	PROFILER_END
}

float SpriteMeshGenerator::evaluateLOD(const float areaHint, float area)
{
	// do rough estimation of simplification lod
	int triangleCount=0;
	int n = (int)m_paths.size();
	
	// evaluate optimal triangle count
	for(int i=0; i<n; i++) {
        path *p = &m_paths[i];
		if (p->isHole())
			triangleCount += 2;
		else
			triangleCount += (int)p->m_path.size() - 2;
	}
	float maxTriangleCount = area * areaHint;
	float lod = 1.0f-(maxTriangleCount / (float)triangleCount);
	return clamp(lod, 0.0f, 1.0f);
	
}

void SpriteMeshGenerator::MakeShape(ColorRGBA32* image, int imageWidth, int imageHeight, float hullTolerance, unsigned char alphaTolerance, bool holeDetection, unsigned int extrude, float bias, int mode)
{
	PROFILER_BEGIN(gProfileTraceShape, NULL);
	m_mask_org = mask(image, imageWidth, imageHeight, alphaTolerance, extrude);
    m_mask_cur = mask(image, imageWidth, imageHeight, alphaTolerance, extrude);
    
   	std::vector<vertex> outline;

	int   sign;
    float area;
	float areaRect  = imageWidth*imageHeight;
	float areaTotal = 0;
    while(contour(outline, sign, area)) {
        if (!holeDetection && sign=='-')
            continue;
		if (area<(areaRect*kHoleAreaLimit) && sign=='-' && hullTolerance < 0.0f)
			continue;
		areaTotal += (sign=='+') ? area : -area;
        m_paths.push_back(path(outline, imageWidth, imageHeight, sign, area, bias));
    }
	PROFILER_END
	
	PROFILER_BEGIN(gProfileSimplify, NULL);
	if (hullTolerance < 0.0f) {
		hullTolerance = evaluateLOD(kMaxTriangles/kResolution, areaTotal);
	}
	// Simplify
	for (std::vector<path>::iterator it = m_paths.begin(); it != m_paths.end(); ++it)
	{
		path& p = *it;
		p.simplify(hullTolerance, mode);
	}
	// Snap
	for (std::vector<path>::iterator it = m_paths.begin(); it != m_paths.end(); ++it)
	{
		path& p = *it;
		for (std::vector<vertex>::iterator vit = p.m_path.begin(); vit != p.m_path.end(); ++vit)
		{
			vertex& vert = *vit;
			vert.p.x = Roundf(vert.p.x);
			vert.p.y = Roundf(vert.p.y);
		}
	}
	PROFILER_END
}

inline bool predMinX(const SpriteMeshGenerator::path& a, const SpriteMeshGenerator::path& b) { return a.GetMin().x < b.GetMin().x; }
inline bool predMinY(const SpriteMeshGenerator::path& a, const SpriteMeshGenerator::path& b) { return a.GetMin().y < b.GetMin().y; }
inline bool predMaxX(const SpriteMeshGenerator::path& a, const SpriteMeshGenerator::path& b) { return a.GetMax().x < b.GetMax().x; }
inline bool predMaxY(const SpriteMeshGenerator::path& a, const SpriteMeshGenerator::path& b) { return a.GetMax().y < b.GetMax().y; }

bool SpriteMeshGenerator::FindBounds(Rectf& bounds)
{
	if (m_paths.size() == 0)
		return false;

	const SpriteMeshGenerator::path& minX = *std::min_element(m_paths.begin(), m_paths.end(), predMinX);
	const SpriteMeshGenerator::path& minY = *std::min_element(m_paths.begin(), m_paths.end(), predMinY);
	const SpriteMeshGenerator::path& maxX = *std::max_element(m_paths.begin(), m_paths.end(), predMaxX);
	const SpriteMeshGenerator::path& maxY = *std::max_element(m_paths.begin(), m_paths.end(), predMaxY);

	bounds.x = minX.GetMin().x;
	bounds.SetRight(maxX.GetMax().x);

	bounds.y = minY.GetMin().y;
	bounds.SetBottom(maxY.GetMax().y);
	
	return true;
}

void SpriteMeshGenerator::path::bbox()
{
    float minx=(std::numeric_limits<float>::max)();
    float miny=(std::numeric_limits<float>::max)();
    float maxx=(std::numeric_limits<float>::min)();
    float maxy=(std::numeric_limits<float>::min)();
    
    int n=(int)m_path.size();
    for(int i=0; i<n; i++) {
        Vector2f p=m_path[i].p;
        if (p.x < minx) minx = p.x;
        if (p.y < miny) miny = p.y;
        if (p.x > maxx) maxx = p.x;
        if (p.y > maxy) maxy = p.y;
    }
	
	// clamp to bounds
	minx = (minx < 0.0) ? 0.0 : (minx > m_bx) ? m_bx : minx;
	miny = (miny < 0.0) ? 0.0 : (miny > m_by) ? m_by : miny;
	maxx = (maxx < 0.0) ? 0.0 : (maxx > m_bx) ? m_bx : maxx;
	maxy = (maxy < 0.0) ? 0.0 : (maxy > m_by) ? m_by : maxy;
	
    m_min = Vector2f(minx, miny);
    m_max = Vector2f(maxx, maxy);
}

bool SpriteMeshGenerator::path::dec(int i)
{
    int  n = (int)m_path.size();
    if (n<3)
        return false;
    
    Vector2f a  = m_path[mod(i-1, n)].p;
    Vector2f b  = m_path[mod(i+0, n)].p;
    Vector2f c  = m_path[mod(i+1, n)].p;
    Vector2f ab = a-b;
    Vector2f bc = b-c;
    Vector2f na = NormalizeSafe(Vector2f(-ab.y, ab.x));
    Vector2f nb = NormalizeSafe(Vector2f(-bc.y, bc.x));
    Vector2f no = NormalizeSafe(nb+na);
    m_path[mod(i, n)].n = no;
    return true;
}
 
bool SpriteMeshGenerator::path::inf(int i)
{
    int  n = (int)m_path.size();
    if (n<3)
        return false;
    Vector2f a  = m_path[mod(i-1, n)].p;
    Vector2f b  = m_path[mod(i+0, n)].p;
    Vector2f c  = m_path[mod(i+1, n)].p;
    m_path[i].s = edge(a,c).test(b);
    return true;
}

static int dir(Vector2f p0, Vector2f p1)
{
    int di[3*3] = {
        0,  1, 2,
        7, -1, 3,
        6,  5, 4
    };
    Vector2f dt = p0 - p1;
    int dx = (dt.x > 0.0f) ? 1 : (dt.x < 0.0f) ? -1 : 0;
    int dy = (dt.y > 0.0f) ? 1 : (dt.y < 0.0f) ? -1 : 0;
    int d  = 4 + 3*dx - dy;
    return (d>=0 || d<=8) ? di[d] : -1;
}

static bool min_positive(float a, float b, float& res)
{
    if(a > 0 && b > 0)
        res = a < b ? a : b;
    else
        res = a > b ? a : b;
    return ((res > 0) || CompareFloatRobust(res, 0.0));
}

#define LE 0x1
#define RE 0x2
#define BE 0x4
#define TE 0x8

bool SpriteMeshGenerator::path::clip_test(Vector2f p, int side)
{
    switch( side ) {
        case LE: return p.x >= m_min.x;
        case RE: return p.x <= m_max.x;
        case TE: return p.y >= m_min.y;
        case BE: return p.y <= m_max.y;
    }
    return false;
}

Vector2f SpriteMeshGenerator::path::clip_isec(Vector2f p, Vector2f q, int e)
{
    double a = (q.y - p.y) / (q.x - p.x);
    double b = p.y - p.x * a;
    double x, y;
    switch(e) {
        case LE:
        case RE:
            x = (e == LE) ? m_min.x : m_max.x;
            y = x * a + b;
            break;
        case TE:
        case BE:
            y = (e == TE) ? m_min.y : m_max.y;
            x = (IsFinite(a)) ? (y - b) / a : p.x;
            break;
    }
    return Vector2f(x,y);
}

void SpriteMeshGenerator::path::clip_edge(int e)
{
    int n=(int)m_path.size();
    std::vector<vertex> cpath;
    
    for (int i=0 ; i<n; i++) {
        Vector2f s = m_path[mod(i+0, n)].p;
        Vector2f p = m_path[mod(i+1, n)].p;
        Vector2f c;
        if (clip_test(p, e)) {
            if (!clip_test(s, e) ) {
                c = clip_isec(p, s, e);
                cpath.push_back(vertex(c));
            }
            cpath.push_back(vertex(p));
        }
        else
            if (clip_test(s, e)) {
                c = clip_isec(s, p, e);
                cpath.push_back(vertex(c));
            }
    }
    m_path.clear();
    m_path=cpath;
}

void SpriteMeshGenerator::path::clip()
{
    clip_edge(LE);
    clip_edge(RE);
    clip_edge(TE);
    clip_edge(BE);
}

static bool lseg_intersect(Vector2f p1, Vector2f p2, Vector2f p3, Vector2f p4)
{
    Vector2f e43 = p4-p3;
    Vector2f e21 = p2-p1;
    Vector2f e13 = p1-p3;
    
    double dn = e43.y*e21.x - e43.x*e21.y;
    double na = e43.x*e13.y - e43.y*e13.x;
    double nb = e21.x*e13.y - e21.y*e13.x;
    
    // coincident?
    if ((fabs(na) < Vector2f::epsilon) &&
        (fabs(nb) < Vector2f::epsilon) &&
        (fabs(dn) < Vector2f::epsilon)) {
        return false;
    }
    
    // parallel ?
    if (fabs(dn) < Vector2f::epsilon)
        return false;
    
    // collinear ?
    double mua = na / dn;
    double mub = nb / dn;
    if ((mua < 0) || (mua > 1) || (mub < 0) || (mub > 1))
        return false;
    
    return true;
}

int SpriteMeshGenerator::path::self_intersect(Vector2f p0, Vector2f p1)
{
    int n=(int)m_path.size();
    for(int i=0; i<n; i++) {
        Vector2f p2 = m_path[mod(i+0, n)].p;
        Vector2f p3 = m_path[mod(i+1, n)].p;
        if ((p2==p0) ||
            (p3==p1) ||
            (p1==p2) ||
            (p0==p3))
            continue;
        if (lseg_intersect(p0, p1, p2, p3))
            return 1;
    }
    return 0;
}

bool SpriteMeshGenerator::path::cvx_cost(int i)
{
    int n = (int)m_path.size();
    if (n<5)
        return false;
	
    vertex   *v = &m_path[i];
    Vector2f sn = m_path[mod(i-1,n)].n;
    Vector2f tn = m_path[mod(i+1,n)].n;
	// detect high convexity -> better triangulation for cyclic geometric shape
	float q = Dot(sn,tn);
	if ((q < 0.000) ||
		CompareFloatRobust(q, 0.0) ||
		CompareFloatRobust(q, 1.0) ) {
        v->cost=s_cost(-1, 0.0);
		return true;
	}
	
	Vector2f s0 = m_path[mod(i-1,n)].p;
    Vector2f t0 = m_path[mod(i+1,n)].p;
    Vector2f p0 = v->p;
    Vector2f a0 = ortho(sn);
    Vector2f c0 = ortho(p0-s0);
    Vector2f c1 = s0-t0;
    Vector2f b0 = tn-sn;
    float c = Dot(c0, c1);
    float a = Dot(tn, a0);
    float b = Dot(b0, c0) + Dot(c1, a0);
	
    float x0=-1,x1=-1,w,d;
    if (QuadraticPolynomialRootsGeneric(a, -b, c, x0, x1) && min_positive(x0, x1, w)) {
        Vector2f r0 = m_path[mod(i-2, n)].p;
        Vector2f u0 = m_path[mod(i+2, n)].p;
        Vector2f s1 = s0 + sn*w;
        Vector2f t1 = t0 + tn*w;
        
        float d0 = det(r0, s1, s0);
        float d1 = det(s1, p0, s0);
        float d2 = det(p0, t1, t0);
        float d3 = det(t1, u0, t0);
        float d  = d0+d1+d2+d3;
        v->cost  = s_cost(d, w);
        return true;
    }
    else {
		// this should not ever happen.. but handle it anyway
        d = -1.0;
        w =  0.0;
        v->cost = s_cost(d, w);
        return false;
    }
}

bool SpriteMeshGenerator::path::cve_cost(int i)
{
    int n=(int)m_path.size();
    if (n<3)
        return 0;
    vertex  *v = &m_path[i];
    Vector2f s = m_path[mod(i-1,n)].p;
    Vector2f t = m_path[mod(i+1,n)].p;
    Vector2f u = v->p;
    float d = det(s,t,u);
    if ((d>0) || CompareFloatRobust(d, 0.0)) {
        v->cost = s_cost(d, 0.0);
        return true;
    }
    else {
        v->cost = s_cost(-1.0, 0.0);
        return false;
    }
}

int SpriteMeshGenerator::path::min_cost()
{
    int   n     = (int)m_path.size();
    int   min_i = -1;
    float min_c = (std::numeric_limits<float>::max)();
    for (int i=0; i<n; i++) {
        vertex v = m_path[i];
        if (v.cost.c < 0)
            continue;
        float c = v.cost.c + v.c;
        if (c < min_c) {
            min_c = c;
            min_i = i;
        }
    }
    return min_i;
}

bool SpriteMeshGenerator::path::select()
{
    int n = (int)m_path.size();    
    if (n<5)
        return false;
    
	int i = 0;
    int m = 0;
	bool found = false;
    do {
        i = min_cost();
        if (i<0)
            return false;
        struct vertex *v = &m_path[i];
        struct vertex *s = &m_path[mod(i-1, n)];
        struct vertex *t = &m_path[mod(i+1, n)];
        struct s_cost cost = v->cost;
        int isec = 0;
        if (v->s > 0) {
            Vector2f s0 = s->p;
            Vector2f t0 = t->p;
			 // check self intersection
            isec = self_intersect(s0, t0);
			
            if (isec) v->cost.c = -1;
            else {
				s->c += cost.c;
            	t->c += cost.c;
				m_invalid.push_back(mod(i+0, n));
				m_invalid.push_back(mod(i+1, n));
				found = true;
			}
        }
        else {
            struct vertex *r = &m_path[mod(i-2, n)];
            struct vertex *u = &m_path[mod(i+2, n)];
            Vector2f r0 = r->p;
            Vector2f u0 = u->p;
            Vector2f s0 = s->p + s->n*cost.w;
            Vector2f t0 = t->p + t->n*cost.w;
			 // check self intersection
            isec |= self_intersect(r0, s0);
            isec |= self_intersect(s0, t0);
            isec |= self_intersect(t0, u0);
        
            if (isec) v->cost.c = -1;
            else {
				s->p  = s0;
				t->p  = t0;
				s->c += cost.c;
				t->c += cost.c;
				m_invalid.push_back(mod(i-2, n));
				m_invalid.push_back(mod(i-1, n));
				m_invalid.push_back(mod(i+1, n));
				m_invalid.push_back(mod(i+2, n));
				found = true;
			}
        }
    }while((m++ < n) && !found);
	
	if (found) {
		m_path.erase(m_path.begin()+i);
		// fix invalid indices after erase
		m = (int)m_invalid.size();
        for (int k=0; k<m; k++) {
            if (m_invalid[k] > i)
                m_invalid[k] = m_invalid[k]-1;
        }
	}
    return found;
}

int SpriteMeshGenerator::path::find_max_distance(int i0)
{
	int n = m_path.size();
	
	Vector2f a = m_path[i0].p;
	float dm = -1;
	int   mi = -1;
	
	for (int i=0; i<n; i++) {
		Vector2f b  = m_path[mod(i, n)].p;
		float    ba = Magnitude(b-a);
		if (ba < dm)
			continue;
		dm = ba;
		mi = i;
	}
	return mi;
}

int SpriteMeshGenerator::path_segment::max_distance(std::vector<vertex> path, int i0, int i1)
{
	int n = path.size();
	
	Vector2f a = path[i0].p;
	Vector2f b = path[i1].p;
	
	float dm = -1;
	int   mi = -1;
	m_cnt=0;
	for (int i=i0; i != i1; i=mod(++i, n), m_cnt++) {
		float dq = distance_point_line(path[i].p, a, b);
		if (dq < dm)
			continue;
		dm = dq;
		mi = i;
	}
	return mi;
}

void SpriteMeshGenerator::path::simplify(float q, int mode)
{
    m_path.clear();
    m_path = m_path0;
    int m;
    int n   = (int)m_path.size();
    int lim = (float)n*(1.0f - clamp(q, 0.0f, 1.0f));
	
    if (n <  5) goto bail_out;
	
	if (mode==kPathEmbed) {
		if (lim < 5) lim=5;

		// mark all vertices invalid
		for (int i=0; i<n; i++)
			m_invalid.push_back(i);
    
		do {
			n = (int)m_path   .size();
			m = (int)m_invalid.size();
			for (int i=0; i<m; i++) {
				dec(m_invalid[i]);
				inf(m_invalid[i]);
			}
			for (int i=0; i<m; i++) {
				int k = m_invalid[i];
				if (m_path[k].s > 0)
					cve_cost(k);
				else
					cvx_cost(k);
			}
			m_invalid.clear();
			if (select() == false)
				break;
		}while(n > lim);
	}
	else {
		if (lim < 4) lim=4;
		
		int i0 = find_max_distance( 0 );
		int i1 = find_max_distance(i0 );
		
		path_segment ls = path_segment(m_path, i0, i1);
		path_segment rs = path_segment(m_path, i1, i0);
		
		std::priority_queue<path_segment, std::vector<path_segment>, compare_path_segment> pq;
		
		if (ls.m_mx>=0) pq.push(ls);
		if (rs.m_mx>=0) pq.push(rs);
		
		std::vector<bool> select(n);
		std::fill(select.begin(), select.end(), false);
		
		select[i0] = true;
		select[i1] = true;
		
		int count=2;
		while (!pq.empty()) {
			path_segment ts = pq.top();
			pq.pop();
			
			select[ts.m_mx]=true;
			if (++count == lim)
				break;
			// split
			ls = path_segment(m_path, ts.m_i0, ts.m_mx);
			if (ls.m_mx >= 0) pq.push(ls);
			rs = path_segment(m_path, ts.m_mx, ts.m_i1);
			if (rs.m_mx >= 0) pq.push(rs);
		}
		
		m_path.clear();
		for (int i=0; i<n; i++) {
			if (select[i]==1)
				m_path.push_back(m_path0[i]);
		}
	}
bail_out:
    if (m_sign == '+' && mode==1)
        clip();
}

void SpriteMeshGenerator::path::fit(std::vector<int>& ci, int i0, int i1)
{
    int n = (int)m_path.size();
    
    if ((mod(i0+1, n) == i1) || (i0==i1)) {
        ci.push_back(i1);
        return;
    }
    
    Vector2f  a  = m_path[i0].p;
    Vector2f  b  = m_path[i1].p;
    edge  e  = edge(a, b);
    int   im = -1;
    float qm = -1;
    int   ic = i0;
    do {
        float qc = fabs(e.grad(m_path[ic].p));
        if (qc > qm) {
            im = ic;
            qm = qc;
        }
        if (ic == i1)
            break;
        ic = mod(ic+1, n);
    }while(1);
    
    float lim = std::max(fabs(e.m_a)*0.5, fabs(e.m_b)*0.5);
    if ( (qm <= lim) || (im < 0)) {
        ci.push_back(i1);
        return;
    }
    fit(ci, i0, im);
    fit(ci, im, i1);
}

bool SpriteMeshGenerator::path::opt(float bias)
{
    int n  = (int)m_path.size();
    if (n<3)
        return false;
    
    std::vector<int> cp;
    std::vector<int> ci;
    
    int s = -1;
    int dt[8] = {0};
    cp.push_back(0);
    for (int i=0; i<n; i++) {
        Vector2f p0 = m_path[i].p;
        Vector2f p1 = m_path[mod(i+1, n)].p;
        
        int d = dir(p0, p1);
        if (d < 0)
            continue;
        dt[d] = 1;
        if (s < 0) {
            s = d;
            continue;
        }
        // cut path, if direction change is not possible for straight line
        if (!(d == mod(s-1, 8) || d == mod(s+1, 8) || d == s) ||
            ((dt[0] + dt[1] +
              dt[2] + dt[3] +
              dt[4] + dt[5] +
              dt[6] + dt[7] ) > 2)) {
            memset(dt, 0, 8*sizeof(int));
            s = -1;
            cp.push_back(i);
        }
    }
    // fit sub paths to straight line
    int m = (int)cp.size();
    for (int i=0; i<m; i++)
        fit(ci, cp[i], cp[mod(i+1, m)]);
    
    //rm extra vertices
    std::vector<vertex> tmp = m_path;
    m_path.clear();
    for(int i=0; i<ci.size(); i++)
        m_path.push_back(tmp[ci[i]]);

	// unit normals
	n = (int)m_path.size();
	for (int i=0; i<n; i++)
		dec(i);
	
	// bias
	for (int i=0; i<n; i++)
		m_path[i].p += m_path[i].n*bias;

    return true;
}

bool SpriteMeshGenerator::invmask(std::vector<vertex>& outline)
{
    int n = (int)outline.size();
    if (n <= 0)
        return false;
    
    int xa = (int)outline[  0].p.x;
    Vector2f pp = outline[n-1].p;
    
    for (int i=0; i<n; i++) {
        Vector2f p0 = outline[i].p;
        
        while (((i+1)<n) && (p0.y == outline[i+1].p.y) ) {
            int d = dir(p0, outline[i+1].p);
            if ((d==1 && (pp.y < p0.y)) ||
                (d==5 && (pp.y > p0.y)) )
                p0 = outline[i+1].p;
            i++;
        }
        int y  = (int)p0.y;
        int x0 = min(xa, (int)p0.x);
        int x1 = max(xa, (int)p0.x);
		for (int x=x0; x<x1; x++)
			m_mask_cur.inv(x, y);
        
        if (((i+1) < n)    &&
            (pp.y != p0.y) && (outline[i+1].p.y == pp.y) ) {
            y  = (int)p0.y;
			x0 = min(xa, (int)p0.x);
			x1 = max(xa, (int)p0.x);
			for (int x=x0; x<x1; x++)
				m_mask_cur.inv(x, y);
        }
        pp = p0;
    }
    for (int i=0; i<n; i++) {
        Vector2f p = outline[i].p;
        m_mask_cur.rst(p.x, p.y);
    }
    return true;
}

bool SpriteMeshGenerator::trace(Vector2f p0, Vector2f p1, Vector2f &p)
{
    static int dt[8][2] = {
        { -1,  0 },
        { -1, -1 },
        {  0, -1 },
        {  1, -1 },
        {  1,  0 },
        {  1,  1 },
        {  0,  1 },
        { -1,  1 }
    };
    
    int t0 = dir(p0, p1);
    if (t0 < 0)
        goto error;
    
    for (int i = 0; i < 8; i++) {
        int t = (t0 + i) % 8;
        int x = (int)p1.x + dt[t][0];
        int y = (int)p1.y + dt[t][1];
        if (m_mask_cur.tst(x, y)) {
            p = Vector2f(x, y);
            return true;
        }
    }
error:
    p = Vector2f(-1, -1);
    return false;
}

bool SpriteMeshGenerator::contour(std::vector<vertex>& outline, int &sign, float &area)
{
    do {
        outline.clear();
        int b = m_mask_cur.first();
        if (b < 0)
            return false;
        
        int x = b % m_mask_cur.w;
        int y = b / m_mask_cur.w;
        Vector2f curr = Vector2f (x, y);
        Vector2f stop;
        Vector2f prev;
        Vector2f next;
        area   = 0.0;
        sign   = m_mask_org.tst(x, y) ? '+' : '-';
        stop   = curr;
        next   = curr;
        curr.x = curr.x-1;
        
        do {
            prev = curr;
            curr  = next;
            outline.push_back(vertex(curr));
            if (trace(prev, curr, next) == false)
                break;
            
            area += curr.x * next.y -
            curr.y * next.x;
            if (next == stop)
                break;
        } while(true);
        
        invmask(outline);
        if (fabs(area)<4) {
            area=0;
            continue;
        }
        if (((sign=='+') && (area < 0)) ||
            ((sign=='-') && (area > 0)) )
            std::reverse(outline.begin(), outline.end());
        area = fabs(area);
        break;
    }while(1);
    return true;
}
#endif //ENABLE_SPRITES
