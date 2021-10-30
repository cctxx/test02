#ifndef PGRESULT_H
#define PGRESULT_H


#include "Runtime/Utilities/File.h"
#if ENABLE_ASSET_SERVER
#include "External/postgresql/include/libpq-fe.h"
#else
typedef void PGconn;
typedef void PGresult;
#endif
#include <string>
#include <map>

// The classes in this file are somewhat incestuous with each other, for the sake of code reuse 
// and a functioning reference counting of m_Res pointers. Move carefully and wear protective 
// helmet at all times.
class PgConn;

/// Common base class for PgResult and PgColumn. Cannot be instantiated freely.
class PgBaseResult {
 public:
	/// Reference counting destructor
	~PgBaseResult();

	bool Ok();
	std::string ErrorStr() const;

	/// Get libpq status code for result
	int Status()
	{
		#if ENABLE_ASSET_SERVER
		return PQresultStatus(m_Res);
		#else
		return 0;
		#endif
	}

	/// Returns number of rows in result set
	int Rows()
	{
		#if ENABLE_ASSET_SERVER
		if (m_Rows == -1 && m_Res)
			m_Rows = PQntuples(m_Res);
		return m_Rows;
		#else
		return 0;
		#endif
	}
	
 protected:
	PgBaseResult();
	PgBaseResult(const PgBaseResult& orig);

	/// Shared and reference counted libpq result set opaque object
 	PGresult* m_Res;

	/// Pointer to PgConn object that created the result set
 	const PgConn* m_Conn;
 	/// Shared integer, that contains the reference count
	int* m_Refs;
	/// Cached row-count (initially -1, but that value is never returned)
	int m_Rows;

};

/// One-column holder. Can only be instantiated from a PgResult, and will share its PGresult opaque object, which is reference counted.
class PgColumn : public PgBaseResult {
 public:
	PgColumn(PgBaseResult& res, int col) : PgBaseResult(res) { m_Col = col; }
	PgColumn(const PgColumn& original) : PgBaseResult(original) { m_Col = original.m_Col; }

	/// (See comments about PgResult::IsNull)
 	bool IsNull(int row);
	/// (See comments about PgResult::IsNull)
 	int GetInt(int row);
	/// (See comments about PgResult::IsNull)
 	std::string GetString(int row);
	/// (See comments about PgResult::IsNull)
 	bool GetBool(int row);
	/// (See comments about PgResult::IsNull)
	std::vector<std::string> GetStringArray(int row);
	/// (See comments about PgResult::IsNull)
 	bool WriteString(int row, File& fileHandle, int offset);

	int GetLength(int row);

 private:
	int m_Col;

};

/// Wrapper of a PostgreSQL result set, which is reference counted.
class PgResult : public PgBaseResult {
 public:
	/// Can instantiate a PgResult from a PGresult pointer (or without to create empty result holder for later use)
	PgResult(const PgConn* conn = NULL, PGresult* res = NULL) { m_Conn=conn; m_Res = res; }
	/// Assignment operator must maintain reference count
	PgResult& operator= (const PgResult& orig);

	bool HasColumn(const std::string& col);

	/// Always check this before trusting a value read from a result set. Null values are returned 
	/// as empty strings and integer -1 respectively. The libpq documentation doesn't seem to specify 
	/// what happens if values outside the returned data are queried for, so this is explicitly 
	/// left as undefined behaviour.
	bool IsNull(const std::string& col, int row);

	/// (See comments about IsNull)
	int GetInt(const std::string& col, int row);
	/// (See comments about IsNull)
	std::string GetString(const std::string& col, int row);
	/// (See comments about IsNull)
	bool GetBool(const std::string& col, int row);

	/// Fetch column holder for given column
	PgColumn GetColumn(const std::string& col) { return PgColumn(*this, ColNum(col)); }
	PgColumn GetColumn(int col) { return PgColumn(*this, col); }
	/// Fetch column holder for only column
	PgColumn GetOnlyColumn();

	/// Check if result set has one and only one row in one and only one column
	bool HasOneValue() { return Rows() == 1 && Columns() == 1 && ! IsNull (0,0); }
	bool HasOneColumn() { return Columns() == 1; }

	/// Read sole value as an integer (whether it is a sole value or is an int isn't checked for you)
	int GetOneInt() { return GetInt(0,0); }
	/// Read sole value as an string (whether it is a sole value or is a string isn't checked for you)
	std::string GetOneString() { return GetString(0,0); }
	/// Read sole value as a bool (whether it is a sole value or is a string isn't checked for you)
	bool GetOneBool() { return GetBool(0,0); }

	/// Return number of columns in result set
	int Columns() {
		#if ENABLE_ASSET_SERVER
		return PQnfields(m_Res);
		#else
		return 0;
		#endif
	}

 private:
	// Methods hidden from the public interface because they use raw column numbers
	int GetInt(int colnum, int row);
	std::string GetString(int colnum, int row);
	bool GetBool(int colnum, int row);
	int GetLength(int colnum, int row);
	bool IsNull(int colnum, int row);

	// Resolution method
	int ColNum(const std::string& col);
	
};

#endif
