#ifndef ANALYTICS_H
#define ANALYTICS_H

#include <string>

// Helper to get build time and status for Analytics for long running processes. 

// AnalyticsProcessTracker tracker ("Build", "WebBundle", "label");
// ... Do heavy work
// tracker.Succeeded(); // If you don't call succeeded, 
// When tracker gets destructed, it will send the register the event and 
struct AnalyticsProcessTracker
{
	AnalyticsProcessTracker(const std::string &category, const std::string &action, const std::string &label = "");
	~AnalyticsProcessTracker();
	
	void Succeeded()
	{
		m_Succeeded = true;
	}
private:
	std::string m_Category, m_Action, m_Label;
	double m_TimeStart;
	bool m_Succeeded;
};


void AnalyticsTrackPageView(const std::string &page, bool forceRequest = false);
void AnalyticsTrackEvent(const std::string &category, const std::string &action, const std::string &label, int value, bool forceRequest = false);


#endif