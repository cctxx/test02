#include "UnityPrefix.h"

#if ENABLE_UNIT_TESTS

#include "SerializedFile.h"
#include "External/UnitTest++/src/UnitTest++.h"
#include "Runtime/Serialize/FileCache.h"

SUITE (SerializedFile)
{
	#if SUPPORT_SERIALIZE_WRITE
	TEST_FIXTURE (SerializedFile, ReadWriteSerializedFileWorks)
	{
		CachedWriter writer;
		FileCacherWrite writeFile;
		writeFile.InitWriteFile("test.serialized", 16);
		writer.InitWrite(writeFile);

		SerializedFile* file = UNITY_NEW_AS_ROOT (SerializedFile(), kMemSerialization, "SerializedFile", "");
		CHECK(file->InitializeWrite (writer, BuildTargetSelection::NoTarget(), 0));
		CHECK(file->FinishWriting());
		UNITY_DELETE(file, kMemSerialization);

		file = UNITY_NEW_AS_ROOT (SerializedFile(), kMemSerialization, "SerializedFile", "");

		ResourceImageGroup resources;
		CHECK(file->InitializeRead("test.serialized", resources, 16, 2, 0));
		CHECK(!file->IsFileDirty());
		CHECK(file->IsEmpty());

		UNITY_DELETE(file, kMemSerialization);
		DeleteFile("test.serialized");
	}	
	#endif

} // SUITE


#endif
