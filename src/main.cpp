/* Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define STB_IMAGE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION

#include "Window.h"
#include "Graphics.h"
#include "imgui/imgui.h"
#include "imgui/examples/imgui_impl_win32.h"
#include "imgui/examples/imgui_impl_dx12.h"

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

/**
 * Your ray tracing application!
 */
class DXRApplication 
{
public:
	
	void Init(ConfigInfo &config) 
	{		
		// Create a new window
		HRESULT hr = Window::Create(config.width, config.height, config.instance, window, L"Introduction to DXR");
		Utils::Validate(hr, L"Error: failed to create window!");

		d3d.width = config.width;
		d3d.height = config.height;

		// Load a model
		Utils::LoadModel(config.model, model, material);

		// Initialize the shader compiler
		D3DShaders::Init_Shader_Compiler(shaderCompiler);

		// Initialize D3D12
		D3D12::Create_Device(d3d);
		D3D12::Create_Command_Queue(d3d);
		D3D12::Create_Command_Allocator(d3d);
		D3D12::Create_Fence(d3d);		
		D3D12::Create_SwapChain(d3d, window);
		D3D12::Create_CommandList(d3d);
		D3D12::Reset_CommandList(d3d);

		// Create common resources
		D3DResources::Create_Descriptor_Heaps(d3d, resources);
		D3DResources::Create_BackBuffer_RTV(d3d, resources);
		D3DResources::Create_Samplers(d3d, resources);		
		D3DResources::Create_Vertex_Buffer(d3d, resources, model);
		D3DResources::Create_Index_Buffer(d3d, resources, model);
		D3DResources::Create_Texture(d3d, resources, material);
		D3DResources::Create_View_CB(d3d, resources);
		D3DResources::Create_Material_CB(d3d, resources, material);
		
		// Create DXR specific resources
		DXR::Create_Bottom_Level_AS(d3d, dxr, resources, model);
		DXR::Create_Top_Level_AS(d3d, dxr, resources);
		DXR::Create_DXR_Output(d3d, resources);
		DXR::Create_CBVSRVUAV_Heap(d3d, dxr, resources, model);	
		DXR::Create_RayGen_Program(d3d, dxr, shaderCompiler);
		DXR::Create_Miss_Program(d3d, dxr, shaderCompiler);
		DXR::Create_Closest_Hit_Program(d3d, dxr, shaderCompiler);
		DXR::Create_Pipeline_State_Object(d3d, dxr);
		DXR::Create_Shader_Table(d3d, dxr, resources);

		d3d.cmdList->Close();
		ID3D12CommandList* pGraphicsList = { d3d.cmdList };
		d3d.cmdQueue->ExecuteCommandLists(1, &pGraphicsList);

		D3D12::WaitForGPU(d3d);
		D3D12::Reset_CommandList(d3d);

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();

		// Setup Platform/Renderer bindings
		ImGui_ImplWin32_Init(window);
		const int NUM_FRAMES_IN_FLIGHT = 2;
		UINT handleIncrement = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += 7 * handleIncrement;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
		gpuHandle.ptr += 7 * handleIncrement;
		ImGui_ImplDX12_Init(d3d.device, NUM_FRAMES_IN_FLIGHT,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			cpuHandle, gpuHandle);
	}
	
	void Update() 
	{
		// Our state
		static bool show_demo_window = false;
		static bool show_another_window = false;
		static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

		// Start the Dear ImGui frame
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
		{
			static float f = 0.0f;
			static int counter = 0;

			ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

			ImGui::SliderInt("SPP", &resources.viewCBData.nSamples, 1, 4);

			ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
			ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
			ImGui::Checkbox("Another Window", &show_another_window);

			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
			ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

			if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
				counter++;
			ImGui::SameLine();
			ImGui::Text("counter = %d", counter);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::End();
		}

		// 3. Show another simple window.
		if (show_another_window)
		{
			ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
			ImGui::Text("Hello from another window!");
			if (ImGui::Button("Close Me"))
				show_another_window = false;
			ImGui::End();
		}

		D3DResources::Update_View_CB(d3d, resources);
	}

	void Render() 
	{		
		DXR::Build_Command_List(d3d, dxr, resources);

		D3D12_RESOURCE_BARRIER OutputBarrier = {};

		// Transition the back buffer to a copy destination
		OutputBarrier.Transition.pResource = d3d.backBuffer[d3d.frameIndex];
		OutputBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		OutputBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		OutputBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		// Wait for the transitions to complete
		d3d.cmdList->ResourceBarrier(1, &OutputBarrier);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = resources.rtvHeap->GetCPUDescriptorHandleForHeapStart();
		rtvHandle.ptr += (d3d.frameIndex * resources.rtvDescSize);

		d3d.cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, NULL);

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3d.cmdList);

		// Transition back buffer to present
		OutputBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		OutputBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

		// Wait for the transitions to complete
		d3d.cmdList->ResourceBarrier(1, &OutputBarrier);

		// Submit the command list and wait for the GPU to idle
		D3D12::Submit_CmdList(d3d);
		D3D12::WaitForGPU(d3d);

		D3D12::Present(d3d);
		D3D12::MoveToNextFrame(d3d);
		D3D12::Reset_CommandList(d3d);
	}

	void Cleanup() 
	{
		D3D12::WaitForGPU(d3d);
		CloseHandle(d3d.fenceEvent);

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		DXR::Destroy(dxr);
		D3DResources::Destroy(resources);		
		D3DShaders::Destroy(shaderCompiler);
		D3D12::Destroy(d3d);

		DestroyWindow(window);
	}
	
private:
	HWND window;
	Model model;
	Material material;

	DXRGlobal dxr = {};
	D3D12Global d3d = {};
	D3D12Resources resources = {};
	D3D12ShaderCompilerInfo shaderCompiler;
};

/**
 * Program entry point
 */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) 
{	
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	HRESULT hr = EXIT_SUCCESS;
	{
		MSG msg = { 0 };

		// Get the application configuration
		ConfigInfo config;
		hr = Utils::ParseCommandLine(lpCmdLine, config);
		if (hr != EXIT_SUCCESS) return hr;

		// Initialize
		DXRApplication app;
		app.Init(config);

		// Main loop
		while (WM_QUIT != msg.message) 
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			app.Update();
			app.Render();
		}

		app.Cleanup();
	}

#if defined _CRTDBG_MAP_ALLOC
	_CrtDumpMemoryLeaks();
#endif

	return hr;
}