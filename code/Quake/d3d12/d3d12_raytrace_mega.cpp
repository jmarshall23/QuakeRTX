// d3d12_raytrace_mega.cpp
//

#include "d3d12_local.h"
#include "../libs/xml/tinyxml2.h"

struct MegaEntry_t {
	char name[512];
	int x;
	int y;
	int w;
	int h;
};

struct MegaTexture_t {
	ComPtr<ID3D12Resource> textureUploadHeap;
	ComPtr<ID3D12Resource> texture2D;
};

std::vector<MegaEntry_t> megaEntries;
MegaTexture_t megaTexture;

extern ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;

void GL_LoadMegaTexture(D3D12_CPU_DESCRIPTOR_HANDLE& srvPtr) {
	const int width = 4096;
	const int height = 4096;

	byte* buffer = new byte[width * height * 4];
	FILE* f = fopen("id1/mega/mega.raw", "rb");
	fread(buffer, 1, width * height * 4, f);
	fclose(f);

	// Create the texture.
	{
		// Describe and create a Texture2D.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&megaTexture.texture2D)));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(megaTexture.texture2D.Get(), 0, 1);

		// Create the GPU upload buffer.
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&megaTexture.textureUploadHeap)));

		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = &buffer[0];
		textureData.RowPitch = width * 4;
		textureData.SlicePitch = textureData.RowPitch * height;

		UpdateSubresources(m_commandList.Get(), megaTexture.texture2D.Get(), megaTexture.textureUploadHeap.Get(), 0, 0, 1, &textureData);
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(megaTexture.texture2D.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		// Describe and create a SRV for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		m_device->CreateShaderResourceView(megaTexture.texture2D.Get(), &srvDesc, srvPtr);
	}
}

void GL_FindMegaTile(const char *name, float &x, float &y, float &width, float &height)
{
	for (int i = 0; i < megaEntries.size(); i++) {
		if (!strcmp(megaEntries[i].name, name)) {
			x = megaEntries[i].x;
			y = megaEntries[i].y;
			width = megaEntries[i].w;
			height = megaEntries[i].h;
			return;
		}
	}

	for(int i = 0; i < megaEntries.size(); i++) {
		if(strstr(megaEntries[i].name, name)) {
			x = megaEntries[i].x;
			y = megaEntries[i].y;
			width = megaEntries[i].w;
			height = megaEntries[i].h;
			return;
		}
	}
	x = -1;
	y = -1;
	width = -1;
	height = -1;
}

void Tileset_ParseTile(tinyxml2::XMLNode* tile) {
//	tinyxml2::XMLNode* KeyNode = tile->FirstChildElement("Key");
//	tinyxml2::XMLNode* NameNode = KeyNode->FirstChildElement("Name");
//	tinyxml2::XMLNode* ShapeNode = KeyNode->FirstChildElement("Shape");
//	tinyxml2::XMLNode* ValueNode = tile->FirstChildElement("Value");
//	tinyxml2::XMLNode* FramesNode = ValueNode->FirstChildElement("Frames");
//	tinyxml2::XMLNode* FrameNode = FramesNode->FirstChildElement("Frame");
//
//	tileRule.name = NameNode->FirstChild()->ToText()->Value();
//	tileRule.hash = generateHashValue(tileRule.name.c_str(), tileRule.name.size());
//	tileRule.shape = atoi(ShapeNode->FirstChild()->ToText()->Value());
//
//	while (FrameNode != NULL) {
//		tileRule.frames.push_back(FrameNode->FirstChild()->ToText()->Value());
//		FrameNode = FrameNode->NextSiblingElement("Frame");
//	}

	tinyxml2::XMLElement* elem = (tinyxml2::XMLElement*)tile;
	const tinyxml2::XMLAttribute* attribute = elem->FirstAttribute();

	MegaEntry_t entry;

	COM_StripExtension(attribute->Value(), entry.name, sizeof(entry.name));
	attribute = attribute->Next();
	entry.x = atoi(attribute->Value());
	attribute = attribute->Next();
	entry.y = atoi(attribute->Value());
	attribute = attribute->Next();
	entry.w = atoi(attribute->Value());
	attribute = attribute->Next();
	entry.h = atoi(attribute->Value());
	megaEntries.push_back(entry);
}

void GL_LoadMegaXML(const char *path) {
	tinyxml2::XMLDocument doc;
	doc.LoadFile(path);

	Con_Printf("Loading Mega XML %s...\n");

	tinyxml2::XMLElement* root = doc.FirstChildElement();
	if (root == NULL) {
		Sys_Error("Failed to load mega XML");
		return;
	}

	tinyxml2::XMLNode* tilesetTypeClassNode = root->FirstChild();
	tinyxml2::XMLNode* tile = tilesetTypeClassNode;
	while (tile != NULL) {
		Tileset_ParseTile(tile);
		tile = tile->NextSiblingElement("sprite");
	}

	Con_Printf("%d mega entries...\n", megaEntries.size());
}