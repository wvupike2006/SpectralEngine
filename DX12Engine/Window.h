#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef min
#undef max
#undef WIN32_LEAN_AND_MEAN
#include "DXGISwapChain4.h"
#include <cstdint>
class GraphicsEngine;
struct ID3D12Resource;

class Window
{
	friend GraphicsEngine;
	static constexpr auto applicationName = L"DX12Engine";
	constexpr static float fieldOfView = 3.141592654f / 4.0f;

	HWND windowHandle;
	DXGISwapChain4 swapChain;
	unsigned int mwidth, mheight;
	bool fullScreen;
	bool vSync;

	void createSwapChain(GraphicsEngine& graphicsEngine, IDXGIFactory5* dxgiFactory);
public:
	Window(void* callbackData, LRESULT CALLBACK WndProc(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam),
		unsigned int width, unsigned int height, int positionX, int positionY, bool fullScreen, bool vSync);
	~Window();

	void resize(unsigned int width, unsigned int height, int positionX, int positionY);
	void setForgroundAndShow();

	unsigned int width() const
	{
		return mwidth;
	}
	unsigned int height() const
	{
		return mheight;
	}

	HWND native_handle()
	{
		return windowHandle;
	}

	uint32_t getCurrentBackBufferIndex();
	ID3D12Resource* getBuffer(uint32_t index);
	void present();
	void setFullScreen();
	void setWindowed();
	void setVSync(bool value) { vSync = value; }
	bool getVSync() { return vSync; }

	static bool processMessagesForAllWindowsCreatedOnCurrentThread();
};