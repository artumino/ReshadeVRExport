#include "pch.h"
#include "reshade.hpp"
#include <dxgi1_6.h>
#include <d3d11.h>
#include <d3d12.h>
#include <iostream>
#include <cstdint>
#include <array>
#include <sstream>

extern "C" __declspec(dllexport) const char* NAME = "VRExport";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "Export 3D back buffers to KatangaVR and Artum's VR Viewer.";


static IDXGIKeyedMutex* sharedTextureMutex;
static void* toShareTexture;
static void* sharedTexture;

void share_d3d11_texture(ID3D11Texture2D* texture, ID3D11Device* device)
{
	toShareTexture = texture;

	D3D11_TEXTURE2D_DESC texDesc;
	texture->GetDesc(&texDesc);
	texDesc.BindFlags |= D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

	if (sharedTexture != NULL) {
		((ID3D11Texture2D*)sharedTexture)->Release();
		sharedTexture = NULL;
	}

	ID3D11Texture2D* stereoSharedBuffer;
	HRESULT result = device->CreateTexture2D(&texDesc, NULL, &stereoSharedBuffer);

	if (FAILED(result)) {
		std::stringstream error;
		error << "Could not create shared texture" << uint64_t(result);
		reshade::log_message(1, error.str().c_str());
		return;
	}

	result = stereoSharedBuffer->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)&sharedTextureMutex);
	if (FAILED(result)) {
		std::stringstream error;
		error << "Could not get IDXGIKeyedMutex" << uint64_t(result);
		reshade::log_message(1, error.str().c_str());
		return;
	}

	HANDLE sharedHandle = nullptr;
	IDXGIResource* tempResource = NULL;
	result = stereoSharedBuffer->QueryInterface(__uuidof(IDXGIResource), (void**)&tempResource);

	if (FAILED(result)) {
		std::stringstream error;
		error << "Could not get IDXGIResource" << uint64_t(result);
		reshade::log_message(1, error.str().c_str());
		return;
	}

	result = tempResource->GetSharedHandle(&sharedHandle);

	if (FAILED(result)) {
		std::stringstream error;
		error << "Could not create shared handle " << uint64_t(result);
		reshade::log_message(1, error.str().c_str());
		return;
	}
	else {
		std::stringstream message;
		message << "Got shared handle " << sharedHandle;
		reshade::log_message(3, message.str().c_str());
	}

	HANDLE katangaFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(sharedHandle), L"Local\\KatangaMappedFile");
	if (katangaFile == NULL)
	{
		std::stringstream error;
		error << "Could not create file mapping object " << GetLastError();
		reshade::log_message(1, error.str().c_str());
		return;
	}


	HANDLE* sharedResourceHandle = (HANDLE*)MapViewOfFile(katangaFile,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		sizeof(sharedHandle));

	if (sharedResourceHandle == NULL)
	{
		std::stringstream error;
		error << "Could not create map view of file " << GetLastError();
		reshade::log_message(1, error.str().c_str());
		return;
	}

	*sharedResourceHandle = sharedHandle;
	tempResource->Release();
	sharedTexture = stereoSharedBuffer;
}

void share_d3d12_texture(ID3D12Resource* texture, ID3D12Device* device)
{
	toShareTexture = texture;

	D3D12_RESOURCE_DESC texDesc = texture->GetDesc();
	texDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	
	D3D12_HEAP_PROPERTIES texHeapProperties = D3D12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT };

	if (sharedTexture != NULL) {
		((ID3D12Resource*)sharedTexture)->Release();
		sharedTexture = NULL;
	}

	ID3D12Resource* stereoSharedBuffer;
	
	HRESULT result = device->CreateCommittedResource(
		&texHeapProperties,
		D3D12_HEAP_FLAG_SHARED,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&stereoSharedBuffer)
	);

	if (FAILED(result)) {
		std::stringstream error;
		error << "Could not create shared stream texture" << uint64_t(result);
		reshade::log_message(1, error.str().c_str());
		return;
	}

	HANDLE sharedHandle = nullptr;
	result = device->CreateSharedHandle(stereoSharedBuffer, nullptr, GENERIC_ALL, L"DX12VRStream", &sharedHandle);

	if (FAILED(result)) {
		std::stringstream error;
		error << "Could not create shared handle " << uint64_t(result);
		reshade::log_message(1, error.str().c_str());
		return;
	}
	else {
		std::stringstream message;
		message << "Got shared handle " << sharedHandle;
		reshade::log_message(3, message.str().c_str());
	}

	HANDLE katangaFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(sharedHandle), L"Local\\KatangaMappedFile");
	if (katangaFile == NULL)
	{
		std::stringstream error;
		error << "Could not create file mapping object " << GetLastError();
		reshade::log_message(1, error.str().c_str());
		return;
	}


	HANDLE* sharedResourceHandle = (HANDLE*)MapViewOfFile(katangaFile,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		sizeof(sharedHandle));

	if (sharedResourceHandle == NULL)
	{
		std::stringstream error;
		error << "Could not create map view of file " << GetLastError();
		reshade::log_message(1, error.str().c_str());
		return;
	}

	*sharedResourceHandle = sharedHandle;
	sharedTexture = stereoSharedBuffer;
}

void export_effects(reshade::api::effect_runtime* runtime)
{
	reshade::log_message(3, "Searching vr buffer...");
	auto vrTexture = runtime->find_texture_variable("3DToElse.fx", "V__texTOT");
	if (vrTexture == NULL) {
		vrTexture = runtime->find_texture_variable("SuperDepth3D_VR+.fx", "V__DoubleTex");
	}
	if (vrTexture != NULL)
	{
		reshade::log_message(3, "Found VR Buffer, sharing...");
		reshade::api::resource_view vr_view;
		reshade::api::resource_view srgb_vr_view;
		runtime->get_texture_binding(vrTexture, &vr_view, &srgb_vr_view);
		if (vr_view.handle != NULL || srgb_vr_view.handle != NULL) {
			reshade::api::resource texture = runtime->get_device()->get_resource_from_view(srgb_vr_view.handle != NULL ? srgb_vr_view : vr_view);
			switch (runtime->get_device()->get_api())
			{
			case reshade::api::device_api::d3d11:
				share_d3d11_texture(reinterpret_cast<ID3D11Texture2D*>(texture.handle), reinterpret_cast<ID3D11Device*>(runtime->get_device()->get_native()));
				break;
			case reshade::api::device_api::d3d12:
				share_d3d12_texture(reinterpret_cast<ID3D12Resource*>(texture.handle), reinterpret_cast<ID3D12Device*>(runtime->get_device()->get_native()));
				break;
			}
		}
	}
}

void add_copy_command(reshade::api::effect_runtime* runtime, reshade::api::command_list * cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb)
{
	if (toShareTexture != NULL && sharedTexture != NULL) {
		reshade::api::resource src;
		reshade::api::resource dst;
		src.handle = uint64_t(toShareTexture);
		dst.handle = uint64_t(sharedTexture);

		if(sharedTextureMutex != NULL)
			sharedTextureMutex->AcquireSync(0, INFINITE);

		runtime->get_command_queue()->get_immediate_command_list()->copy_resource(src, dst);
		runtime->get_command_queue()->flush_immediate_command_list();

		if(sharedTextureMutex != NULL)
			sharedTextureMutex->ReleaseSync(0);
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(export_effects);
		reshade::register_event<reshade::addon_event::reshade_finish_effects>(add_copy_command);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
