// d3d12_main.cpp
//

#include "d3d12_local.h"
#include "nv_helpers_dx12/RaytracingPipelineGenerator.h"
#include "nv_helpers_dx12/RootSignatureGenerator.h"
#include "nv_helpers_dx12/BottomLevelASGenerator.h"
#include "nv_helpers_dx12/TopLevelASGenerator.h"
#include "nv_helpers_dx12/ShaderBindingTableGenerator.h"
#include "../math/vectormath.h"

int m_frameIndex = 0;


float sky_map_x, sky_map_y, sky_map_w, sky_map_h;

tr_renderer *renderer;

ComPtr<IDXGISwapChain3> m_swapChain;
ComPtr<ID3D12Device5> m_device;
ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
ComPtr<ID3D12CommandAllocator> m_commandAllocator;
ComPtr<ID3D12CommandQueue> m_commandQueue;
ComPtr<ID3D12RootSignature> m_rootSignature;
ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
ComPtr<ID3D12PipelineState> m_pipelineState;
ComPtr<ID3D12GraphicsCommandList4> m_commandList;
UINT m_rtvDescriptorSize;

// Synchronization objects.
HANDLE m_fenceEvent;
ComPtr<ID3D12Fence> m_fence;
UINT64 m_fenceValue;

ComPtr<IDxcBlob> m_rayGenLibrary;
ComPtr<IDxcBlob> m_hitLibrary;
ComPtr<IDxcBlob> m_missLibrary;
ComPtr<IDxcBlob> m_shadowLibrary;

ComPtr<ID3D12RootSignature> m_rayGenSignature;
ComPtr<ID3D12RootSignature> m_hitSignature;
ComPtr<ID3D12RootSignature> m_shadowSignature;
ComPtr<ID3D12RootSignature> m_missSignature;

// Ray tracing pipeline state
ComPtr<ID3D12StateObject> m_rtStateObject;

//ComPtr<ID3D12Resource> m_outputResource;
//ComPtr<ID3D12Resource> m_outputLightResource;
tr_texture* albedoTexture;
tr_texture* lightTexture;
tr_texture* uiTexture;
tr_texture* compositeTexture;
tr_texture* compositeStagingTexture;
ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;

// Ray tracing pipeline state properties, retaining the shader identifiers
// to use in the Shader Binding Table
ComPtr<ID3D12StateObjectProperties> m_rtStateObjectProps;

extern AccelerationStructureBuffers m_topLevelASBuffers;

nv_helpers_dx12::ShaderBindingTableGenerator m_sbtHelper;
ComPtr<ID3D12Resource> m_sbtStorage;
ComPtr< ID3D12Resource > m_cameraBuffer;
ComPtr< ID3D12DescriptorHeap > m_constHeap;
uint32_t m_cameraBufferSize = 0;

byte* uiTextureBuffer = nullptr;

