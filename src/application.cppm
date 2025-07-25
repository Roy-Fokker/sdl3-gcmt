﻿module;

export module application;

import std;
import sdl;
import clock;
import io;
import camera;

namespace st = sdl::type;
using namespace std::literals;

export namespace gcmt
{
	constexpr auto WND_WIDTH  = 800u;
	constexpr auto WND_HEIGHT = 600u;
	constexpr auto WND_TITLE  = "Geometric Clipmap Terrain";

	constexpr auto SHADER_FORMAT = SDL_GPUShaderFormat{
#ifdef SPIRV
		SDL_GPU_SHADERFORMAT_SPIRV
#elifdef DXIL
		SDL_GPU_SHADERFORMAT_DXIL
#endif
	};

	class application
	{
	public:
		auto run() -> int
		{
			prepare_scene();

			clk.reset();
			while (not quit)
			{
				handle_sdl_events();

				update_state();

				draw();

				clk.tick();
			}

			std::println("Elapsed Time: {:.4f}s", clk.get_elapsed<clock::s>());

			return 0;
		}

	private:
		// functions to handle SDL Events and Inputs
		void handle_sdl_events();
		void handle_sdl_input();

		// Scene and Application State
		void prepare_scene();
		void update_state();

		// Show on screen
		void draw();

		// Structure to hold scene objects
		struct scene
		{
			SDL_FColor clear_color;
			st::gfx_pipeline_ptr basic_pipeline;
			st::gpu_buffer_ptr vertex_buffer;
			st::gpu_buffer_ptr index_buffer;
			uint32_t vertex_count;
			uint32_t index_count;

			st::gpu_texture_ptr depth_texture;

			struct uniform_data
			{
				glm::mat4 projection;
				glm::mat4 view;
			} mvp;
		};

		// Private members
		sdl::sdl_base sdl_o = {};                                                     // SDL base object
		st::window_ptr wnd  = sdl::make_window(WND_WIDTH, WND_HEIGHT, WND_TITLE, {}); // SDL window object
		st::gpu_ptr gpu     = sdl::make_gpu(wnd.get(), SHADER_FORMAT);                // SDL GPU object
		SDL_Event evt       = {};                                                     // SDL Event object
		scene scn           = {};                                                     // Project's Render context;

		camera cam = {};
		clock clk  = {};
		bool quit  = false;
	};
}

using namespace gcmt;
using namespace sdl;
using namespace sdl::type;

namespace
{
	struct vertex
	{
		glm::vec3 pos;
		glm::vec4 clr;
	};

	struct mesh
	{
		std::vector<vertex> vertices;
		std::vector<uint32_t> indices;
	};

	auto make_pipeline(SDL_GPUDevice *gpu, SDL_Window *wnd) -> gfx_pipeline_ptr
	{
		const auto vs_shdr = shader_builder{
			.shader_binary        = io::read_file("shaders/basic.vs_6_4.cso"),
			.stage                = shader_stage::vertex,
			.uniform_buffer_count = 1,
		};

		const auto ps_shdr = shader_builder{
			.shader_binary = io::read_file("shaders/basic.ps_6_4.cso"),
			.stage         = shader_stage::fragment,
		};

		using VA = SDL_GPUVertexAttribute;
		auto va  = std::array{
            VA{
			   .location    = 0,
			   .buffer_slot = 0,
			   .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
			   .offset      = 0,
            },
            VA{
			   .location    = 1,
			   .buffer_slot = 0,
			   .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
			   .offset      = sizeof(glm::vec3),
            },
		};

		using VBD = SDL_GPUVertexBufferDescription;
		auto vbd  = std::array{
            VBD{
			   .slot       = 0,
			   .pitch      = sizeof(vertex),
			   .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
            },
		};

		auto pl = gfx_pipeline_builder{
			.vertex_shader              = vs_shdr.build(gpu),
			.fragment_shader            = ps_shdr.build(gpu),
			.vertex_attributes          = va,
			.vertex_buffer_descriptions = vbd,
			.color_format               = SDL_GetGPUSwapchainTextureFormat(gpu, wnd),
			.enable_depth_stencil       = true,
			.raster                     = raster_type::back_ccw_wire,
			.blend                      = blend_type::opaque,
			.topology                   = topology_type::triangle_strip,
		};

		return pl.build(gpu);
	}

