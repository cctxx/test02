#ifndef RECURSIONLIMIT_H
#define RECURSIONLIMIT_H

class RecursionLimiter {
public:
	RecursionLimiter (unsigned int *count)
	{
		m_Count = count;
		(*m_Count)++;
	}
	
	~RecursionLimiter ()
	{
		(*m_Count)--;
	}
private:
	unsigned int *m_Count;
};

#define LIMIT_RECURSION(x, retval) static unsigned int reclimit = 0; RecursionLimiter limiter(&reclimit); if (reclimit > x) return retval;


class ReentrancyChecker
{
public:
	ReentrancyChecker( bool* variable )
		: m_Variable(variable)
	{
		if( *m_Variable == false )
		{
			*m_Variable = true;
			m_OK = true;
		}
		else
		{
			m_OK = false;
		}
	}
	~ReentrancyChecker()
	{
		if( m_OK )
		{
			*m_Variable = false;
		}
	}
	bool IsOK() const { return m_OK; }

private:
	bool*	m_Variable;
	bool	m_OK;
};


#endif
