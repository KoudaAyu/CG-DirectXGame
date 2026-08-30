#include "RenderContext.h"
#include <d3d12.h>
#include <cassert>

RenderContext::RenderContext(ID3D12GraphicsCommandList* cmdList, WindowAPI* winAPI, Camera* cam, Light* lt)
	: commandList(cmdList), windowAPI(winAPI), camera(cam), light(lt)
{
}

void RenderContext::IASetVertexBuffers(UINT startSlot, UINT numViews, const D3D12_VERTEX_BUFFER_VIEW* pViews) const
{
	assert(commandList);
	commandList->IASetVertexBuffers(startSlot, numViews, pViews);
}

void RenderContext::IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* pView) const
{
	assert(commandList);
	commandList->IASetIndexBuffer(pView);
}

void RenderContext::IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology) const
{
	assert(commandList);
	commandList->IASetPrimitiveTopology(topology);
}

void RenderContext::SetGraphicsRootConstantBufferView(UINT rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS bufferLocation) const
{
	assert(commandList);
	commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, bufferLocation);
}

void RenderContext::SetGraphicsRootDescriptorTable(UINT rootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE baseDescriptor) const
{
	assert(commandList);
	commandList->SetGraphicsRootDescriptorTable(rootParameterIndex, baseDescriptor);
}

void RenderContext::DrawIndexedInstanced(UINT indexCountPerInstance, UINT instanceCount, UINT startIndexLocation, INT baseVertexLocation, UINT startInstanceLocation) const
{
	assert(commandList);
	commandList->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
}

void RenderContext::DrawInstanced(UINT vertexCountPerInstance, UINT instanceCount, UINT startVertexLocation, UINT startInstanceLocation) const
{
	assert(commandList);
	commandList->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
}
