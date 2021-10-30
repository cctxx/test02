#include "UnityPrefix.h"
#include "Runtime/Testing/Testing.h"

#if ENABLE_PERFORMANCE_TESTS

#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Profiler/TimeHelper.h"

extern void MultiplyMatrices4x4REF (const Matrix4x4f* __restrict lhs, const Matrix4x4f* __restrict rhs, Matrix4x4f* __restrict res);
extern void MultiplyMatrices4x4SSE (const Matrix4x4f* __restrict lhs, const Matrix4x4f* __restrict rhs, Matrix4x4f* __restrict res);

extern void CopyMatrixREF( const float* __restrict lhs, float* __restrict res);
extern void CopyMatrixSSE( const float* __restrict lhs, float* __restrict res);

#define ITERATIONS_COUNT 10000


/*
		one of the good results ( launched with Shift+F5 in release mode ):
		ITERATIONS_COUNT: 10000
		timeElapsedREF: 1393
		timeElapsedSSE: 639
		timeElapsedJOE: 647
		total cycles REF: 1423390
		total cycles SSE: 650381
		total cycles JOE: 660177
		avg cycles REF: 142
		avg cycles SSE: 65
		avg cycles JOE: 66

		
		Matrix Copy: Ref vs SSE
		ITERATIONS_COUNT=10000
		time elapsedCPY: 2114
		time elapsedCPYSSE: 964
		total cycles CPY: 2157582
		total cycles CPYSSE: 980763
		avg cycles CPY: 215
		avg cycles CPYSSE: 98
		ctrl:20000.000000

*/