void GL_WaitForPreviousFrame(void) 
{
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
	// sample illustrates how to use fences for efficient resource usage and to
	// maximize GPU utilization.

	// Signal and increment the fence value.
	const UINT64 fence = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
	m_fenceValue++;

	// Wait until the previous frame is finished.
	if (m_fence->GetCompletedValue() < fence)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

/*
===============
GL_InitRaytracing
===============
*/
void GL_InitRaytracing(int width, int height) {
	Con_Printf("------ GL_InitRaytracing -------\n");

	{
		nv_helpers_dx12::RootSignatureGenerator rsc;
		rsc.AddHeapRangesParameter(
			{ {0 /*u0*/, 1 /*1 descriptor */, 0 /*use the implicit register space 0*/,
			  D3D12_DESCRIPTOR_RANGE_TYPE_UAV /* UAV representing the output buffer*/,
			  0 /*heap slot where the UAV is defined*/},
			  {1 /*u1*/, 1 /*1 descriptor */, 0 /*use the implicit register space 0*/,
			  D3D12_DESCRIPTOR_RANGE_TYPE_UAV /* UAV representing the output buffer*/,
			  1 /*heap slot where the UAV is defined*/},
			 {0 /*t0*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*Top-level acceleration structure*/, 2},
			 {0 /*b0*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV /*Camera parameters*/, 3},			
			 {1 /*t1*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*megatexture*/, 4}
			});

		m_rayGenSignature = rsc.Generate(m_device.Get(), true);
	}

	{
		nv_helpers_dx12::RootSignatureGenerator rsc;
		m_missSignature = rsc.Generate(m_device.Get(), true);
	}

	{
		nv_helpers_dx12::RootSignatureGenerator rsc;
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0);
		rsc.AddHeapRangesParameter(
			{ {1 /*t1*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*megatexture*/, 4},
			  {2 /*t2*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*megatexture*/, 5},
			  {3 /*t3*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*Top-level acceleration structure*/, 2},
			  {4 /*t4*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*Top-level acceleration structure*/, 6},
			});
		m_hitSignature = rsc.Generate(m_device.Get(), true);
	}

	{
		nv_helpers_dx12::RootSignatureGenerator rsc;
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0);
		rsc.AddHeapRangesParameter(
			{ {1 /*t1*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*megatexture*/, 4},
			  {2 /*t2*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*megatexture*/, 5},
			  {3 /*t3*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*Top-level acceleration structure*/, 2},
			  {4 /*t4*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*Top-level acceleration structure*/, 6},
			});
		m_shadowSignature = rsc.Generate(m_device.Get(), true);
	}

	nv_helpers_dx12::RayTracingPipelineGenerator pipeline(m_device.Get());

	// The pipeline contains the DXIL code of all the shaders potentially executed
	// during the raytracing process. This section compiles the HLSL code into a
	// set of DXIL libraries. We chose to separate the code in several libraries
	// by semantic (ray generation, hit, miss) for clarity. Any code layout can be
	// used.
	m_rayGenLibrary = nv_helpers_dx12::CompileShaderLibrary(L"id1/shaders/RayGen.hlsl");
	m_missLibrary = nv_helpers_dx12::CompileShaderLibrary(L"id1/shaders/Miss.hlsl");
	m_hitLibrary = nv_helpers_dx12::CompileShaderLibrary(L"id1/shaders/Hit.hlsl");
	// #DXR Extra - Another ray type
	m_shadowLibrary = nv_helpers_dx12::CompileShaderLibrary(L"id1/shaders/ShadowRay.hlsl");
	pipeline.AddLibrary(m_shadowLibrary.Get(),{ L"ShadowClosestHit", L"ShadowMiss" });

	// In a way similar to DLLs, each library is associated with a number of
	// exported symbols. This
	// has to be done explicitly in the lines below. Note that a single library
	// can contain an arbitrary number of symbols, whose semantic is given in HLSL
	// using the [shader("xxx")] syntax
	pipeline.AddLibrary(m_rayGenLibrary.Get(), { L"RayGen" });
	pipeline.AddLibrary(m_missLibrary.Get(), { L"Miss" });
	pipeline.AddLibrary(m_hitLibrary.Get(), { L"ClosestHit" });

	// 3 different shaders can be invoked to obtain an intersection: an
	// intersection shader is called
	// when hitting the bounding box of non-triangular geometry. This is beyond
	// the scope of this tutorial. An any-hit shader is called on potential
	// intersections. This shader can, for example, perform alpha-testing and
	// discard some intersections. Finally, the closest-hit program is invoked on
	// the intersection point closest to the ray origin. Those 3 shaders are bound
	// together into a hit group.

	// Note that for triangular geometry the intersection shader is built-in. An
	// empty any-hit shader is also defined by default, so in our simple case each
	// hit group contains only the closest hit shader. Note that since the
	// exported symbols are defined above the shaders can be simply referred to by
	// name.

	// Hit group for the triangles, with a shader simply interpolating vertex
	// colors
	pipeline.AddHitGroup(L"HitGroup", L"ClosestHit");
	// Hit group for all geometry when hit by a shadow ray
	pipeline.AddHitGroup(L"ShadowHitGroup", L"ShadowClosestHit");

	// The following section associates the root signature to each shader. Note
	// that we can explicitly show that some shaders share the same root signature
	// (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
	// to as hit groups, meaning that the underlying intersection, any-hit and
	// closest-hit shaders share the same root signature.
	Con_Printf("Loading Raytracing Shaders...\n");
	pipeline.AddRootSignatureAssociation(m_rayGenSignature.Get(), { L"RayGen" });
	pipeline.AddRootSignatureAssociation(m_missSignature.Get(), { L"Miss" });
	pipeline.AddRootSignatureAssociation(m_hitSignature.Get(), { L"HitGroup" });
	// #DXR Extra - Another ray type
	pipeline.AddRootSignatureAssociation(m_shadowSignature.Get(),{ L"ShadowHitGroup" });
	// #DXR Extra - Another ray type
	pipeline.AddRootSignatureAssociation(m_missSignature.Get(), { L"Miss", L"ShadowMiss" });
	
	// The payload size defines the maximum size of the data carried by the rays,
	// ie. the the data
	// exchanged between shaders, such as the HitInfo structure in the HLSL code.
	// It is important to keep this value as low as possible as a too high value
	// would result in unnecessary memory consumption and cache trashing.
	pipeline.SetMaxPayloadSize(16 * sizeof(float)); // RGB + distance, lightcolor.

	// Upon hitting a surface, DXR can provide several attributes to the hit. In
	// our sample we just use the barycentric coordinates defined by the weights
	// u,v of the last two vertices of the triangle. The actual barycentrics can
	// be obtained using float3 barycentrics = float3(1.f-u-v, u, v);
	pipeline.SetMaxAttributeSize(2 * sizeof(float)); // barycentric coordinates

	// The raytracing process can shoot rays from existing hit points, resulting
	// in nested TraceRay calls. Our sample code traces only primary rays, which
	// then requires a trace depth of 1. Note that this recursion depth should be
	// kept to a minimum for best performance. Path tracing algorithms can be
	// easily flattened into a simple loop in the ray generation.
	pipeline.SetMaxRecursionDepth(2);

	// Compile the pipeline for execution on the GPU
	Con_Printf("Compiling Raytracing Pipeline...\n");
	m_rtStateObject = pipeline.Generate();

	// Cast the state object into a properties object, allowing to later access
	// the shader pointers by name
	ThrowIfFailed(m_rtStateObject->QueryInterface(IID_PPV_ARGS(&m_rtStateObjectProps)));


	Con_Printf("Creating Raytrace Output Buffer...\n");
	tr_create_texture_2d(renderer, width, height, tr_sample_count_1, tr_format_r8g8b8a8_unorm, 1, NULL, false, tr_texture_usage_sampled_image | tr_texture_usage_storage_image, &albedoTexture);
	tr_create_texture_2d(renderer, width, height, tr_sample_count_1, tr_format_r8g8b8a8_unorm, 1, NULL, false, tr_texture_usage_sampled_image | tr_texture_usage_storage_image, &lightTexture);
	tr_create_texture_2d(renderer, width, height, tr_sample_count_1, tr_format_r8g8b8a8_unorm, 1, NULL, false, tr_texture_usage_sampled_image | tr_texture_usage_storage_image, &compositeTexture);
	tr_create_texture_2d(renderer, width, height, tr_sample_count_1, tr_format_r8g8b8a8_unorm, 1, NULL, false, tr_texture_usage_sampled_image | tr_texture_usage_storage_image, &compositeStagingTexture);
	tr_create_texture_2d(renderer, width, height, tr_sample_count_1, tr_format_r8g8b8a8_unorm, 1, NULL, true, tr_texture_usage_sampled_image | tr_texture_usage_storage_image, &uiTexture);

	uiTextureBuffer = new byte[width * height * 4];

	//{
	//	D3D12_RESOURCE_DESC resDesc = {};
	//	resDesc.DepthOrArraySize = 1;
	//	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	//	// The backbuffer is actually DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, but sRGB
	//	// formats cannot be used with UAVs. For accuracy we should convert to sRGB
	//	// ourselves in the shader
	//	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//
	//	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	//	resDesc.Width = width;
	//	resDesc.Height = height;
	//	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	//	resDesc.MipLevels = 1;
	//	resDesc.SampleDesc.Count = 1;
	//	ThrowIfFailed(m_device->CreateCommittedResource(
	//		&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
	//		D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr,
	//		IID_PPV_ARGS(&m_outputResource)));
	//}
	//{
	//	D3D12_RESOURCE_DESC resDesc = {};
	//	resDesc.DepthOrArraySize = 1;
	//	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	//	// The backbuffer is actually DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, but sRGB
	//	// formats cannot be used with UAVs. For accuracy we should convert to sRGB
	//	// ourselves in the shader
	//	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//
	//	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	//	resDesc.Width = width;
	//	resDesc.Height = height;
	//	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	//	resDesc.MipLevels = 1;
	//	resDesc.SampleDesc.Count = 1;
	//	ThrowIfFailed(m_device->CreateCommittedResource(
	//		&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
	//		D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr,
	//		IID_PPV_ARGS(&m_outputLightResource)));
	//}

	// Camera matrix stuff.
	{
		uint32_t nbMatrix = 4; // view, perspective, viewInv, perspectiveInv
		m_cameraBufferSize = nbMatrix * sizeof(DirectX::XMMATRIX);

		// Create the constant buffer for all matrices
		m_cameraBuffer = nv_helpers_dx12::CreateBuffer(
			m_device.Get(), m_cameraBufferSize, D3D12_RESOURCE_FLAG_NONE,
			D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

		// Create a descriptor heap that will be used by the rasterization shaders
		m_constHeap = nv_helpers_dx12::CreateDescriptorHeap(
			m_device.Get(), 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

		// Describe and create the constant buffer view.
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_cameraBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = m_cameraBufferSize;

		// Get a handle to the heap memory on the CPU side, to be able to write the
		// descriptors directly
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandle =
			m_constHeap->GetCPUDescriptorHandleForHeapStart();
		m_device->CreateConstantBufferView(&cbvDesc, srvHandle);
	}

	{
		// #DXR Extra: Perspective Camera
		// The root signature describes which data is accessed by the shader. The camera matrices are held
		// in a constant buffer, itself referenced the heap. To do this we reference a range in the heap,
		// and use that range as the sole parameter of the shader. The camera buffer is associated in the
		// index 0, making it accessible in the shader in the b0 register.
		CD3DX12_ROOT_PARAMETER constantParameter;
		CD3DX12_DESCRIPTOR_RANGE range;
		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		constantParameter.InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_ALL);

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(1, &constantParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3D12SerializeRootSignature(
			&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
		ThrowIfFailed(m_device->CreateRootSignature(
			0, signature->GetBufferPointer(), signature->GetBufferSize(),
			IID_PPV_ARGS(&m_rootSignature)));
	}

	//{
	//	ComPtr<ID3D12DeviceRemovedExtendedDataSettings> pDredSettings;
	//	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&pDredSettings)));
	//
	//	// Turn on AutoBreadcrumbs and Page Fault reporting
	//	pDredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
	//	pDredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
	//	//pDredSettings->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
	//	pDredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
	//}
}

/*
===============
GL_Init
===============
*/
void GL_Init(HWND hwnd, HINSTANCE hinstance, int width, int height)
{
	UINT dxgiFactoryFlags = 0;

	Con_Printf("------ GL_Init -------\n");

#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	//{
	//	ComPtr<ID3D12Debug> debugController;
	//	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	//	{
	//		debugController->EnableDebugLayer();
	//
	//		// Enable additional debug layers.
	//		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	//	}
	//}
#endif

	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	ComPtr<IDXGIAdapter1> hardwareAdapter;
	GetHardwareAdapter(factory.Get(), &hardwareAdapter);

	Con_Printf("Creating D3D12 device...\n");
	ThrowIfFailed(D3D12CreateDevice(
		hardwareAdapter.Get(),
		D3D_FEATURE_LEVEL_12_0,
		IID_PPV_ARGS(&m_device)
	));

	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		m_commandQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
	));

	// This sample does not support fullscreen transitions.
	ThrowIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// Create frame resources.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		for (UINT n = 0; n < FrameCount; n++)
		{
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);
		}
	}

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

	Con_Printf("Checking for raytracing support...\n");
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
	ThrowIfFailed(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,
		&options5, sizeof(options5)));
	if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
		Sys_Error("Your GPU doesn't support raytracing!\n");

	// Create the command list.
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

	// Command lists are created in the recording state, but there is nothing
	// to record yet. The main loop expects it to be closed, so close it now.
	ThrowIfFailed(m_commandList->Close());

	Con_Printf("Creating GPU fences...\n");
	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValue = 1;

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		GL_WaitForPreviousFrame();
	}

	// Create the tiny render pipeline that we use for everything except raytracing.
	{
		tr_renderer_settings settings = { 0 };
		settings.handle.hinstance = ::GetModuleHandle(NULL);
		settings.handle.hwnd = hwnd;
		settings.width = width;
		settings.height = height;
		settings.swapchain.image_count = 3;
		settings.swapchain.sample_count = tr_sample_count_8;
		settings.swapchain.color_format = tr_format_b8g8r8a8_unorm;
		settings.swapchain.depth_stencil_format = tr_format_undefined;

		tr_create_renderer("Darklight", &settings, &renderer, m_device.Get(), m_swapChain.Get());
	}

	GL_InitRaytracing(width, height);

	GL_InitCompositePass(albedoTexture, lightTexture, compositeStagingTexture, compositeTexture, uiTexture);

	GL_LoadMegaXML("id1/mega/mega.xml");

	DXGI_ADAPTER_DESC adapterDesc;	
    hardwareAdapter->GetDesc(&adapterDesc);

	char gl_vendor[512];

	sprintf(gl_vendor, "%ws", adapterDesc.Description);
	Con_Printf("GL_GPU: %s\n", gl_vendor);

	gl_renderer = "Direct3D 12";
	Con_Printf("GL_RENDERER: %s\n", gl_renderer);

	gl_version = "DX12";
	Con_Printf("GL_VERSION: %s\n", gl_version);
	gl_extensions = "";
	Con_Printf("GL_EXTENSIONS: %s\n", gl_extensions);
}



