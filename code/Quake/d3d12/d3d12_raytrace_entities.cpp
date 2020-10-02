// d3d12_raytrace_entities.cpp
//

#include "d3d12_local.h"
#include "nv_helpers_dx12/BottomLevelASGenerator.h"
#include "nv_helpers_dx12/TopLevelASGenerator.h"
#include <vector>

nv_helpers_dx12::TopLevelASGenerator m_topLevelASGenerator;
AccelerationStructureBuffers m_topLevelASBuffers;

std::vector<entity_t*> r_dxrEntities;

int r_currentDxrEntities = -1;

struct dxrMeshIntance_t {
	int startVertex;
};

dxrMeshIntance_t meshInstanceData[MAX_VISEDICTS];

ComPtr<ID3D12Resource> m_instanceProperties;

D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
void GL_CreateInstanceInfo(D3D12_CPU_DESCRIPTOR_HANDLE& srvPtr) {
	uint32_t bufferSize = ROUND_UP( static_cast<uint32_t>(MAX_VISEDICTS) * sizeof(dxrMeshIntance_t), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	// Create the constant buffer for all matrices
	m_instanceProperties = nv_helpers_dx12::CreateBuffer( m_device.Get(), bufferSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = MAX_VISEDICTS;
	srvDesc.Buffer.StructureByteStride = sizeof(dxrMeshIntance_t);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	// Write the per-instance properties buffer view in the heap
	m_device->CreateShaderResourceView(m_instanceProperties.Get(), &srvDesc, srvPtr);

}

void GL_CreateTopLevelAccelerationStructs(bool forceUpdate) {
	// Add in the entities.
	int numProcessedEntities = 1;

	for (int i = 0; i < cl_numvisedicts; i++)
	{
		entity_t *currententity = cl_visedicts[i];

		dxrMesh_t* mesh = (dxrMesh_t*)currententity->model->dxrModel;
		if (mesh == NULL)
			continue;

		switch (currententity->model->type)
		{
			case mod_brush:
				create_brush_matrix(&currententity->dxrTransform[0], currententity, false);
				numProcessedEntities++;
				break;
			case mod_alias:
				create_entity_matrix(&currententity->dxrTransform[0], currententity, false);
				numProcessedEntities++;
				break;
		}

		if (currententity->model->flags & EF_ROTATE)
		{ // Floating motion
			currententity->dxrTransform[14] += sin(currententity->origin[0]
				+ currententity->origin[1] + (cl.time * 3)) * 5.5;
		}
	}

	// Add the view entity
	{
		entity_t* currententity = &cl.viewent;
		dxrMesh_t* mesh = (dxrMesh_t*)currententity->model->dxrModel;
		create_entity_matrix(&currententity->dxrTransform[0], currententity, false);
	}

	bool onlyUpdate = (numProcessedEntities == r_currentDxrEntities);
	r_currentDxrEntities = numProcessedEntities;

	if(!onlyUpdate || forceUpdate)
	{
		m_topLevelASGenerator.Clear();

		// Add in the BSP world.
		{
			// World matrix is always a identity.
			static DirectX::XMMATRIX worldmatrix = DirectX::XMMatrixIdentity();
			m_topLevelASGenerator.AddInstance(dxrMeshList[0]->buffers.pResult.Get(), worldmatrix, 0, 0);
		}

		for (int i = 0; i < cl_numvisedicts; i++)
		{
			entity_t* currententity = cl_visedicts[i];

			dxrMesh_t* mesh = (dxrMesh_t*)currententity->model->dxrModel;
			if (mesh == NULL)
				continue;

			meshInstanceData[i + 1].startVertex = mesh->startSceneVertex;

			switch (currententity->model->type)
			{
			case mod_brush:
			case mod_alias:				
				m_topLevelASGenerator.AddInstance(mesh->buffers.pResult.Get(), (DirectX::XMMATRIX&)currententity->dxrTransform, i + 1, 0);
				break;
			}
		}

		// Add the view entity
		{
			entity_t* currententity = &cl.viewent;
			dxrMesh_t* mesh = (dxrMesh_t*)currententity->model->dxrModel;
			meshInstanceData[cl_numvisedicts + 1].startVertex = mesh->startSceneVertex;
			m_topLevelASGenerator.AddInstance(mesh->buffers.pResult.Get(), (DirectX::XMMATRIX&)currententity->dxrTransform, cl_numvisedicts + 1, 0);
		}

		// Update our instance info.
		if (m_instanceProperties != nullptr)
		{
			dxrMeshIntance_t* current = nullptr;

			CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
			ThrowIfFailed(m_instanceProperties->Map(0, &readRange, reinterpret_cast<void**>(&current)));

			for(int d = 0; d < MAX_VISEDICTS; d++)
			{
				memcpy(current, &meshInstanceData[d], sizeof(dxrMeshIntance_t));
				current++;
			}
			m_instanceProperties->Unmap(0, nullptr);
		}

		if (forceUpdate)
		{
			// As for the bottom-level AS, the building the AS requires some scratch space
			// to store temporary data in addition to the actual AS. In the case of the
			// top-level AS, the instance descriptors also need to be stored in GPU
			// memory. This call outputs the memory requirements for each (scratch,
			// results, instance descriptors) so that the application can allocate the
			// corresponding memory
			UINT64 scratchSize, resultSize, instanceDescsSize;

			m_topLevelASGenerator.ComputeASBufferSizes(m_device.Get(), true, &scratchSize,
				&resultSize, &instanceDescsSize);

			// Create the scratch and result buffers. Since the build is all done on GPU,
			// those can be allocated on the default heap
			m_topLevelASBuffers.pScratch = nv_helpers_dx12::CreateBuffer(
				m_device.Get(), scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nv_helpers_dx12::kDefaultHeapProps);
			m_topLevelASBuffers.pResult = nv_helpers_dx12::CreateBuffer(
				m_device.Get(), resultSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
				nv_helpers_dx12::kDefaultHeapProps);

			// The buffer describing the instances: ID, shader binding information,
			// matrices ... Those will be copied into the buffer by the helper through
			// mapping, so the buffer has to be allocated on the upload heap.
			m_topLevelASBuffers.pInstanceDesc = nv_helpers_dx12::CreateBuffer(
				m_device.Get(), instanceDescsSize, D3D12_RESOURCE_FLAG_NONE,
				D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

			// After all the buffers are allocated, or if only an update is required, we
			// can build the acceleration structure. Note that in the case of the update
			// we also pass the existing AS as the 'previous' AS, so that it can be
			// refitted in place.
		}
	}
	
	m_topLevelASGenerator.Generate(m_commandList.Get(),
		m_topLevelASBuffers.pScratch.Get(),
		m_topLevelASBuffers.pResult.Get(),
		m_topLevelASBuffers.pInstanceDesc.Get(), onlyUpdate, m_topLevelASBuffers.pResult.Get());
}