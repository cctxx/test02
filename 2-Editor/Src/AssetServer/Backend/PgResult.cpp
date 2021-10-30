#include "UnityPrefix.h"
#include "PgResult.h"

#if ENABLE_ASSET_SERVER

#include "PgConn.h"
#include "External/postgresql/include/libpq-fe.h"
#include "Runtime/Utilities/LogAssert.h"

#include <sys/types.h>

#if UNITY_WIN
#include <Winsock2.h>
#else
#include <netinet/in.h>
#endif

#include <string>


// Helper function
static int NetworkIntFromBuffer(const char* buff);
static int NetworkIntFromBuffer(const char* buff) {
	// Make a network-byte-ordered integer from the fetched data
	const int *network = reinterpret_cast<const int*>(buff);
	// Convert to host (local) byte order and return
	int host = ntohl(*network);
	return host;
}



/* PgBaseResult */

PgBaseResult::PgBaseResult() {
	m_Refs = new int; 
	(*m_Refs) = 1; 
	m_Rows = -1; 
}

PgBaseResult::PgBaseResult(const PgBaseResult& orig) {
	m_Refs = orig.m_Refs;
	(*m_Refs)++;
	m_Res = orig.m_Res;
	m_Conn = orig.m_Conn;
	m_Rows = orig.m_Rows;
}

PgBaseResult::~PgBaseResult() {
	if (--(*m_Refs) == 0) {
		delete m_Refs; 
		if (m_Res) 
			PQclear(m_Res); 
	}
}

bool PgBaseResult::Ok() {
	if (m_Res != NULL) {
		int status = PQresultStatus(m_Res);
		if (status == PGRES_TUPLES_OK || status == PGRES_COMMAND_OK)
			return true;
	}
	return false;
}

std::string PgBaseResult::ErrorStr() const {
	if(m_Res) return std::string(PQresultErrorMessage(m_Res));
	if(m_Conn) return m_Conn->ErrorStr();
	return std::string();
}



/* PgColumn */

int PgColumn::GetInt(int row) {
	if (IsNull(row) || row > Rows() || GetLength(row) != 4)
		return 0;

	return NetworkIntFromBuffer(PQgetvalue(m_Res, row, m_Col));
}

std::string PgColumn::GetString(int row) {
	if (row > Rows() || IsNull(row))
		return std::string();

	return std::string(PQgetvalue(m_Res, row, m_Col), GetLength(row));
}

std::vector<std::string> PgColumn::GetStringArray(int row) {
	std::vector<std::string> result;
	if (row > Rows() || IsNull(row) || GetLength(row) < 20)
		return result;
	const char * rawData = PQgetvalue(m_Res, row, m_Col);
	int elements = NetworkIntFromBuffer(rawData+3*sizeof(int));
	int dim = NetworkIntFromBuffer(rawData+4*sizeof(int));
	if(dim != 1)
		return result; // We only support one-dimensional arrays
	
	const char* start=rawData + 5*sizeof(int);
	for(int i = 0 ; i < elements; i++) {
		int size = NetworkIntFromBuffer(start);
		result.push_back(string(start + sizeof(int), size));
		start += size + sizeof(int);
	}

	return result;
}


bool PgColumn::WriteString(int row, File& fileHandle, int offset) {
	if (row > Rows() || IsNull(row))
		return false;
	return fileHandle.Write(offset, PQgetvalue(m_Res, row, m_Col), GetLength(row));
}


bool PgColumn::GetBool(int row) {
	if (row > Rows() || IsNull(row) || GetLength(row) != 1)
		return false;

	char* byte = static_cast<char*>(PQgetvalue(m_Res, row, m_Col));
	if (*byte == '\0')
		return false;
	else
		return true;
}

bool PgColumn::IsNull(int row) {
	return PQgetisnull(m_Res, row, m_Col) == 1;
}

int PgColumn::GetLength(int row) {
	return PQgetlength(m_Res, row, m_Col);
}



/* PgResult */

