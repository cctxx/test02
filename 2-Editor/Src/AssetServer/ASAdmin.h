#ifndef ASAdmin_H
#define ASAdmin_H

#include "Runtime/Utilities/GUID.h"
#include "Editor/Src/AssetServer/Backend/PgConn.h"

using std::vector;
using std::set;
using std::map;
using std::string;

struct MaintUserRecord 
{
	bool enabled;
	string userName;
	string fullName;
	string email;

	MaintUserRecord(bool i_Enabled, const string& i_UserName, const string& i_FullName, const string& i_Email) : 
		enabled(i_Enabled), userName(i_UserName), fullName(i_FullName), email(i_Email) { }
	bool operator < (const MaintUserRecord &other) const 
	{
		return userName < other.userName;
	}
};

struct MaintDatabaseRecord 
{
	string name;
	string dbName;

	MaintDatabaseRecord(const string& i_Name, const string& i_DBName) : 
		name(i_Name), dbName(i_DBName) { }
	bool operator < (const MaintDatabaseRecord &other) const 
	{
		return name < other.name;
	}
};

class Admin
{
private:
	set<MaintUserRecord> m_Users;
	set<MaintDatabaseRecord> m_Databases;

	string m_server;
	int m_port;
	string m_user;
	string m_password;

	PGconn* GetAdminConnection(string databaseName);
public:
	static Admin& Get ();

	void AdminSetCredentials(const string& server, int port, const string& user, const string& password);
	bool AdminRefreshDatabases();
	bool AdminGetUsers(const string& databaseName);

	bool AdminCreateDB(const string& newProjectName, const string& copyOf=string());
	bool AdminDeleteDB(const string& projectName);
	bool AdminCreateUser(const string& userName, const string& userFullName, const string& userEmail, const string& userPassword);
	bool AdminDeleteUser(const string& userName);
	void AdminChangePassword(const string& userName, const string& newPassword);
	bool SetUserEnabled(const string& databaseName, const string& userName, const string& fullName, const string& email, bool enabled);
	bool ModifyUserInfo(const string& databaseName, const string& userName, const string& fullName, const string& email);

	const set<MaintUserRecord>& GetUsers() const { return m_Users; }
	const set<MaintDatabaseRecord>& GetDatabases() const { return m_Databases; }
};

#endif
