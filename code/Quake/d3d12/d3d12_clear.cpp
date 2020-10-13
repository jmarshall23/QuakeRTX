// d3d12_clear.cpp
//

#include "d3d12_local.h"
#include "nv_helpers_dx12/BottomLevelASGenerator.h"
#include "nv_helpers_dx12/TopLevelASGenerator.h"
#include <vector>

#define MAX_COMPOSITE_SECTIONS 20
tr_shader_program* g_compute_shader_clear = nullptr;
tr_descriptor_set* g_compute_desc_set_clear[MAX_COMPOSITE_SECTIONS];
tr_pipeline* g_compute_pipeline_clear[MAX_COMPOSITE_SECTIONS];


struct PostUniformBuffer_t {
	int xoffset;
	int yoffset;
};

static tr_buffer* m_uniform_buffer[20];

void GL_InitClearPass(tr_texture* lightPass) {
	Con_Printf("Init Composite Pass...\n");

	// Load the blur compute shader pass.
	{
		Con_Printf("Loading clear compute shader...\n");
		// Open and read the file
		std::ifstream shaderFile("id1/compute/clear.hlsl");
		if (shaderFile.good() == false)
		{
			throw std::logic_error("Cannot find shader file");
		}
		std::stringstream strStream;
		strStream << shaderFile.rdbuf();
		std::string sShader = strStream.str();

		tr_create_shader_program_compute(renderer, (uint32_t)sShader.size(), (LPBYTE)sShader.c_str(), "clear_main", &g_compute_shader_clear);

		for (int i = 0; i < 20; i++)
		{
			std::vector<tr_descriptor> descriptors(2);

			descriptors[0].type = tr_descriptor_type_texture_uav;
			descriptors[0].count = 1;
			descriptors[0].binding = 0;
			descriptors[0].shader_stages = tr_shader_stage_comp;

			descriptors[1].type = tr_descriptor_type_uniform_buffer_cbv;
			descriptors[1].count = 1;
			descriptors[1].binding = 1;
			descriptors[1].shader_stages = tr_shader_stage_comp;

			tr_create_descriptor_set(renderer, (uint32_t)descriptors.size(), descriptors.data(), &g_compute_desc_set_clear[i]);

			tr_pipeline_settings pipeline_settings = {};
			tr_create_compute_pipeline(renderer, g_compute_shader_clear, g_compute_desc_set_clear[i], &pipeline_settings, &g_compute_pipeline_clear[i]);

			tr_create_uniform_buffer(renderer, sizeof(PostUniformBuffer_t), true, &m_uniform_buffer[i]);


			// Update descriptor sets
			{
				// clear
				g_compute_desc_set_clear[i]->descriptors[0].textures[0] = lightPass;
				g_compute_desc_set_clear[i]->descriptors[1].uniform_buffers[0] = m_uniform_buffer[i];
				tr_update_descriptor_set(renderer, g_compute_desc_set_clear[i]); 
			}
		}
	}
}

void GL_ClearLightPass(tr_texture* lightPass, ID3D12GraphicsCommandList4* cmdList, ID3D12CommandAllocator* commandAllocator) {
	tr_cmd cmd = { };
	tr_cmd_pool pool = { };

	pool.dx_cmd_alloc = commandAllocator;
	pool.renderer = renderer;
	cmd.cmd_pool = &pool;
	cmd.dx_cmd_list = cmdList;


	int section = 0;
	int numhsection = ceil(((float)g_width) / 1024.0f);
	int numvsection = ceil(((float)g_height) / 1024.0f);
	for (int x = 0; x < numhsection; x++)
	{
		for (int y = 0; y < numvsection; y++)
		{
			PostUniformBuffer_t buffer;
			buffer.xoffset = x * 1024;
			buffer.yoffset = y * 1024;
			memcpy(m_uniform_buffer[section]->cpu_mapped_address, &buffer, sizeof(PostUniformBuffer_t));
			//tr_update_un

			// clear pass
			{
				tr_cmd_image_transition(&cmd, lightPass, tr_texture_usage_sampled_image, tr_texture_usage_storage_image);
				tr_cmd_bind_pipeline(&cmd, g_compute_pipeline_clear[section]);
				tr_cmd_bind_descriptor_sets(&cmd, g_compute_pipeline_clear[section], g_compute_desc_set_clear[section]);
				const int num_groups_x = 1;
				const int num_groups_y = g_height;
				const int num_groups_z = 1;
				tr_cmd_dispatch(&cmd, num_groups_x, num_groups_y, num_groups_z);
				tr_cmd_image_transition(&cmd, lightPass, tr_texture_usage_storage_image, tr_texture_usage_sampled_image);
			}


			section++;
		}
	}
}