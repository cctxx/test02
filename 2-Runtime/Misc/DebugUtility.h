#ifndef DEBUGUTILITY_H
#define DEBUGUTILITY_H

class Vector3f;
class ColorRGBAf;
class EditorExtension;

#if GAMERELEASE

//#define DebugDrawLine (a,b,c,d) ;
//#define ClearLines () ;
//#define PauseEditor () ;
inline void DebugDrawLine (const Vector3f& p0, const Vector3f& p1, const ColorRGBAf& color, double durationSeconds, bool depthTest){}
inline void ClearLines (){}
inline void ClearAllLines (){}
inline void PauseEditor (){}

#else

void DebugDrawLine (const Vector3f& p0, const Vector3f& p1, const ColorRGBAf& color, double durationSeconds, bool depthTest);
void ClearLines ();
void ClearAllLines ();
void PauseEditor ();

void DrawDebugLinesGizmo ();

#endif


#endif
