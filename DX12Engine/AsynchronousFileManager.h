#pragma once
#include <unordered_map>
#include <cstdint>
#include "File.h"
#include "IOCompletionQueue.h"
#include "ActorQueue.h"

class AsynchronousFileManager
{
private:
	struct ResourceId
	{
		const wchar_t* filename;
		std::size_t start;
		std::size_t end;

		bool operator==(const ResourceId& other) const
		{
			return filename == other.filename && start == other.start && end == other.end;
		}
	};

	struct Hasher
	{
		std::size_t operator()(const ResourceId& key) const
		{
			std::size_t result = (std::size_t)key.filename;
			result = result * 31u + key.start;
			result = result * 31u + key.end;
			return result;
		}
	};
public:
	enum class Action : short
	{
		allocate,
		deallocate,
	};

	class ReadRequest : public OVERLAPPED, public SinglyLinked, public ResourceId
	{
	public:
		Action action;
		//location to read
		File file;
		//location to put the result
		unsigned char* buffer;
		//amount read
		std::size_t accumulatedSize;
		//what to do with the result
		void(*fileLoadedCallback)(ReadRequest& request, void* executor, void* sharedResources, const unsigned char* data);
		void(*deleteReadRequest)(ReadRequest& request, void* tr, void* gr);

		ReadRequest() {}
		ReadRequest(const wchar_t* filename, File file, std::size_t start, std::size_t end,
			void(*fileLoadedCallback)(ReadRequest& request, void* executor, void* sharedResources, const unsigned char* data),
			void(*deleteRequest)(ReadRequest& request, void* executor, void* sharedResources)) :
			file(file),
			fileLoadedCallback(fileLoadedCallback),
			deleteReadRequest(deleteRequest)
		{
			this->filename = filename;
			this->start = start;
			this->end = end;
		}
	};
private:
	struct FileData
	{
		unsigned char* allocation;
		unsigned int userCount;
		ReadRequest* requests;
	};

	std::unordered_map<ResourceId, FileData, Hasher> files;
	std::size_t pageSize;
	ActorQueue messageQueue;

	static bool processIOCompletionHelper(AsynchronousFileManager& fileManager, void* executor, void* sharedResources, DWORD numberOfBytes, LPOVERLAPPED overlapped);
	void run(void* executor, void* sharedResources);
	bool readFile(ReadRequest& request, void* tr, void* gr);
	void discard(ReadRequest& resource, void* tr, void* gr);
public:
	AsynchronousFileManager();
	~AsynchronousFileManager();

	template<class GlobalResources>
	static bool processIOCompletion(void* executor, void* sharedResources, DWORD numberOfBytes, LPOVERLAPPED overlapped)
	{
		GlobalResources& globalResources = *static_cast<GlobalResources*>(sharedResources);
		return processIOCompletionHelper(globalResources.asynchronousFileManager, executor, sharedResources, numberOfBytes, overlapped);
	}

	template<class GlobalResources>
	File openFileForReading(IOCompletionQueue& ioCompletionQueue, const wchar_t* name)
	{
		File file(name, File::accessRight::genericRead, File::shareMode::readMode, File::creationMode::openExisting, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING);
		ioCompletionQueue.associateFile(file.native_handle(), (ULONG_PTR)(void*)(processIOCompletion<GlobalResources>));
		return file;
	}

	template<class ThreadResources, class GlobalResources>
	void readFile(ReadRequest* request, ThreadResources&, GlobalResources& globalResources)
	{
		request->action = Action::allocate;
		bool needsStarting = messageQueue.push(request);
		if(needsStarting)
		{
			IOCompletionPacket task;
			task.numberOfBytesTransfered = 0u;
			task.overlapped = reinterpret_cast<LPOVERLAPPED>(this);
			task.completionKey = reinterpret_cast<ULONG_PTR>(static_cast<bool(*)(void* executor, void* sharedResources, DWORD numberOfBytes, LPOVERLAPPED overlapped)>(
				[](void* tr, void* gr, DWORD, LPOVERLAPPED overlapped)
			{
				auto& afm = *reinterpret_cast<AsynchronousFileManager*>(overlapped);
				afm.run(tr, gr);
				return true;
			}));
			globalResources.ioCompletionQueue.push(task);
		}
	}

	template<class ThreadResources, class GlobalResources>
	void discard(ReadRequest* request, ThreadResources&, GlobalResources& globalResources)
	{
		request->action = Action::deallocate;
		bool needsStarting = messageQueue.push(request);
		if(needsStarting)
		{
			IOCompletionPacket task;
			task.numberOfBytesTransfered = 0u;
			task.overlapped = reinterpret_cast<LPOVERLAPPED>(this);
			task.completionKey = reinterpret_cast<ULONG_PTR>(static_cast<bool(*)(void* executor, void* sharedResources, DWORD numberOfBytes, LPOVERLAPPED overlapped)>(
				[](void* tr, void* gr, DWORD, LPOVERLAPPED overlapped)
			{
				auto& afm = *reinterpret_cast<AsynchronousFileManager*>(overlapped);
				afm.run(tr, gr);
				return true;
			}));
			globalResources.ioCompletionQueue.push(task);
		}
	}
};