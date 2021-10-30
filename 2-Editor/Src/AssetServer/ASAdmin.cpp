#include "UnityPrefix.h"
#include "ASAdmin.h"
#include "ASCache.h"
#include "ASController.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Src/AssetServer/Backend/PgConn.h"
#include "Editor/Src/LicenseInfo.h"

#include "Configuration/UnityConfigureVersion.h"

using namespace std;
using namespace AssetServer;

// TODO: use non-blocking interface for server admin functions

#define ConnectionError(error) { Controller::Get().SetConnectionError(error); ErrorString(error);}
#define Error(error) { Controller::Get().SetError(error); ErrorString(error);}

Admin& Admin::Get() 
{
	static Admin* s_Admin= new Admin();
	return *s_Admin;
}

PGconn* Admin::GetAdminConnection(string databaseName)
{
	#if ENABLE_ASSET_SERVER

	if (!LicenseInfo::Flag (lf_maint_client))
	{
		// make sure nothing works at all when user does not have license
		// don't show any errors, as only way user will get here is by hacking protection (or some weird bug)
		return NULL;
	}

	PGconn* connection = NULL;
	PgConn::FlushFreeConnections();

	Controller::Get().ClearError();

	if (databaseName.empty())
		databaseName = "template1";

	string connectionString = Format("host='%s' user='%s' password='%s' dbname='%s' port='%d' sslmode=disable",
		m_server.c_str(), m_user.c_str(), m_password.c_str(), databaseName.c_str(), m_port);

	connection = PQconnectdb(connectionString.c_str());

	// Check to see that the connection was successfully made
	if (PQstatus(connection) == CONNECTION_OK)
		return connection;

	const char* error = PQerrorMessage(connection);
	if(strstr(error, "FATAL: ") == error) 
	{
		error += 8;
	}

	ConnectionError("Could not log in to " + m_server + "\n" + string(error));

	PQfinish(connection);

	#endif // ENABLE_ASSET_SERVER

	return NULL; 
}

void Admin::AdminSetCredentials(const string& server, int port, const string& user, const string& password)
{
	m_server = server;
	m_port = port;
	m_user = user;
	m_password = password;

	m_Databases.clear();
}

bool Admin::AdminRefreshDatabases()
{
	#if ENABLE_ASSET_SERVER

	PGconn	   *conn;
	PGresult   *res;

	conn = GetAdminConnection("");

	// Check to see that the backend connection was successfully made 
	if (conn)
	{
		// Fetch rows from pg_database, the system catalog of databases
		res = PQexec(conn,  "select projectName, databaseName from all_databases__view");

		ExecStatusType status  = PQresultStatus(res);
		if ( status != PGRES_TUPLES_OK )
		{
			Error(Format("Could not fetch a list of projects on server: %s", PQerrorMessage(conn)));
			PQclear(res);
			PQfinish(conn);
			return false;
		}

		m_Databases.clear();

		// loop through the list of databases and add them to the list 
		for (int i = 0; i < PQntuples(res); i++)
		{
			MaintDatabaseRecord r = MaintDatabaseRecord(  PQgetvalue(res, i, 0),  PQgetvalue(res, i, 1) );
			m_Databases.insert(r);
		}

		PQclear(res);
		PQfinish(conn);

		return true;
	}

	#endif // ENABLE_ASSET_SERVER

	return false;
}

bool Admin::AdminGetUsers(const string& databaseName)
{
	#if ENABLE_ASSET_SERVER

	PGconn	   *conn;
	PGresult   *res;

	m_Users.clear();
	conn = GetAdminConnection(databaseName);

	// Check to see that the backend connection was successfully made
	if (conn)
	{
		// Fetch rows from user records from the all_users__view
		res = PQexec(conn,  "select enabled, username, realname, email from all_users__view");

		ExecStatusType status  = PQresultStatus(res);
		if ( status != PGRES_TUPLES_OK )
		{
			Error(Format("Could not get list of users: %s", PQerrorMessage(conn)));
			PQclear(res);
			PQfinish(conn);
			return false;
		}

		// loop through the list of users and add them to the list
		for (int i = 0; i < PQntuples(res); i++)
		{
			MaintUserRecord r = MaintUserRecord( (PQgetvalue(res, i, 0)[0]=='t'),  PQgetvalue(res, i, 1),  PQgetvalue(res, i, 2),  PQgetvalue(res, i, 3));
			m_Users.insert(r);
		}

		PQclear(res);
		PQfinish(conn);

		return true;
	}

	#endif // ENABLE_ASSET_SERVER

	return false;
}

