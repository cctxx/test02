#ifndef __REPORT_HARDWARE_H
#define __REPORT_HARDWARE_H

class WWW;


class HardwareInfoReporter {
public:
	HardwareInfoReporter() : m_InfoPost(NULL) { };

	void ReportHardwareInfo();
	void Shutdown();

private:
	WWW*	m_InfoPost;
};


#endif
