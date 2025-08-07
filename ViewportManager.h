#pragma once
#include <d3d12.h>
#include "Window.h"

class ViewportManager
{
public:
	void Initialize(Window window);
	const D3D12_VIEWPORT& GetViewport() const
	{
		return viewport;
	}
private:
	//ビューポート
	D3D12_VIEWPORT viewport{};
};