bool Admin::AdminCreateDB(const string& newProjectName, const string& copyOf)
{

	PGconn	   *conn;
	PGresult   *res;
	bool ret = false;

	#if ENABLE_ASSET_SERVER

	conn = GetAdminConnection("");

	// Check to see that the backend connection was successfully made 
	if (conn)
	{
		ExecStatusType status;
		string databaseName;
		string origDatabaseName;
		const char* values[3];

		values[0] = newProjectName.c_str();
		values[1] = copyOf.c_str();

		// First generate a database name for the project
		res = PQexecParams(conn,  "select db_name($1),db_name($2)", 2, NULL, values, NULL, NULL, 0);
		status  = PQresultStatus(res);
		if ( status > PGRES_TUPLES_OK )
		{
			Error(Format("Could not create new project: %s\n", PQerrorMessage(conn)));
			PQclear(res);
			goto cleanup;
		}

		databaseName = PQgetvalue(res, 0, 0);
		origDatabaseName = PQgetvalue(res, 0, 1);
		PQclear(res);

		// Then create the database
		{
			string createDBQuery = "create database \"" + databaseName + "\"";
			if ( ! copyOf.empty() && ! origDatabaseName.empty() ) {
				createDBQuery += " with template \"" + origDatabaseName	+ "\"";
			}
			res = PQexec(conn,  createDBQuery.c_str() );
			status  = PQresultStatus(res);
			if ( status > PGRES_TUPLES_OK )
			{
				Error(Format("Could not create new project: %s\n", PQerrorMessage(conn)));
				PQclear(res);
				goto cleanup;
			}
			PQclear(res);
		}

		// And save name and comment
		values[1] = "Created with Unity " UNITY_VERSION;
		values[2] = databaseName.c_str();
		res = PQexecParams(conn,  "select set_project_info($3,$1,$2)", 3, NULL, values, NULL, NULL, 0);
		status  = PQresultStatus(res);
		if ( status > PGRES_TUPLES_OK )
		{
			Error(Format("Could not create new project: %s\n", PQerrorMessage(conn)));
			PQclear(res);
			goto cleanup;
		}
		PQclear(res);

		// Fix up user records for all users copied from the old database
		if (! copyOf.empty() )
		{
			AdminGetUsers(databaseName);
			for (set<MaintUserRecord>::iterator i = m_Users.begin() ; i != m_Users.end() ; i++)
				SetUserEnabled(databaseName, i->userName, i->fullName, i->email, i->enabled);			
		}
		ret = true;

cleanup:
		PQfinish(conn);
	}

	#endif // ENABLE_ASSET_SERVER

	if (ret == true)
		LogString("Project '" + newProjectName + "' was created successfully.");

	return ret;
}

bool Admin::AdminDeleteDB(const string& projectName)
{
	PGconn	   *conn;
	PGresult   *res;
	bool ret = false;

	#if ENABLE_ASSET_SERVER

	conn = GetAdminConnection("");

	// Check to see that the backend connection was successfully made 
	if (conn)
	{
		ExecStatusType status;
		string databaseName;
		const char* values[3];

		values[0] = projectName.c_str();
		values[1] = NULL;
		values[2] = NULL;

		// First generate a database name for the project
		res = PQexecParams(conn,  "select db_name($1)", 1, NULL, values, NULL, NULL, 0);
		status  = PQresultStatus(res);
		if ( status > PGRES_TUPLES_OK )
		{
			Error(Format("Could not get database name: %s\n", PQerrorMessage(conn)));
			PQclear(res);
			goto cleanup;
		}

		databaseName = PQgetvalue(res, 0, 0);
		PQclear(res);

		// Then delete the database
		res = PQexec(conn,  ("drop database \"" + databaseName + "\"").c_str());
		status  = PQresultStatus(res);
		if ( status > PGRES_TUPLES_OK )
		{
			Error(Format("Could not delete project: %s\n", PQerrorMessage(conn)));
			PQclear(res);
			goto cleanup;
		}
		PQclear(res);

		ret = true;

cleanup:
		PQfinish(conn);
	}

	#endif // ENABLE_ASSET_SERVER


	if (ret == true)
		LogString("Project '" + projectName + "' was deleted successfully.");

	return ret;
}