PgResult& PgResult::operator= (const PgResult& orig) {
	// Throw out old result set (and refcount variable) if refcount hits zero
	if (--(*m_Refs) == 0) {
		delete m_Refs; 
		if (m_Res) 
			PQclear(m_Res); 
	}

	// Take data from PgResult being assigned from, and increase now-shared reference count
	m_Refs = orig.m_Refs; 
	(*m_Refs)++; 
	m_Res = orig.m_Res; 
	m_Rows = orig.m_Rows;

	return *this;
}


bool PgResult::HasColumn(const std::string& colname) {
	if (ColNum(colname) == -1)
		return false;
	else
		return true;
}
PgColumn PgResult::GetOnlyColumn() { 
	if (Columns() != 1) 
		AssertString("PgResult::GetOnlyColumn(): Result set doesn't have exactly 1 column (while that was implied)");

	return PgColumn(*this, 0);
}

int PgResult::GetInt(const std::string& colname, int row) {
	int col = ColNum(colname);
	if (col == -1)
		return 0;

	return GetInt(col, row);
}

std::string PgResult::GetString(const std::string& colname, int row) {
	int col = ColNum(colname);
	if (col == -1)
		return std::string();

	return GetString(col, row);
}

int PgResult::GetInt(int col, int row) {
	if (IsNull(col, row) || row > Rows() || col > Columns() || GetLength(col, row) != 4)
		return -1;

	return NetworkIntFromBuffer(PQgetvalue(m_Res, row, col));
}

std::string PgResult::GetString(int col, int row) {
	if (col > Columns() || row > Rows() || IsNull(col, row))
		return std::string();

	return string(PQgetvalue(m_Res, row, col), GetLength(col, row));
}

bool PgResult::GetBool(int col, int row) {
	if (col > Columns() || row > Rows() || IsNull(col, row) || GetLength(col, row) != 1)
		return false;

	char* byte = static_cast<char*>(PQgetvalue(m_Res, row, col));
	if (*byte == '\0')
		return false;
	else
		return true;
}

bool PgResult::IsNull(const std::string& colname, int row) {
	int col = ColNum(colname);
	if (col == -1)
		return true;
		
	return PQgetisnull(m_Res, row, col) == 1;
}

bool PgResult::IsNull(int col, int row) {
	return PQgetisnull(m_Res, row, col) == 1;
}

int PgResult::GetLength(int col, int row) {
	return PQgetlength(m_Res, row, col);
}

int PgResult::ColNum(const string& colname) {
	if (PQresultStatus(m_Res) != PGRES_TUPLES_OK || PQnfields(m_Res) == 0)
		return -1;

	int colnum = PQfnumber(m_Res, colname.c_str());
	if (colnum == -1)
		LogString("Column " + colname + " not in result set");

	return colnum;
}

#else // ENABLE_ASSET_SERVER

bool PgColumn::IsNull(int row) { return false; }
int PgColumn::GetInt(int row) { return 0; }
std::string PgColumn::GetString(int row) { return std::string(); }
bool PgColumn::GetBool(int row) { return false; }
std::vector<std::string> PgColumn::GetStringArray(int row) { return std::vector<std::string>(); }
bool PgColumn::WriteString(int row, File& fileHandle, int offset) { return false; }
int PgColumn::GetLength(int row) { return 0; }

int PgResult::GetInt(const std::string& colname, int row) { return 0; }
int PgResult::GetInt(int col, int row) { return 0; }
std::string PgResult::GetString(const std::string& colname, int row) { return std::string(); }
int PgResult::ColNum(const std::string& colname) { return 0; }

PgBaseResult::PgBaseResult() { }
PgBaseResult::PgBaseResult(const PgBaseResult& orig) { }
PgBaseResult::~PgBaseResult() { }
bool PgBaseResult::Ok() { return false; }
std::string PgBaseResult::ErrorStr() const { return std::string(); }

PgResult& PgResult::operator= (const PgResult& orig) { return *this; }
bool PgResult::IsNull(int col, int row) { return false; }


#endif // ENABLE_ASSET_SERVER
