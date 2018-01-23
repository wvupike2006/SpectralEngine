#pragma once
#include <BaseExecutor.h>
#include "RenderPass1.h"
#include <array>
class D3D12GraphicsEngine;
class Assets;

class Executor : public BaseExecutor
{
protected:
	void update1(std::unique_lock<std::mutex>&& lock);
	virtual void update2(std::unique_lock<std::mutex>&& lock) override;

	static void finishInitializing(BaseExecutor* exe);
public:
	Executor(SharedResources* const sharedResources, unsigned long uploadHeapStartingSize, unsigned int uploadRequestBufferStartingCapacity, unsigned int halfFinishedUploadRequestBufferStartingCapasity) : 
		BaseExecutor(sharedResources), renderPass(this),
		streamingManager(sharedResources->graphicsEngine.graphicsDevice, uploadHeapStartingSize, uploadRequestBufferStartingCapacity, halfFinishedUploadRequestBufferStartingCapasity)
	{}

	static void update1NextPhaseJob(BaseExecutor* exe, std::unique_lock<std::mutex>&& lock);
	static void update2NextPhaseJob(BaseExecutor* exe, std::unique_lock<std::mutex>&& lock) { reinterpret_cast<Executor*>(exe)->update2(std::move(lock)); }
	static void initialize(BaseExecutor* exe, std::unique_lock<std::mutex>&& lock);

	StreamingManagerThreadLocal streamingManager;//local
	RenderPass1::Local renderPass;

	uint32_t frameIndex() { return sharedResources->graphicsEngine.frameIndex; }
	Assets* getSharedResources() { return (Assets*)sharedResources; }
};