bool Admin::AdminCreateUser(const string& userName, const string& userFullName, const string& userEmail, const string& userPassword)
{
	PGconn	   *conn;
	PGresult   *res;
	bool		ret = false;

	#if ENABLE_ASSET_SERVER

	//TODO: transaction?

	conn = GetAdminConnection("");

	// Check to see that the backend connection was successfully made
	if (conn)
	{
		ExecStatusType status;

		const char* values[4];

		values[0] = userName.c_str();
		values[1] = userFullName.c_str();
		values[2] = userEmail.c_str();
		values[3] = userPassword.c_str();

		res = PQexec(conn, ("create user \""+ userName + "\" password $_$" + userPassword + "$_$ nocreatedb nocreateuser").c_str());
		status = PQresultStatus(res);
		if ( status > PGRES_TUPLES_OK )
		{
			Error(Format("Could not create user: %s\n", PQerrorMessage(conn)));
			PQclear(res);
			goto cleanup;
		}
		PQclear(res);

		res = PQexecParams(conn, "select make_person($1, $2, $3)", 3, NULL, values, NULL, NULL, 0);
		if ( status > PGRES_TUPLES_OK )
		{
			Error(Format("Could not create user: %s\n", PQerrorMessage(conn)));
			PQclear(res);
			goto cleanup;
		}
		PQclear(res);

		/* - We are in template1.  Users should never get access to template1!
		// Allow user to commit to main trunk
		res = PQexecParams(conn,  "select assign_role($1, 'staff')", 1, NULL, values, NULL, NULL, 0);
		if ( status > PGRES_TUPLES_OK )
		{
			Error(Format("Could not create user: %s\n", PQerrorMessage(conn)));
			PQclear(res);
			goto cleanup;
		}
		PQclear(res);
		*/

		ret = true;
		LogString("User '" + userName + "' was created successfully.");

cleanup:
		PQfinish(conn);
	}

	#endif // ENABLE_ASSET_SERVER


	return ret;
}

bool Admin::AdminDeleteUser(const string& userName)
{
	PGconn	   *conn;
	PGresult   *res;
	bool		ret = false;

	#if ENABLE_ASSET_SERVER

	conn = GetAdminConnection("");

	// Check to see that the backend connection was successfully made
	if (conn)
	{
		ExecStatusType status;
		// First loop through all projects and revoke user access from them:
		res = PQexec(conn,  "select databaseName from all_databases__view");

		status  = PQresultStatus(res);
		if ( status != PGRES_TUPLES_OK )
		{
			Error(Format("Could not fetch a list of projects on server: %s", PQerrorMessage(conn)));
			PQclear(res);
			PQfinish(conn);
			return false;
		}

		for (int i = 0; i < PQntuples(res); i++)
		{
			if ( ! SetUserEnabled(  PQgetvalue(res, i, 0), userName, "*DELETE*", "*DELETE*", false ))
				return false;
		}
		// User might have been assigned to template1 by previous versions of Unity as well
		if ( ! SetUserEnabled( "template1", userName, "*DELETE*", "*DELETE*", false ) )
			return false;

		PQclear(res);
		
		res = PQexec(conn, ("drop user \"" + userName + "\"").c_str());
		status = PQresultStatus(res);
		if ( status > PGRES_TUPLES_OK )
		{
			Error(Format("Could not delete user: %s\n", PQerrorMessage(conn)));
			PQclear(res);
			goto cleanup;
		}
		PQclear(res);

		ret = true;
		LogString("User '" + userName + "' was deleted successfully.");

cleanup:
		PQfinish(conn);
	}

	#endif // ENABLE_ASSET_SERVER


	return ret;
}

