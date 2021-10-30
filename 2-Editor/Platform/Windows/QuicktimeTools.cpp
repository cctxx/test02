#include "UnityPrefix.h"

#if UNITY_EXTERNAL_TOOL

#include "Editor/Platform/Interface/Quicktime.h"
#include "Editor/Platform/Windows/QuicktimeTools.h"
#include "Editor/Src/AssetPipeline/QuickTimeMovieImporter.h"

enum Operation
{
	USAGE,
	TRANSCODE,
	DURATION
};

int requestedOperation = USAGE;
std::string input;
std::string output;
int vbr = 0, abr = 0;
bool verbose = false;

bool ParseCommandline(int argc, char* argv[]);
int ShowHelp();
int Transcode(const std::string& input, const std::string& output, int vbr, int abr);
int GetDuration(const std::string& input);
void UpdateProgress (float individualAssetProgress, float overrideTotalProgress, const std::string& customTitle);

int main(int argc, char* argv[])
{
	bool validCommandline = ParseCommandline(argc, argv);

	int operationSuccess = 0;
	switch(requestedOperation)
	{
		case USAGE:
			 operationSuccess = ShowHelp();
			 break;
		case TRANSCODE:
			 operationSuccess = Transcode(input, output, vbr, abr);
			 break;
		case DURATION:
			 operationSuccess = GetDuration(input);
			 break;
	}

	return validCommandline ? operationSuccess : QTT_GENERIC_ERROR;
}

bool ParseCommandline(int argc, char* argv[])
{
	if (argc < 2)
	{
		printf("Error: No operation provided.\n");
		return false;	// ShowHelp
	}

	const char* operation = argv[1];
	if (!_stricmp(operation, "help"))
		requestedOperation = USAGE;
	else if (!_stricmp(operation, "transcode"))
		requestedOperation = TRANSCODE;
	else if (!_stricmp(operation, "duration"))
		requestedOperation = DURATION;
	else
	{
		printf("Error: Unknown operation '%s'.\n", operation);
		return false;
	}

	for (int i = 2; i < argc; ++i)
	{
		const char* var = argv[i];
		if (!_stricmp(var, "-input") && i < argc-1)
			input = argv[++i];
		else if (!_stricmp(var, "-output") && i < argc-1)
			output = argv[++i];
		else if (!_stricmp(var, "-vbr") && i < argc-1)
			vbr = atoi(argv[++i]);
		else if (!_stricmp(var, "-abr") && i < argc-1)
			abr = atoi(argv[++i]);
		else if (!_stricmp(var, "-verbose") && i < argc-1)
			verbose = true;
	}

	if (verbose)
	{
		printf("Log: -input = %s\n", input.c_str());
		printf("Log: -output = %s\n", output.c_str());
		printf("Log: -vbr = %i\n", vbr);
		printf("Log: -abr = %i\n", abr);
	}

	return true;
}

int ShowHelp()
{
	printf("\n");
	printf("QuicktimeTools <operation> [options]\n");
	printf("\nOperations:\n");
	printf("\thelp		- Show this info\n");
	printf("\ttranscode	- Transcode video\n");
	printf("\tduration	- Retrieve video duration\n");
	printf("\nOptions:\n");
	printf("\t-intput		- Source file\n");
	printf("\t-output		- Destination file\n");
	printf("\t-vbr		- Video bitrate\n");
	printf("\t-abr		- Audio bitrate\n");
	printf("\t-verbose	- Additional logging\n");
	return QTT_NO_ERRORS_OR_WARNINGS;
}

int Transcode(const std::string& input, const std::string& output, int vbr, int abr)
{
	if (input.empty())
	{
		printf("Error: No input provided.\n");
		return QTT_GENERIC_ERROR;
	}
	if (output.empty())
	{
		printf("Error: No output provided.\n");
		return QTT_GENERIC_ERROR;
	}
	if (!vbr)
	{
		printf("Error: No video bitrate provided.\n");
		return QTT_GENERIC_ERROR;
	}
	if (!abr)
	{
		printf("Error: No audio bitrate provided.\n");
		return QTT_GENERIC_ERROR;
	}

	if (verbose)	printf("Log: Initializing QTML.\n");

	if (InitializeQTML(0L) != 0)
	{
		printf("InitializeQTML Error %i\n", InitializeQTML(0L));
		return QTT_NEED_QUICKTIME;
	}

	QuickTimeMovieImporter qtimport;

	if (verbose)	printf("Log: Opening '%s'.\n", input.c_str());

	if (!qtimport.Open (input))
	{
		printf("Error: Unable to open '%s'.\n", input.c_str());
		return QTT_UNABLE_TO_READ;
	}

	if (verbose)	printf("Log: Setting VBR/ABR to %i / %i.\n", vbr, abr);
	qtimport.SetOggVideoBitrate(vbr);
	qtimport.SetOggAudioBitrate(abr);

	if (verbose)	printf("Log: Transcoding to '%s'\n", output.c_str());

	if(!qtimport.TranscodeToOgg(output, UpdateProgress, true))
	{
		printf("Error: %s\n", qtimport.GetErrorMessage());
		return QTT_UNABLE_TO_ENCODE;
	}

	if (verbose)	printf("Log: Done.\n");

	int ret = QTT_NO_ERRORS_OR_WARNINGS;

	if (!qtimport.GetWarningMessage().empty())
	{
		printf("%s\n", qtimport.GetWarningMessage());
		ret = QTT_WARNINGS_WHILE_ENCODING;
	}

	if (verbose)	printf("Log: Terminating QTML.\n");
	TerminateQTML();
	return ret;
}

int GetDuration(const std::string& input)
{
	if (input.empty())
	{
		printf("Error: No input provided.\n");
		return QTT_GENERIC_ERROR;
	}

	if (verbose)	printf("Log: Initializing QTML.\n");

	if (InitializeQTML(0L) != 0)
	{
		printf("InitializeQTML Error %i\n", InitializeQTML(0L));
		return QTT_NEED_QUICKTIME;
	}

	QuickTimeMovieImporter qtimport;

	if (verbose)	printf("Log: Opening '%s'.\n", input.c_str());

	if (!qtimport.Open (input))
	{
		printf("Error: Unable to open '%s'.\n", input.c_str());
		return QTT_UNABLE_TO_READ;
	}

	if (verbose)	printf("Log: Requesting video duration.\n");

	printf("%f\n", qtimport.GetDuration());

	if (verbose)	printf("Log: Terminating QTML.\n");
	TerminateQTML();
	return QTT_NO_ERRORS_OR_WARNINGS;
}

void UpdateProgress (float individualAssetProgress, float overrideTotalProgress, const std::string& customTitle)
{
	printf("[");
	float f;
	for (f = 0.f; f < individualAssetProgress; f += 0.05f)
		printf("*");
	for (; f < 1.0; f += 0.05f)
		printf(" ");
	printf("] { %2.1f }\r", individualAssetProgress*100.f);
}

#endif