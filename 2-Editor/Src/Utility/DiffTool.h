#ifndef DIFF_TOOL_H
#define DIFF_TOOL_H

#include <vector>
#include <string>

std::vector<std::string> GetAvailableDiffTools();

std::string InvokeDiffTool(
	const std::string& leftTitle, const std::string& leftFile,
	const std::string& rightTitle, const std::string& rightFile,
	const std::string& ancestorTitle, const std::string& ancestorFile );

std::string InvokeMergeTool(
	const std::string& leftTitle, const std::string& leftFile,
	const std::string& rightTitle, const std::string& rightFile,
	const std::string& ancestorTitle, const std::string& ancestorFile,
	const std::string& outputFile );

std::string GetNoDiffToolsDetectedMessage();
#endif