void Admin::AdminChangePassword(const string& userName, const string& newPassword)
{
	#if ENABLE_ASSET_SERVER

	PGconn	   *conn;
	PGresult   *res;

	conn = GetAdminConnection("");

	// Check to see that the backend connection was successfully made
	if (conn)
	{
		ExecStatusType status;

		res = PQexec(conn, ("alter user \""+ userName + "\" password $_$" + newPassword + "$_$").c_str());
		status  = PQresultStatus(res);
		if ( status > PGRES_TUPLES_OK )
		{
			Error(Format("Could not change password: %s\n", PQerrorMessage(conn)));
			PQclear(res);
			PQfinish(conn);
			return;
		}
		PQclear(res);
		PQfinish(conn);

		LogString("Password changed for user '" + userName + "'");
	}

	#endif // ENABLE_ASSET_SERVER
}


bool Admin::SetUserEnabled(const string& databaseName, const string& userName, const string& fullName, const string& email, bool enabled)
{
	PGconn	   *conn;
	bool result = false;

	#if ENABLE_ASSET_SERVER

	conn = GetAdminConnection(databaseName);

	// Check to see that the backend connection was successfully made
	if (conn)
	{
		PGresult   *res;

		ExecStatusType status;

		const char* values[3];
		values[0] = userName.c_str();
		values[1] = fullName.c_str();
		values[2] = email.c_str();

		// Ensure person record exists
		res = PQexecParams(conn, "select make_person($1, $2, $3)", 3, NULL, values, NULL, NULL, 0);

		// Ignore results as any errors will be caught later on, and make_person will fail if the person record already exists
		PQclear(res);

		if (enabled) // Create user role in DB
		{ 
			res = PQexec(conn, ("grant asset_server_user to \"" + userName + "\"").c_str());
			PQclear(res);
			res = PQexecParams(conn, "select assign_role($1, 'staff')", 1, NULL, values, NULL, NULL, 0);
		}
		else // Remove user role from DB
		{ 
			res = PQexecParams(conn,  "select revoke_role($1, 'staff')", 1, NULL, values, NULL, NULL, 0);
		}

		status = PQresultStatus(res);
		if ( status > PGRES_TUPLES_OK ) 
		{
			Error(Format("User update failed: %s", PQerrorMessage(conn)));
		}
		else 
		{
			result = true;
		}

		PQclear(res);
	}

	PQfinish(conn);

	#endif // ENABLE_ASSET_SERVER

	return result;
}

bool Admin::ModifyUserInfo(const string& databaseName, const string& userName, const string& fullName, const string& email)
{
	PGconn	   *conn;
	bool result = false;

	#if ENABLE_ASSET_SERVER

	conn = GetAdminConnection(databaseName);

	// Check to see that the backend connection was successfully made
	if (conn)
	{
		PGresult   *res;

		ExecStatusType status;

		const char* values[3];
		values[0] = userName.c_str();
		values[1] = fullName.c_str();
		values[2] = email.c_str();

		// Ensure person record exists
		res = PQexecParams(conn, "select make_person($1, $2, $3)", 3, NULL, values, NULL, NULL, 0);

		// Ignore results as any errors will be caught later on, and make_person will fail if the person record already exists
		PQclear(res);

		values[1] = fullName.c_str();
		values[2] = email.c_str();
		res = PQexecParams(conn, "select update_person($1, $2, $3)", 3, NULL, values, NULL, NULL, 0);

		status = PQresultStatus(res);
		if ( status > PGRES_TUPLES_OK ) 
		{
			Error(Format("User update failed: %s", PQerrorMessage(conn)));
		}
		else 
		{
			result = true;
		}

		PQclear(res);
	}

	PQfinish(conn);

	#endif // ENABLE_ASSET_SERVER

	return result;
}
