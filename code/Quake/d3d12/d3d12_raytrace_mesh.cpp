// d3d12_raytrace_mesh.cpp
//

#include "d3d12_local.h"
#include "nv_helpers_dx12/BottomLevelASGenerator.h"
#include "nv_helpers_dx12/TopLevelASGenerator.h"
#include <vector>

std::vector<dxrMesh_t *> dxrMeshList;


ComPtr<ID3D12Resource> m_vertexBuffer;
D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
std::vector<dxrVertex_t> sceneVertexes;

void GL_DumpSceneVertexes(void) {
	// Write Obj test	
	{
		FILE* f = fopen("test.obj", "wb");
	
		for (int i = 0; i < sceneVertexes.size(); i++) {
			fprintf(f, "v %f %f %f\n", sceneVertexes[i].xyz[0], sceneVertexes[i].xyz[2], -sceneVertexes[i].xyz[1]);
		}
	
		for (int i = 0; i < sceneVertexes.size(); i += 3) {
			int idx1 = i + 0;
			int idx2 = i + 1;
			int idx3 = i + 2;
	
			fprintf(f, "f %d %d %d\n", idx1 + 1, idx2 + 1, idx3 + 1);
		}
	
		fclose(f);
	}
}

void GL_LoadBottomLevelAccelStruct(dxrMesh_t* mesh, msurface_t* surfaces, int numSurfaces) {
	glpoly_t* p;

	mesh->startSceneVertex = sceneVertexes.size();

	for (int i = 0; i < numSurfaces; i++)
	{
		msurface_t* fa = &surfaces[i];
		dxrSurface_t surf;

		int materialInfo = 1;

		if(strstr(map_name, "e1m7")) {
			materialInfo = 1;
		}

		float x, y, w, h;
		const char* name = fa->texinfo->texture->name;
		if(name[0] == '*') {
			materialInfo = 1;
			name++;
		}

		if (strstr(name, "04water")) {
			name = "water0"; // stupid hack, fix the damn atlas!
			materialInfo = 1;
		}

		if (strstr(name, "afloor3_1")) {
			materialInfo = 1;
		}

		if (strstr(name, "wall9_8")) {
			materialInfo = 1;
		}

		if (strstr(name, "wizmet")) {
			materialInfo = 1;
		}

		if (strstr(name, "wiz1_1")) {
			materialInfo = 1;
		}

		if (strstr(name, "wmet")) {
			materialInfo = 1;
		}

		if (strstr(name, "city2_7")) {
			materialInfo = 1;
		}

		if (strstr(name, "woodflr1_4")) {
			materialInfo = 1;
		}

		if (strstr(name, "city4_7")) {
			materialInfo = 1;
		}

		if (strstr(name, "edoor01_1")) {
			materialInfo = 1;
		}

		if (strstr(name, "sfloor4_2")) {
			materialInfo = 1;
		}

		if (strstr(name, "comp1_1")) {
			materialInfo = 1;
		}

		if (strstr(name, "comp1_2")) {
			materialInfo = 1;
		}

		if (strstr(name, "comp1_3")) {
			materialInfo = 1;
		}

		if (strstr(name, "comp1_6")) {
			materialInfo = 1;
		}

		if (strstr(name, "ecop1_6")) {
			materialInfo = 1;
		}

		if (strstr(name, "twall5_1")) {
			materialInfo = 1;
		}

		if (strstr(name, "ecop1_4")) {
			materialInfo = 1;
		}


		if(strstr(name, "lava")) {
			materialInfo = 2;
		}

		if (strstr(name, "light")) {
			materialInfo = 2;
		}

		GL_FindMegaTile(name, x, y, w, h);

		BuildSurfaceDisplayList(fa);

		if(fa->bmodelindex > 0 && currentmodel->name[0] != '*') {
			continue;
		}

		if (fa->flags & SURF_DRAWSKY) {
			continue;
		}

		surf.startVertex = mesh->meshVertexes.size();
		surf.numVertexes = 0;
		for (p = fa->polys; p; p = p->next) {
			for (int d = 0; d < p->numverts; d++) {
				dxrVertex_t v;
		
				v.xyz[0] = p->verts[d][0];
				v.xyz[1] = p->verts[d][1];
				v.xyz[2] = p->verts[d][2];
				v.st[0] = p->verts[d][3];
				v.st[1] = p->verts[d][4];
				v.st[2] = materialInfo;
				v.vtinfo[0] = x;
				v.vtinfo[1] = y;
				v.vtinfo[2] = w;
				v.vtinfo[3] = h;
		
				mesh->meshVertexes.push_back(v);
				surf.numVertexes++;
			}
		}

		surf.numIndexes = 0;
		surf.startIndex = mesh->meshIndexes.size();
		for (int d = 0; d < surf.numVertexes - 1; d++)
		{
			mesh->meshIndexes.push_back(surf.startVertex + 0);
			mesh->meshIndexes.push_back(surf.startVertex + d + 1);
			mesh->meshIndexes.push_back(surf.startVertex + d);
			surf.numIndexes+=3;
		}

		mesh->meshSurfaces.push_back(surf);
	}

	// TODO: Use a index buffer here : )
	{
		for (int i = 0; i < mesh->meshSurfaces.size(); i++)
		{
			mesh->meshSurfaces[i].startMegaVertex = mesh->meshTriVertexes.size();

			for (int d = 0; d < mesh->meshSurfaces[i].numIndexes; d++)
			{
				int indexId = mesh->meshSurfaces[i].startIndex + d;
				int idx = mesh->meshIndexes[indexId];

				mesh->meshTriVertexes.push_back(mesh->meshVertexes[idx]);
				sceneVertexes.push_back(mesh->meshVertexes[idx]);
				mesh->numSceneVertexes++;
			}
		}
	}

	// Calculate the normals
	{
		for(int i = 0; i < mesh->numSceneVertexes; i+=3)
		{
			float* v0 = &sceneVertexes[mesh->startSceneVertex + i + 0].xyz[0];
			float* v1 = &sceneVertexes[mesh->startSceneVertex + i + 1].xyz[0];
			float* v2 = &sceneVertexes[mesh->startSceneVertex + i + 2].xyz[0];
	
			vec3_t e1, e2, normal;
			VectorSubtract(v1, v0, e1);
			VectorSubtract(v2, v0, e2);
			CrossProduct(e1, e2, normal);
			VectorNormalize(normal);
	
			memcpy(sceneVertexes[mesh->startSceneVertex + i + 0].normal, normal, sizeof(float) * 3);
			memcpy(sceneVertexes[mesh->startSceneVertex + i + 1].normal, normal, sizeof(float) * 3);
			memcpy(sceneVertexes[mesh->startSceneVertex + i + 2].normal, normal, sizeof(float) * 3);
		}
	}
}