/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering(int* x, int* y, int* width, int* height)
{
	extern cvar_t gl_clear;

	GL_WaitForPreviousFrame();

	*x = *y = 0;
	*width = VID_GetCurrentWidth();
	*height = VID_GetCurrentHeight();

	CD3DX12_VIEWPORT m_viewport(0.0f, 0.0f, static_cast<float>(*width), static_cast<float>(*height));
	CD3DX12_RECT m_scissorRect(0, 0, static_cast<LONG>(*width), static_cast<LONG>(*height));

	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	ThrowIfFailed(m_commandAllocator->Reset());

	// However, when ExecuteCommandList() is called on a particular command
	// list, that command list can then be reset at any time and must be before
	// re-recording.
	ThrowIfFailed(
		m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	// Set necessary state.
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Indicate that the back buffer will be used as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		m_renderTargets[m_frameIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
		m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex,
		m_rtvDescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	memset(uiTextureBuffer, 0, sizeof(byte) * 4 * g_width * g_height);
}


void GL_EndRendering(void)
{
	//tr_util_update_texture_uint8(renderer->graphics_queue, g_width, g_height, g_width * 4, uiTextureBuffer, 4, uiTexture, NULL, NULL);	
	{
		{
			CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
				lightTexture->dx_resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			m_commandList->ResourceBarrier(1, &transition);
		}

		{
			CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
				albedoTexture->dx_resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			m_commandList->ResourceBarrier(1, &transition);
		}

		GL_CompositePass(albedoTexture, lightTexture, compositeStagingTexture, compositeTexture, m_commandList.Get(), m_commandAllocator.Get());

		{
			CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
				lightTexture->dx_resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_COPY_SOURCE);
			m_commandList->ResourceBarrier(1, &transition);
		}

		{
			CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
				albedoTexture->dx_resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_COPY_SOURCE);
			m_commandList->ResourceBarrier(1, &transition);
		}

		// The raytracing output needs to be copied to the actual render target used
		// for display. For this, we need to transition the raytracing output from a
		// UAV to a copy source, and the render target buffer to a copy destination.
		// We can then do the actual copy, before transitioning the render target
		// buffer into a render target, that will be then used to display the image
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
			compositeTexture->dx_resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_COPY_SOURCE);
		m_commandList->ResourceBarrier(1, &transition);
		transition = CD3DX12_RESOURCE_BARRIER::Transition(
			m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_COPY_DEST);
		m_commandList->ResourceBarrier(1, &transition);

		m_commandList->CopyResource(m_renderTargets[m_frameIndex].Get(),
			compositeTexture->dx_resource);

		transition = CD3DX12_RESOURCE_BARRIER::Transition(
			m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_commandList->ResourceBarrier(1, &transition);

		{
			CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
				compositeTexture->dx_resource, D3D12_RESOURCE_STATE_COPY_SOURCE,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			m_commandList->ResourceBarrier(1, &transition);
		}
	}


	// Indicate that the back buffer will now be used to present.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_commandList->Close());

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame.
	ThrowIfFailed(m_swapChain->Present(0, 0));

	uiTexture->dx_resource->WriteToSubresource(0, NULL, uiTextureBuffer, g_width * 4, 1);
}

