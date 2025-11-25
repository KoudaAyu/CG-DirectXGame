#include "ParticleSystem.h"
#include <DirectXMath.h>
#include <cassert>
#include <cmath>

ParticleSystem::ParticleSystem()
{
}

ParticleSystem::~ParticleSystem()
{
	if (particles_)
	{
		delete[] particles_;
	}
	// instanceResource_ will be released by ComPtr
}

void ParticleSystem::Initialize(DirectXCom* dxCommon, Sprite* quadSprite, uint32_t numInstances)
{
	dxCommon_ = dxCommon;
	quad_ = quadSprite;
	numInstances_ = numInstances;

	particles_ = new Particle[numInstances_];
	for (uint32_t i = 0; i < numInstances_; ++i)
	{
		particles_[i].pos = { 0.0f,0.0f,0.0f };
		particles_[i].vel = { 0.0f,0.0f,0.0f };
		particles_[i].life = 0.0f;
	}

	instanceResource_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice(), sizeof(TransformationMatrix) * numInstances_);
	instanceResource_->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_));
	for (uint32_t i = 0; i < numInstances_; ++i)
	{
		instanceData_[i].WVP = MakeIdentity4x4();
		instanceData_[i].World = MakeIdentity4x4();
	}
}

void ParticleSystem::Spawn8()
{
	const float twoPi = DirectX::XM_2PI;
	const float speed = 5.0f;
	for (uint32_t i = 0; i < numInstances_; ++i)
	{
		float angle = twoPi * float(i) / float(numInstances_);
		particles_[i].pos = { 0.0f,0.0f,0.0f };
		particles_[i].vel = { cosf(angle) * speed, sinf(angle) * speed, 0.0f };
		particles_[i].life = 3.0f;
	}
}

void ParticleSystem::Update(float dt, Camera* camera)
{
	for (uint32_t i = 0; i < numInstances_; ++i)
	{
		if (particles_[i].life > 0.0f)
		{
			particles_[i].pos.x += particles_[i].vel.x * dt;
			particles_[i].pos.y += particles_[i].vel.y * dt;
			particles_[i].pos.z += particles_[i].vel.z * dt;
			particles_[i].life -= dt;
		}

		Vector3 scale = { 0.5f,0.5f,1.0f };
		Vector3 rotate = { 0.0f,0.0f,0.0f };
		Vector3 translate = particles_[i].pos;
		Matrix4x4 world = MakeAffineMatrix(scale, rotate, translate);
		Matrix4x4 wvp = Multiply(world, camera->GetViewProjectionMatrix());
		instanceData_[i].World = world;
		instanceData_[i].WVP = wvp;
	}
}

void ParticleSystem::Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvGPU, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvGPU2,
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource,
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLight,
	D3D12_GPU_DESCRIPTOR_HANDLE instanceSrvHandleGPU,
	bool useMonsterBall)
{
	if (!quad_) return;
	auto cmd = dxCommon_->GetCommandList();
	const D3D12_VERTEX_BUFFER_VIEW& quadVB = quad_->GetVertexBufferViewSprite();
	const D3D12_INDEX_BUFFER_VIEW& quadIB = quad_->GetIndexBufferViewSprite();
	cmd->IASetVertexBuffers(0, 1, &quadVB);
	cmd->IASetIndexBuffer(&quadIB);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cmd->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	cmd->SetGraphicsRootConstantBufferView(1, quad_->GetTransformationMatrixResourceSprite()->GetGPUVirtualAddress());
	cmd->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvGPU2 : textureSrvGPU);
	cmd->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());
	cmd->SetGraphicsRootDescriptorTable(4, instanceSrvHandleGPU);

	// Count alive particles so we only draw those instances
	uint32_t aliveCount = 0;
	for (uint32_t i = 0; i < numInstances_; ++i)
	{
		if (particles_[i].life > 0.0f) ++aliveCount;
	}

	if (aliveCount == 0) return; // nothing to draw

	cmd->DrawIndexedInstanced(6, aliveCount, 0, 0, 0);
}