void *GL_LoadDXRMesh(msurface_t *surfaces, int numSurfaces)  {
	dxrMesh_t* mesh = new dxrMesh_t();
	
	//mesh->meshId = dxrMeshList.size();
	
	GL_LoadBottomLevelAccelStruct(mesh, surfaces, numSurfaces);

	dxrMeshList.push_back(mesh);

	return mesh;
}

void GL_AliasVertexToDxrVertex(trivertx_t inVert, stvert_t stvert, dxrVertex_t &vertex, float x, float y, float w, float h, int facesFront) {
	memset(&vertex, 0, sizeof(dxrVertex_t));
	vertex.xyz[0] = inVert.v[0];
	vertex.xyz[1] = inVert.v[1];
	vertex.xyz[2] = inVert.v[2];
	vertex.st[0] = stvert.s;
	vertex.st[1] = stvert.t;
	vertex.st[2] = -1;

	if(stvert.onseam && !facesFront) {
		vertex.st[0] += w * 0.5f; // backface.
	}

	vertex.st[0] = (vertex.st[0] + 0.5) / w;
	vertex.st[1] = (vertex.st[1] + 0.5) / h;

	//assert(vertex.st[0] > 1 || vertex.st[1] > 0);
	//if(vertex.st[0] > 1 || vertex.st[1] > 1 || w == -1 || h == -1) {
	//	vertex.vtinfo[0] = -1;
	//	vertex.vtinfo[1] = -1;
	//	vertex.vtinfo[2] = -1;
	//	vertex.vtinfo[3] = -1;
	//}
	//else
	{
		vertex.vtinfo[0] = x;
		vertex.vtinfo[1] = y;
		vertex.vtinfo[2] = w;
		vertex.vtinfo[3] = h;
	}
}