void TestMultiplyMatrices()
{
	Matrix4x4f m0, m1, m2;
#define RESET_MATS() for(int i=0;i<16;i++) { m0.m_Data[i] = (float)(i+1); m1.m_Data[15-i] = (float)(i+1); }


	RESET_MATS();

	ABSOLUTE_TIME startTimeREF = GetStartTime();
	UInt64 cycStartREF = __rdtsc();
	for(UInt32 i=0;i<ITERATIONS_COUNT;i++)
	{
		MultiplyMatrices4x4REF(&m0, &m1, &m2);
		m0.m_Data[0]*=-1.f;
		m1.m_Data[0]*=-1.f;
	}
	UInt64 cycEndREF = __rdtsc();
	ABSOLUTE_TIME elapsedREF = GetElapsedTime(startTimeREF);

	RESET_MATS();

	startTimeREF = GetStartTime();
	cycStartREF = __rdtsc();
	for(UInt32 i=0;i<ITERATIONS_COUNT;i++)
	{
		MultiplyMatrices4x4REF(&m0, &m1, &m2);
		m0.m_Data[0]*=-1.f;
		m1.m_Data[0]*=-1.f;
	}
	cycEndREF = __rdtsc();
	elapsedREF = GetElapsedTime(startTimeREF);

	RESET_MATS();

	ABSOLUTE_TIME startTimeJOE = GetStartTime();
	UInt64 cycStartJOE = __rdtsc();
	for(UInt32 i=0;i<ITERATIONS_COUNT;i++)
	{
		MultiplyMatrices4x4(&m0, &m1, &m2);
		m0.m_Data[0]*=-1.f;
		m1.m_Data[0]*=-1.f;
	}
	UInt64 cycEndJOE = __rdtsc();
	ABSOLUTE_TIME elapsedJOE = GetElapsedTime(startTimeJOE);

	RESET_MATS();

	startTimeJOE = GetStartTime();
	cycStartJOE = __rdtsc();
	for(UInt32 i=0;i<ITERATIONS_COUNT;i++)
	{
		MultiplyMatrices4x4(&m0, &m1, &m2);
		m0.m_Data[0]*=-1.f;
		m1.m_Data[0]*=-1.f;
	}
	cycEndJOE = __rdtsc();
	elapsedJOE = GetElapsedTime(startTimeJOE);

	
	UInt64 avgCycREF = (cycEndREF - cycStartREF) / ITERATIONS_COUNT;
	UInt64 avgCycJOE = (cycEndJOE - cycStartJOE) / ITERATIONS_COUNT;
	
#if UNITY_WIN
	{
		char szMsg[1024];
		sprintf(szMsg, "ITERATIONS_COUNT=%d\r\ntime elapsedREF: %I64d\r\ntime elapsedJOE: %I64d\r\ntotal cycles REF: %I64d\r\ntotal cycles JOE: %I64d\r\navg cycles REF: %I64d\r\navg cycles JOE: %I64d\r\nctrl:%f" , 
			ITERATIONS_COUNT, elapsedREF, elapsedJOE, cycEndREF - cycStartREF, cycEndJOE - cycStartJOE, avgCycREF, avgCycJOE, m0.m_Data[4] + m1.m_Data[5] + m2.m_Data[7]);
		OutputDebugString(LPCSTR(szMsg));
		MessageBox(0, szMsg, "REF vs SSE Multiply", MB_ICONINFORMATION);
	}
#else
	printf_console("REF vs SSE Multiply: ITERATIONS_COUNT=%d\r\ntime elapsedREF: %I64d\r\ntime elapsedJOE: %I64d\r\ntotal cycles REF: %I64d\r\ntotal cycles JOE: %I64d\r\navg cycles REF: %I64d\r\navg cycles JOE: %I64d\r\nctrl:%f" , 
		ITERATIONS_COUNT, elapsedREF, elapsedJOE, cycEndREF - cycStartREF, cycEndJOE - cycStartJOE, avgCycREF, avgCycJOE, m0.m_Data[4] + m1.m_Data[5] + m2.m_Data[7]);
#endif

	RESET_MATS();

	ABSOLUTE_TIME startTimeCPY = GetStartTime();
	UInt64 cycStartCPY = __rdtsc();
	float f=0.f;
	for(UInt32 i=0;i<ITERATIONS_COUNT;i++)
	{
		CopyMatrixREF(m0.GetPtr(), m1.GetPtr());
		f+=m1.m_Data[0];
	}
	UInt64 cycEndCPY = __rdtsc();
	ABSOLUTE_TIME elapsedCPY = GetElapsedTime(startTimeCPY);

	RESET_MATS();

	ABSOLUTE_TIME startTimeCPYSSE = GetStartTime();
	UInt64 cycStartCPYSSE = __rdtsc();
	for(UInt32 i=0;i<ITERATIONS_COUNT;i++)
	{
		CopyMatrixSSE(m0.GetPtr(), m1.GetPtr());
		f+=m1.m_Data[0];
	}
	UInt64 cycEndCPYSSE = __rdtsc();
	ABSOLUTE_TIME elapsedCPYSSE = GetElapsedTime(startTimeCPYSSE);

	UInt64 avgCycCPY = (cycEndCPY - cycStartCPY) / ITERATIONS_COUNT;
	UInt64 avgCycCPYSSE = (cycEndCPYSSE - cycStartCPYSSE) / ITERATIONS_COUNT;

#if UNITY_WIN
	{
		char szMsg[1024];
		sprintf(szMsg, "ITERATIONS_COUNT=%d\r\ntime elapsedCPY: %I64d\r\ntime elapsedCPYSSE: %I64d\r\ntotal cycles CPY: %I64d\r\ntotal cycles CPYSSE: %I64d\r\navg cycles CPY: %I64d\r\navg cycles CPYSSE: %I64d\r\nctrl:%f" , 
			ITERATIONS_COUNT, elapsedCPY, elapsedCPYSSE, cycEndCPY - cycStartCPY, cycEndCPYSSE - cycStartCPYSSE, avgCycCPY, avgCycCPYSSE, f);
		OutputDebugString(LPCSTR(szMsg));
		MessageBox(0, szMsg, "REF vs SSE copy", MB_ICONINFORMATION);
	}
#else
	printf_console("REF vs SSE copy: ITERATIONS_COUNT=%d\r\ntime elapsedCPY: %I64d\r\ntime elapsedCPYSSE: %I64d\r\ntotal cycles CPY: %I64d\r\ntotal cycles CPYSSE: %I64d\r\navg cycles CPY: %I64d\r\navg cycles CPYSSE: %I64d\r\nctrl:%f" , 
		ITERATIONS_COUNT, elapsedCPY, elapsedCPYSSE, cycEndCPY - cycStartCPY, cycEndCPYSSE - cycStartCPYSSE, avgCycCPY, avgCycCPYSSE, f);
#endif




}

#endif