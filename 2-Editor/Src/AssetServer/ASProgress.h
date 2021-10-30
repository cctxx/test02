#ifndef ASPROGRESS_H
#define ASPROGRESS_H


namespace AssetServer {

// Implement this to get callbacks for updating a progress bar.
class ProgressCallback {
	public:
	ProgressCallback() {}
	~ProgressCallback() {}
	virtual void UpdateProgress(SInt64 bytes, SInt64 total, const std::string& currentFileName)=0;
	virtual bool ShouldAbort()=0;
};

}

#endif