	auto make_mesh() -> mesh
	{
		constexpr auto x = 1.5f;
		constexpr auto y = 1.5f;

		return {
			.vertices = {
			  { { -x, -y, 1.f }, { 0.f, 1.f, 1.f, 1.f } }, // Bottom Left
			  { { +x, -y, 1.f }, { 0.f, 1.f, 0.f, 1.f } }, // Bottom Right
			  { { -x, +y, 1.f }, { 0.f, 0.f, 1.f, 1.f } }, // Top Left
			  { { +x, +y, 1.f }, { 1.f, 0.f, 0.f, 1.f } }, // Top Right
			},
			.indices = {
			  0,
			  1,
			  2,
			  3,
			},
		};
	}

	auto upload_mesh(SDL_GPUDevice *gpu, const mesh &msh)
	{
		auto vtx_bytes = io::as_byte_span(msh.vertices);
		auto idx_bytes = io::as_byte_span(msh.indices);

		auto vbo = make_gpu_buffer(gpu, SDL_GPU_BUFFERUSAGE_VERTEX, static_cast<uint32_t>(vtx_bytes.size()), "Vertex Buffer");
		upload_to_gpu(gpu, vbo.get(), vtx_bytes);

		auto ibo = make_gpu_buffer(gpu, SDL_GPU_BUFFERUSAGE_INDEX, static_cast<uint32_t>(idx_bytes.size()), "Index Buffer");
		upload_to_gpu(gpu, ibo.get(), idx_bytes);

		return std::pair{
			std::move(vbo),
			std::move(ibo)
		};
	}

	auto make_depth_texture(SDL_GPUDevice *gpu, SDL_Window *wnd) -> gpu_texture_ptr
	{
		auto w = 0;
		auto h = 0;
		SDL_GetWindowSizeInPixels(wnd, &w, &h);

		const auto texture_info = SDL_GPUTextureCreateInfo{
			.type                 = SDL_GPU_TEXTURETYPE_2D,
			.format               = get_gpu_supported_depth_stencil_format(gpu),
			.usage                = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
			.width                = static_cast<uint32_t>(w),
			.height               = static_cast<uint32_t>(h),
			.layer_count_or_depth = 1,
			.num_levels           = 1,
			.sample_count         = SDL_GPU_SAMPLECOUNT_1,
		};

		return make_gpu_texture(gpu, texture_info, "Depth Stencil Texture");
	}

	auto make_perspective() -> glm::mat4
	{
		constexpr auto fovy         = glm::radians(60.f);
		constexpr auto aspect_ratio = static_cast<float>(WND_WIDTH) / static_cast<float>(WND_HEIGHT);
		constexpr auto near_plane   = 0.1f;
		constexpr auto far_plane    = 10.f;

		return glm::perspective(fovy, aspect_ratio, near_plane, far_plane);
	}

	void place_camera(camera &cam)
	{
		cam.lookat(glm::vec3{ 0.0f, 0.0f, -2.0f },
		           glm::vec3{ 0.0f, 0.0f, 0.0f },
		           glm::vec3{ 0.0f, 1.0f, 0.0f });
	}
}

void application::handle_sdl_events()
{
	while (SDL_PollEvent(&evt))
	{
		switch (evt.type)
		{
		case SDL_EVENT_QUIT:
			quit = true;
			break;
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_MOUSE_MOTION:
		case SDL_EVENT_MOUSE_WHEEL:
			handle_sdl_input();
			break;
		default:
			break;
		}
	}
}