void GL_Bind(int texnum)
{
	//if (currenttexture == texnum)
	//	return;
	//currenttexture = texnum;
	 
}

void GL_FinishDXRLoading(void) 
{
	GL_FindMegaTile("sky1", sky_map_x, sky_map_y, sky_map_w, sky_map_h);

	GL_FinishVertexBufferAllocation();

	{
	//	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));
		GL_CreateTopLevelAccelerationStructs(true);

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

	// Create a SRV/UAV/CBV descriptor heap. We need 2 entries - 1 UAV for the
	// raytracing output and 1 SRV for the TLAS
	m_srvUavHeap = nv_helpers_dx12::CreateDescriptorHeap( m_device.Get(), 7, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	// Get a handle to the heap memory on the CPU side, to be able to write the
	// descriptors directly
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle =
		m_srvUavHeap->GetCPUDescriptorHandleForHeapStart();

	// Create the UAV. Based on the root signature we created it is the first
	// entry. The Create*View methods write the view information directly into
	// srvHandle
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	m_device->CreateUnorderedAccessView(albedoTexture->dx_resource, nullptr, &uavDesc,
		srvHandle);

	// Add the Top Level AS SRV right after the raytracing output buffer
	srvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	m_device->CreateUnorderedAccessView(lightTexture->dx_resource, nullptr, &uavDesc,
		srvHandle);

	// Add the Top Level AS SRV right after the raytracing output buffer
	srvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = m_topLevelASBuffers.pResult->GetGPUVirtualAddress();
	// Write the acceleration structure view in the heap
	m_device->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);
	
	// #DXR Extra: Perspective Camera
	// Add the constant buffer for the camera after the TLAS
	srvHandle.ptr +=
		m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Describe and create a constant buffer view for the camera
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_cameraBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = m_cameraBufferSize;
	m_device->CreateConstantBufferView(&cbvDesc, srvHandle);

	srvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);	
	GL_LoadMegaTexture(srvHandle);
	srvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	GL_CreateInstanceInfo(srvHandle);
	srvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	GL_InitLightInfoBuffer(srvHandle);

	{
		// The SBT helper class collects calls to Add*Program.  If called several
		// times, the helper must be emptied before re-adding shaders.
		m_sbtHelper.Reset();

		// The pointer to the beginning of the heap is the only parameter required by
		// shaders without root parameters
		D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle =
			m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();

		// The helper treats both root parameter pointers and heap pointers as void*,
		// while DX12 uses the
		// D3D12_GPU_DESCRIPTOR_HANDLE to define heap pointers. The pointer in this
		// struct is a UINT64, which then has to be reinterpreted as a pointer.
		auto heapPointer = reinterpret_cast<UINT64*>(srvUavHeapHandle.ptr);

		// The ray generation only uses heap data
		m_sbtHelper.AddRayGenerationProgram(L"RayGen", { heapPointer });

		// The miss and hit shaders do not access any external resources: instead they
		// communicate their results through the ray payload
		m_sbtHelper.AddMissProgram(L"Miss", {});
		m_sbtHelper.AddMissProgram(L"ShadowMiss", {});

		// Adding the triangle hit shader
		m_sbtHelper.AddHitGroup(L"HitGroup", { (void*)m_vertexBuffer->GetGPUVirtualAddress(), (UINT64 *) m_srvUavHeap->GetGPUDescriptorHandleForHeapStart().ptr });
		m_sbtHelper.AddHitGroup(L"ShadowHitGroup", {});

		// Compute the size of the SBT given the number of shaders and their
  // parameters
		uint32_t sbtSize = m_sbtHelper.ComputeSBTSize();

		// Create the SBT on the upload heap. This is required as the helper will use
		// mapping to write the SBT contents. After the SBT compilation it could be
		// copied to the default heap for performance.
		m_sbtStorage = nv_helpers_dx12::CreateBuffer(
			m_device.Get(), sbtSize, D3D12_RESOURCE_FLAG_NONE,
			D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
		if (!m_sbtStorage) {
			throw std::logic_error("Could not allocate the shader binding table");
		}
		// Compile the SBT from the shader and parameters info
		m_sbtHelper.Generate(m_sbtStorage.Get(), m_rtStateObjectProps.Get());
	}
}

void GL_CalcFov(float base_fov, float& fov_x, float& fov_y) {
	float	x;
	float	y;
	float	ratio_x;
	float	ratio_y;

	// first, calculate the vertical fov based on a 640x480 view
	x = 640.0f / tan(base_fov / 360.0f * M_PI);
	y = atan2(480.0f, x);
	fov_y = y * 360.0f / M_PI;

	// 16:9
	ratio_x = 16.0f;
	ratio_y = 9.0f;

	y = ratio_y / tan(fov_y / 360.0f * M_PI);
	fov_x = atan2(ratio_x, y) * 360.0f / M_PI;

	if (fov_x < base_fov) {
		fov_x = base_fov;
		x = ratio_x / tan(fov_x / 360.0f * M_PI);
		fov_y = atan2(ratio_y, x) * 360.0f / M_PI;
	}	
}

void GL_Render(float x, float y, float z, float* viewAngles)
{
	std::vector<DirectX::XMMATRIX> matrices(4);

	GL_BuildLightList(x, y, z);

	// Update the top level acceleration structs based on new scene data. 
	GL_CreateTopLevelAccelerationStructs(false);

	// Initialize the view matrix, ideally this should be based on user
	// interactions The lookat and perspective matrices used for rasterization are
	// defined to transform world-space vertices into a [0,1]x[0,1]x[0,1] camera
	// space
	//DirectX::XMVECTOR Eye = DirectX::XMVectorSet(x, y, z, 0.0f);
	//DirectX::XMVECTOR At = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	//DirectX::XMVECTOR Up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	//matrices[0] = DirectX::XMMatrixLookAtRH(Eye, At, Up);
	
	float vieworg[3] = { x, y, z };
	create_view_matrix((float*)&matrices[0], vieworg, viewAngles);

	//float m_aspectRatio = g_width / g_height;
	//float fovAngleY = 45.0f * DirectX::XM_PI / 180.0f;
	//matrices[1] =
	//	DirectX::XMMatrixPerspectiveFovRH(fovAngleY, m_aspectRatio, 0.1f, 1000.0f);
	float fov_x, fov_y;
	GL_CalcFov(scr_fov.value, fov_x, fov_y);
	create_projection_matrix((float *)&matrices[1], 0.1, 1000.0f, fov_x, fov_y);

	// Raytracing has to do the contrary of rasterization: rays are defined in
	// camera space, and are transformed into world space. To do this, we need to
	// store the inverse matrices as well.
	DirectX::XMVECTOR det;
	matrices[2] = XMMatrixInverse(&det, matrices[0]);
	matrices[3] = XMMatrixInverse(&det, matrices[1]);

	static float fakeFrameTime = 0;
	fakeFrameTime += 1.0f;
	//matrices[0][0] = fakeFrameTime;
	float* frameData = (float *)&matrices[0];
	frameData[0] = fakeFrameTime;
	frameData[1] = vieworg[0];
	frameData[2] = vieworg[1];
	frameData[3] = vieworg[2];
	frameData[4] = sky_map_x;
	frameData[5] = sky_map_y;
	frameData[6] = sky_map_w;
	frameData[7] = sky_map_h;


	// Copy the matrix contents
	uint8_t* pData;
	ThrowIfFailed(m_cameraBuffer->Map(0, nullptr, (void**)&pData));
	memcpy(pData, matrices.data(), m_cameraBufferSize);
	m_cameraBuffer->Unmap(0, nullptr);

	// #DXR
   // Bind the descriptor heap giving access to the top-level acceleration
   // structure, as well as the raytracing output
	std::vector<ID3D12DescriptorHeap*> heaps = { m_srvUavHeap.Get() };
	m_commandList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()),
		heaps.data());

	// On the last frame, the raytracing output was used as a copy source, to
	// copy its contents into the render target. Now we need to transition it to
	// a UAV so that the shaders can write in it.
	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
			albedoTexture->dx_resource, D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_commandList->ResourceBarrier(1, &transition);
	}

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
			lightTexture->dx_resource, D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_commandList->ResourceBarrier(1, &transition);
	}

	// Setup the raytracing task
	D3D12_DISPATCH_RAYS_DESC desc = {};
	// The layout of the SBT is as follows: ray generation shader, miss
	// shaders, hit groups. As described in the CreateShaderBindingTable method,
	// all SBT entries of a given type have the same size to allow a fixed
	// stride.

	// The ray generation shaders are always at the beginning of the SBT.
	uint32_t rayGenerationSectionSizeInBytes = m_sbtHelper.GetRayGenSectionSize();
	desc.RayGenerationShaderRecord.StartAddress = m_sbtStorage->GetGPUVirtualAddress();
	desc.RayGenerationShaderRecord.SizeInBytes = rayGenerationSectionSizeInBytes;

	// The miss shaders are in the second SBT section, right after the ray
	// generation shader. We have one miss shader for the camera rays and one
	// for the shadow rays, so this section has a size of 2*m_sbtEntrySize. We
	// also indicate the stride between the two miss shaders, which is the size
	// of a SBT entry
	uint32_t missSectionSizeInBytes = m_sbtHelper.GetMissSectionSize();
	desc.MissShaderTable.StartAddress = m_sbtStorage->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes;
	desc.MissShaderTable.SizeInBytes = missSectionSizeInBytes;
	desc.MissShaderTable.StrideInBytes = m_sbtHelper.GetMissEntrySize();

	// The hit groups section start after the miss shaders. In this sample we
	// have one 1 hit group for the triangle
	uint32_t hitGroupsSectionSize = m_sbtHelper.GetHitGroupSectionSize();
	desc.HitGroupTable.StartAddress = m_sbtStorage->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes + missSectionSizeInBytes;
	desc.HitGroupTable.SizeInBytes = hitGroupsSectionSize;
	desc.HitGroupTable.StrideInBytes = m_sbtHelper.GetHitGroupEntrySize();

	// Dimensions of the image to render, identical to a kernel launch dimension
	desc.Width = g_width;
	desc.Height = g_height;
	desc.Depth = 1;

	// Bind the raytracing pipeline
	m_commandList->SetPipelineState1(m_rtStateObject.Get());
	// Dispatch the rays and write to the raytracing output
	m_commandList->DispatchRays(&desc);
}