void *GL_LoadDXRAliasMesh(const char* name, int numVertexes, trivertx_t* vertexes, int numTris, mtriangle_t* triangles, stvert_t* stverts) {
	dxrMesh_t* mesh = new dxrMesh_t();
	
	//mesh->meshId = dxrMeshList.size();
	mesh->startSceneVertex = sceneVertexes.size();
	mesh->numSceneVertexes = 0;

	float x, y, w, h;
	char textureName[512];
	char textureSkinName[512];
	COM_StripExtension(COM_SkipPath((char*)name), textureName, sizeof(textureName));
	sprintf(textureSkinName, "%s_skin0", textureName);
	GL_FindMegaTile(textureSkinName, x, y, w, h);
	
	// TODO: Use a index buffer here : )
	for (int d = 0; d < numTris; d++)
	{
		{
			dxrVertex_t v;
			GL_AliasVertexToDxrVertex(vertexes[triangles[d].vertindex[0]], stverts[triangles[d].vertindex[0]], v, x, y, w, h, triangles[d].facesfront);
			mesh->meshTriVertexes.push_back(v);
			sceneVertexes.push_back(v);
			mesh->numSceneVertexes++;
		}

		{
			dxrVertex_t v;
			GL_AliasVertexToDxrVertex(vertexes[triangles[d].vertindex[1]], stverts[triangles[d].vertindex[1]], v, x, y, w, h, triangles[d].facesfront);
			mesh->meshTriVertexes.push_back(v);
			sceneVertexes.push_back(v);
			mesh->numSceneVertexes++;
		}

		{
			dxrVertex_t v;
			GL_AliasVertexToDxrVertex(vertexes[triangles[d].vertindex[2]], stverts[triangles[d].vertindex[2]], v, x, y, w, h, triangles[d].facesfront);
			mesh->meshTriVertexes.push_back(v);
			sceneVertexes.push_back(v);
			mesh->numSceneVertexes++;
		}
	}


	// Calculate the normals
	{
		for (int i = 0; i < mesh->numSceneVertexes; i += 3)
		{
			float* v0 = &sceneVertexes[mesh->startSceneVertex + i + 0].xyz[0];
			float* v1 = &sceneVertexes[mesh->startSceneVertex + i + 1].xyz[0];
			float* v2 = &sceneVertexes[mesh->startSceneVertex + i + 2].xyz[0];

			vec3_t e1, e2, normal;
			VectorSubtract(v1, v0, e1);
			VectorSubtract(v2, v0, e2);
			CrossProduct(e1, e2, normal);
			VectorNormalize(normal);

			memcpy(sceneVertexes[mesh->startSceneVertex + i + 0].normal, normal, sizeof(float) * 3);
			memcpy(sceneVertexes[mesh->startSceneVertex + i + 1].normal, normal, sizeof(float) * 3);
			memcpy(sceneVertexes[mesh->startSceneVertex + i + 2].normal, normal, sizeof(float) * 3);
		}
	}

	dxrMeshList.push_back(mesh);

	return mesh;
}

