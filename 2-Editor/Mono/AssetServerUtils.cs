// Utility classess for handling asset server stuff
using System;
using UnityEngine;
using Object=UnityEngine.Object;
using System.IO;
using System.Collections.Generic;
using System.Text.RegularExpressions;

namespace UnityEditor {

internal class AssetServerConfig {

	private Dictionary<string, string> fileContents;
	private string fileName;
	
	private static Regex sKeyTag = new Regex(@"<key>([^<]+)</key>");
	private static Regex sValueTag = new Regex(@"<string>([^<]+)</string>");
	
	public AssetServerConfig() {
		fileContents=new Dictionary<string, string>();
		fileName=Application.dataPath+"/../Library/ServerPreferences.plist";
		
		try // World's smallest xml parser -- just enough for the asset server plist
		{
            using (StreamReader sr = new StreamReader(fileName)) 
            {
                string line;
				string key=".unkown";
                // Read and display lines from the file until the end of 
                // the file is reached.
                while ((line = sr.ReadLine()) != null) 
                {
                    Match m =  sKeyTag.Match(line) ;
					if(m.Success)
						key=m.Groups[1].Value;
					m =  sValueTag.Match(line) ;
					if(m.Success)
						fileContents[key]=m.Groups[1].Value;
					
                }
            }
        }
        catch (Exception e) 
        {
            // Let the user know what went wrong.
            Debug.Log("Could not read asset server configuration: "+ e.Message);
        }		
	}

	public string connectionSettings {
		get {return fileContents["Maint Connection Settings"];}
		set {fileContents["Maint Connection Settings"]=value;}
	}
	public string server{
		get {return fileContents["Maint Server"];}
		set {fileContents["Maint Server"]=value;}
	}

	public int portNumber{
		get {return int.Parse(fileContents["Maint port number"]);}
		set {fileContents["Maint port number"]=value.ToString();}
	}

	public float timeout{
		get {return float.Parse(fileContents["Maint Timeout"]);}
		set {fileContents["Maint Timeout"]=value.ToString();}
	}

	public string userName{
		get {return fileContents["Maint UserName"];}
		set {fileContents["Maint UserName"]=value;}
	}

	public string dbName{
		get {return fileContents["Maint database name"];}
		set {fileContents["Maint database name"]=value;}
	}

	public string projectName{
		get {return fileContents["Maint project name"];}
		set {fileContents["Maint project name"]=value;}
	}

	public string settingsType{
		get {return fileContents["Maint settings type"];}
		set {fileContents["Maint settings type"]=value;}
	}

}

}