#ifndef LOAD_PROGRESS_H
#define LOAD_PROGRESS_H

class LoadProgress
{
	volatile float* progressIndicator;
	float progressInterval;
	
public:
	float totalItems;
	float processedItems;
	
	LoadProgress(unsigned total, float interval, float* indicator) : processedItems(0), totalItems(total), progressIndicator(indicator), progressInterval (interval) {}
	
	void ItemProcessed (int count = 1)
	{
		processedItems = std::min (totalItems, processedItems + count);
		
		if (progressIndicator)
			*progressIndicator = totalItems == 0 ? 1.0f : progressInterval * processedItems / totalItems;
	}
	
};
#endif
