#pragma once
#include <d3d12.h>
#include"Window.h"
class ScissorManager
{
public:
	void Initialize(Window window);
	const D3D12_RECT& GetScissorRect() const
	{
		return scissorRect;
	}
private:
	//シザー矩形
	D3D12_RECT scissorRect{};
};
