#pragma once
#include <atomic>

class WorldManagerStopRequest
{
public:
	std::atomic<unsigned long> numberOfComponentsUnloaded = 0u;
	unsigned long numberOfComponentsToUnload;
	void(*callback)(WorldManagerStopRequest& stopRequest, void* tr, void* gr);

	void onZoneStopped(void* tr, void* gr)
	{
		if(numberOfComponentsUnloaded.fetch_add(1u, std::memory_order_acq_rel) == (numberOfComponentsToUnload - 1u))
		{
			callback(*this, tr, gr);
		}
	}
};