#if 0
/*
** LerpMeshVertexes
*/
static void LerpMeshVertexes(md3Surface_t* surf, float backlerp, int frame, int oldframe, dxrVertex_t* vertexes, float x, float y, float w, float h)
{
	short* oldXyz, * newXyz, * oldNormals, * newNormals;
	//float* outXyz, * outNormal;
	float	oldXyzScale, newXyzScale;
	float	oldNormalScale, newNormalScale;
	int		vertNum;
	unsigned lat, lng;
	int		numVerts;
	float* texCoords;

	//outXyz = tess.xyz[tess.numVertexes];
	//outNormal = tess.normal[tess.numVertexes];

	newXyz = (short*)((byte*)surf + surf->ofsXyzNormals)
		+ (frame * surf->numVerts * 4);
	newNormals = newXyz + 3;

	newXyzScale = MD3_XYZ_SCALE * (1.0 - backlerp);
	newNormalScale = 1.0 - backlerp;

	numVerts = surf->numVerts;
	texCoords = (float*)((byte*)surf + surf->ofsSt);
	//
	// just copy the vertexes
	//
	for (vertNum = 0; vertNum < numVerts; vertNum++,
		newXyz += 4, newNormals += 4, vertexes++, texCoords += 2)
	{

		vertexes->xyz[0] = newXyz[0] * newXyzScale;
		vertexes->xyz[1] = newXyz[1] * newXyzScale;
		vertexes->xyz[2] = newXyz[2] * newXyzScale;

		lat = (newNormals[0] >> 8) & 0xff;
		lng = (newNormals[0] & 0xff);
		lat *= (FUNCTABLE_SIZE / 256);
		lng *= (FUNCTABLE_SIZE / 256);

		// decode X as cos( lat ) * sin( long )
		// decode Y as sin( lat ) * sin( long )
		// decode Z as cos( long )

		vertexes->normal[0] = tr.sinTable[(lat + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK] * tr.sinTable[lng];
		vertexes->normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
		vertexes->normal[2] = tr.sinTable[(lng + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK];

		vertexes->st[0] = texCoords[0];
		vertexes->st[1] = texCoords[1];
		vertexes->st[2] = 0;

		vertexes->vtinfo[0] = x;
		vertexes->vtinfo[1] = y;
		vertexes->vtinfo[2] = w;
		vertexes->vtinfo[3] = h;
	}
	/*
		if (backlerp == 0) {
	#if idppc_altivec
			vector signed short newNormalsVec0;
			vector signed short newNormalsVec1;
			vector signed int newNormalsIntVec;
			vector float newNormalsFloatVec;
			vector float newXyzScaleVec;
			vector unsigned char newNormalsLoadPermute;
			vector unsigned char newNormalsStorePermute;
			vector float zero;

			newNormalsStorePermute = vec_lvsl(0, (float*)&newXyzScaleVec);
			newXyzScaleVec = *(vector float*) & newXyzScale;
			newXyzScaleVec = vec_perm(newXyzScaleVec, newXyzScaleVec, newNormalsStorePermute);
			newXyzScaleVec = vec_splat(newXyzScaleVec, 0);
			newNormalsLoadPermute = vec_lvsl(0, newXyz);
			newNormalsStorePermute = vec_lvsr(0, outXyz);
			zero = (vector float)vec_splat_s8(0);
			//
			// just copy the vertexes
			//
			for (vertNum = 0; vertNum < numVerts; vertNum++,
				newXyz += 4, newNormals += 4,
				outXyz += 4, outNormal += 4)
			{
				newNormalsLoadPermute = vec_lvsl(0, newXyz);
				newNormalsStorePermute = vec_lvsr(0, outXyz);
				newNormalsVec0 = vec_ld(0, newXyz);
				newNormalsVec1 = vec_ld(16, newXyz);
				newNormalsVec0 = vec_perm(newNormalsVec0, newNormalsVec1, newNormalsLoadPermute);
				newNormalsIntVec = vec_unpackh(newNormalsVec0);
				newNormalsFloatVec = vec_ctf(newNormalsIntVec, 0);
				newNormalsFloatVec = vec_madd(newNormalsFloatVec, newXyzScaleVec, zero);
				newNormalsFloatVec = vec_perm(newNormalsFloatVec, newNormalsFloatVec, newNormalsStorePermute);
				//outXyz[0] = newXyz[0] * newXyzScale;
				//outXyz[1] = newXyz[1] * newXyzScale;
				//outXyz[2] = newXyz[2] * newXyzScale;

				lat = (newNormals[0] >> 8) & 0xff;
				lng = (newNormals[0] & 0xff);
				lat *= (FUNCTABLE_SIZE / 256);
				lng *= (FUNCTABLE_SIZE / 256);

				// decode X as cos( lat ) * sin( long )
				// decode Y as sin( lat ) * sin( long )
				// decode Z as cos( long )

				outNormal[0] = tr.sinTable[(lat + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK] * tr.sinTable[lng];
				outNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
				outNormal[2] = tr.sinTable[(lng + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK];

				vec_ste(newNormalsFloatVec, 0, outXyz);
				vec_ste(newNormalsFloatVec, 4, outXyz);
				vec_ste(newNormalsFloatVec, 8, outXyz);
			}

	#else
			//
			// just copy the vertexes
			//
			for (vertNum = 0; vertNum < numVerts; vertNum++,
				newXyz += 4, newNormals += 4, vertexes++)
			{

				vertexes->xyz[0] = newXyz[0] * newXyzScale;
				vertexes->xyz[1] = newXyz[1] * newXyzScale;
				vertexes->xyz[2] = newXyz[2] * newXyzScale;

				lat = (newNormals[0] >> 8) & 0xff;
				lng = (newNormals[0] & 0xff);
				lat *= (FUNCTABLE_SIZE / 256);
				lng *= (FUNCTABLE_SIZE / 256);

				// decode X as cos( lat ) * sin( long )
				// decode Y as sin( lat ) * sin( long )
				// decode Z as cos( long )

				outNormal[0] = tr.sinTable[(lat + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK] * tr.sinTable[lng];
				outNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
				outNormal[2] = tr.sinTable[(lng + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK];
			}
	#endif
		}
		else {
			//
			// interpolate and copy the vertex and normal
			//
			oldXyz = (short*)((byte*)surf + surf->ofsXyzNormals)
				+ (oldframe * surf->numVerts * 4);
			oldNormals = oldXyz + 3;

			oldXyzScale = MD3_XYZ_SCALE * backlerp;
			oldNormalScale = backlerp;

			for (vertNum = 0; vertNum < numVerts; vertNum++,
				oldXyz += 4, newXyz += 4, oldNormals += 4, newNormals += 4,
				outXyz += 4, outNormal += 4)
			{
				vec3_t uncompressedOldNormal, uncompressedNewNormal;

				// interpolate the xyz
				outXyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
				outXyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
				outXyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

				// FIXME: interpolate lat/long instead?
				lat = (newNormals[0] >> 8) & 0xff;
				lng = (newNormals[0] & 0xff);
				lat *= 4;
				lng *= 4;
				uncompressedNewNormal[0] = tr.sinTable[(lat + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK] * tr.sinTable[lng];
				uncompressedNewNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
				uncompressedNewNormal[2] = tr.sinTable[(lng + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK];

				lat = (oldNormals[0] >> 8) & 0xff;
				lng = (oldNormals[0] & 0xff);
				lat *= 4;
				lng *= 4;

				uncompressedOldNormal[0] = tr.sinTable[(lat + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK] * tr.sinTable[lng];
				uncompressedOldNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
				uncompressedOldNormal[2] = tr.sinTable[(lng + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK];

				outNormal[0] = uncompressedOldNormal[0] * oldNormalScale + uncompressedNewNormal[0] * newNormalScale;
				outNormal[1] = uncompressedOldNormal[1] * oldNormalScale + uncompressedNewNormal[1] * newNormalScale;
				outNormal[2] = uncompressedOldNormal[2] * oldNormalScale + uncompressedNewNormal[2] * newNormalScale;

				//			VectorNormalize (outNormal);
			}
			VectorArrayNormalize((vec4_t*)tess.normal[tess.numVertexes], numVerts);
		}
	*/
}

void* GL_LoadMD3RaytracedMesh(md3Header_t* mod, int frame) {
	dxrMesh_t* mesh = new dxrMesh_t();

	//mesh->meshId = dxrMeshList.size();
	mesh->startSceneVertex = sceneVertexes.size();
	mesh->numSceneVertexes = 0;

	float x, y, w, h;
	char textureName[512];

	md3Surface_t* surf = (md3Surface_t*)((byte*)mod + mod->ofsSurfaces);

	for (int i = 0; i < mod->numSurfaces; i++) {
		md3Shader_t* shader = (md3Shader_t*)((byte*)surf + surf->ofsShaders);

		COM_StripExtension(COM_SkipPath((char*)shader->name), textureName);
		GL_FindMegaTile(textureName, &x, &y, &w, &h);

		int startVert = mesh->meshVertexes.size();
		dxrVertex_t* meshVertexes = new dxrVertex_t[surf->numVerts];

		LerpMeshVertexes(surf, 0.0f, frame, frame, meshVertexes, x, y, w, h);

		int indexes = surf->numTriangles * 3;
		int* triangles = (int*)((byte*)surf + surf->ofsTriangles);

		for (int j = 0; j < indexes; j++) {
			int tri = triangles[j];
			dxrVertex_t v = meshVertexes[tri];

			mesh->meshTriVertexes.push_back(v);
			sceneVertexes.push_back(v);
			mesh->numSceneVertexes++;
		}

		delete[] meshVertexes;

		// find the next surface
		surf = (md3Surface_t*)((byte*)surf + surf->ofsEnd);
	}

	dxrMeshList.push_back(mesh);

	return mesh;
}
#endif
void GL_FinishVertexBufferAllocation(void) {
//	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	// Create the vertex buffer.
	{
		const UINT vertexBufferSize = sizeof(dxrVertex_t) * sceneVertexes.size();

		// Note: using upload heaps to transfer static data like vert buffers is not 
		// recommended. Every time the GPU needs it, the upload heap will be marshalled 
		// over. Please read up on Default Heap usage. An upload heap is used here for 
		// code simplicity and because there are very few verts to actually transfer.
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_vertexBuffer)));

		// Copy the triangle data to the vertex buffer.
		UINT8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, &sceneVertexes[0], sizeof(dxrVertex_t) * sceneVertexes.size());
		m_vertexBuffer->Unmap(0, nullptr);

		// Initialize the vertex buffer view.
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(dxrVertex_t);
		m_vertexBufferView.SizeInBytes = vertexBufferSize;
	}

	for(int i = 0; i < dxrMeshList.size(); i++)
	{
		dxrMesh_t* mesh = dxrMeshList[i];

		nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS;
		bottomLevelAS.AddVertexBuffer(m_vertexBuffer.Get(), mesh->startSceneVertex * sizeof(dxrVertex_t), mesh->numSceneVertexes, sizeof(dxrVertex_t), NULL, 0);

		// Adding all vertex buffers and not transforming their position.
		//for (const auto& buffer : vVertexBuffers) {
		//	bottomLevelAS.AddVertexBuffer(buffer.first.Get(), 0, buffer.second,
		//		sizeof(Vertex), 0, 0);
		//}

		// The AS build requires some scratch space to store temporary information.
		// The amount of scratch memory is dependent on the scene complexity.
		UINT64 scratchSizeInBytes = 0;
		// The final AS also needs to be stored in addition to the existing vertex
		// buffers. It size is also dependent on the scene complexity.
		UINT64 resultSizeInBytes = 0;

		bottomLevelAS.ComputeASBufferSizes(m_device.Get(), false, &scratchSizeInBytes,
			&resultSizeInBytes);

		// Once the sizes are obtained, the application is responsible for allocating
		// the necessary buffers. Since the entire generation will be done on the GPU,
		// we can directly allocate those on the default heap	
		mesh->buffers.pScratch = nv_helpers_dx12::CreateBuffer(m_device.Get(), scratchSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, nv_helpers_dx12::kDefaultHeapProps);
		mesh->buffers.pResult = nv_helpers_dx12::CreateBuffer(m_device.Get(), resultSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nv_helpers_dx12::kDefaultHeapProps);

		// Build the acceleration structure. Note that this call integrates a barrier
		// on the generated AS, so that it can be used to compute a top-level AS right
		// after this method.

		bottomLevelAS.Generate(m_commandList.Get(), mesh->buffers.pScratch.Get(), mesh->buffers.pResult.Get(), false, nullptr);
	}

	// Flush the command list and wait for it to finish
	//m_commandList->Close();
	//ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	//m_commandQueue->ExecuteCommandLists(1, ppCommandLists);
	//m_fenceValue++;
	//m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
	//
	//m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
	//WaitForSingleObject(m_fenceEvent, INFINITE);
}