void application::handle_sdl_input()
{
	const auto dt         = static_cast<float>(clk.get_delta<clock::s>());
	const auto move_speed = 2.f * dt;
	const auto rot_speed  = glm::radians(10.0f) * dt;

	auto handle_keyboard = [&]([[maybe_unused]] const SDL_KeyboardEvent &key_evt) {
		auto cam_dir = glm::vec3{};

		switch (key_evt.scancode)
		{
		case SDL_SCANCODE_ESCAPE:
			quit = true;
			break;
		case SDL_SCANCODE_W:
			cam_dir.z = 1.f;
			break;
		case SDL_SCANCODE_S:
			cam_dir.z = -1.f;
			break;
		case SDL_SCANCODE_A:
			cam_dir.x = -1.f;
			break;
		case SDL_SCANCODE_D:
			cam_dir.x = 1.f;
			break;
		case SDL_SCANCODE_Q:
			cam_dir.y = -1.f;
			break;
		case SDL_SCANCODE_E:
			cam_dir.y = 1.f;
			break;
		default:
			break;
		}
		cam.translate(cam_dir * move_speed);
	};

	auto handle_mouse_motion = [&]([[maybe_unused]] const SDL_MouseMotionEvent &mouse_evt) {
		auto cam_rot = glm::vec3{};
		cam_rot.y    = -mouse_evt.xrel;
		cam_rot.x    = -mouse_evt.yrel;
		cam.rotate(cam_rot * rot_speed);
	};

	auto handle_mouse_wheel = [&]([[maybe_unused]] const SDL_MouseWheelEvent &wheel_evt) {

	};

	switch (evt.type)
	{
	case SDL_EVENT_KEY_DOWN:
		handle_keyboard(evt.key);
		break;
	case SDL_EVENT_MOUSE_MOTION:
		handle_mouse_motion(evt.motion);
		break;
	case SDL_EVENT_MOUSE_WHEEL:
		handle_mouse_wheel(evt.wheel);
		break;
	default:
		break;
	}
}

void application::prepare_scene()
{
	place_camera(cam);

	scn.clear_color    = { 0.2f, 0.2f, 0.4f, 1.0f };
	scn.basic_pipeline = make_pipeline(gpu.get(), wnd.get());

	auto sqr_msh     = make_mesh();
	scn.vertex_count = static_cast<uint32_t>(sqr_msh.vertices.size());
	scn.index_count  = static_cast<uint32_t>(sqr_msh.indices.size());

	std::tie(scn.vertex_buffer, scn.index_buffer) = upload_mesh(gpu.get(), sqr_msh);

	scn.depth_texture = make_depth_texture(gpu.get(), wnd.get());

	scn.mvp.projection = make_perspective();
	scn.mvp.view       = cam.get_view();
}

void application::update_state()
{
	scn.mvp.view = cam.get_view();
}

void application::draw()
{
	auto cmd_buf = SDL_AcquireGPUCommandBuffer(gpu.get());
	assert(cmd_buf != nullptr and "Failed to acquire command buffer.");

	// Push Uniform buffer
	const auto mvp_bytes = io::as_byte_span(scn.mvp);
	SDL_PushGPUVertexUniformData(cmd_buf, 0, mvp_bytes.data(), static_cast<uint32_t>(mvp_bytes.size()));

	const auto sc_img = sdl::next_swapchain_image(wnd.get(), cmd_buf);

	const auto color_target = SDL_GPUColorTargetInfo{
		.texture     = sc_img,
		.clear_color = scn.clear_color,
		.load_op     = SDL_GPU_LOADOP_CLEAR,
		.store_op    = SDL_GPU_STOREOP_STORE,
	};

	const auto depth_target = SDL_GPUDepthStencilTargetInfo{
		.texture          = scn.depth_texture.get(),
		.clear_depth      = 1.0f,
		.load_op          = SDL_GPU_LOADOP_CLEAR,
		.store_op         = SDL_GPU_STOREOP_STORE,
		.stencil_load_op  = SDL_GPU_LOADOP_CLEAR,
		.stencil_store_op = SDL_GPU_STOREOP_STORE,
		.cycle            = true,
		.clear_stencil    = 0,
	};

	{ // Render Pass
		auto render_pass = SDL_BeginGPURenderPass(cmd_buf, &color_target, 1, &depth_target);
		SDL_BindGPUGraphicsPipeline(render_pass, scn.basic_pipeline.get());

		const auto vertex_bindings = std::array{
			SDL_GPUBufferBinding{
			  .buffer = scn.vertex_buffer.get(),
			  .offset = 0,
			},
		};
		SDL_BindGPUVertexBuffers(render_pass, 0, vertex_bindings.data(), static_cast<uint32_t>(vertex_bindings.size()));

		const auto index_binding = SDL_GPUBufferBinding{
			.buffer = scn.index_buffer.get(),
			.offset = 0,
		};
		SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

		SDL_DrawGPUIndexedPrimitives(render_pass, scn.index_count, 1, 0, 0, 0);
		SDL_EndGPURenderPass(render_pass);
	}
	SDL_SubmitGPUCommandBuffer(cmd_buf);
}