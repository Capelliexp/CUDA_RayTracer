//--------------------------------------------------------------------------------------
// File: TemplateMain.cpp
//
// BTH-D3D-Template
//
// Copyright (c) Stefan Petersson 2013. All rights reserved.
//
// Note: For this demo complete/correct error managment is left out.
//--------------------------------------------------------------------------------------
#include "stdafx.h"
#include "ComputeHelp.h"
#include "D3D11Timer.h"
#include "../tinyobjloader/tiny_obj_loader.h"

#include <string>
#include <iostream>

using namespace std;

/*	DirectXTex library - for usage info, see http://directxtex.codeplex.com/
	
	Usage example (may not be the "correct" way, I just wrote it in a hurry):

	DirectX::ScratchImage img;
	DirectX::TexMetadata meta;
	ID3D11ShaderResourceView* srv = nullptr;
	if(SUCCEEDED(hr = DirectX::LoadFromDDSFile(_T("C:\\Program Files (x86)\\Microsoft DirectX SDK (June 2010)\\Samples\\Media\\Dwarf\\Armor.dds"), 0, &meta, img)))
	{
		//img loaded OK
		if(SUCCEEDED(hr = DirectX::CreateShaderResourceView(g_Device, img.GetImages(), img.GetImageCount(), meta, &srv)))
		{
			//srv created OK
		}
	}
*/
#include <DirectXTex.h>

#if defined( DEBUG ) || defined( _DEBUG )
#pragma comment(lib, "DirectXTexD.lib")
#else
#pragma comment(lib, "DirectXTex.lib")
#endif

/*
	convert D3D_FEATURE_LEVEL to string version
*/
char* FeatureLevelToString(D3D_FEATURE_LEVEL featureLevel)
{
	if(featureLevel == D3D_FEATURE_LEVEL_11_0)
		return "11.0";
	if(featureLevel == D3D_FEATURE_LEVEL_10_1)
		return "10.1";
	if(featureLevel == D3D_FEATURE_LEVEL_10_0)
		return "10.0";

	return "Unknown";
}

/*
	Create Direct3D device
*/
HRESULT InitD3D(ID3D11Device** ppDevice, ID3D11DeviceContext** ppDeviceContext, D3D_DRIVER_TYPE driverType)
{
	HRESULT hr = S_OK;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] = { driverType };
	D3D_FEATURE_LEVEL featureLevelsToTry[] = { D3D_FEATURE_LEVEL_11_0 };
	D3D_FEATURE_LEVEL initiatedFeatureLevel;

	for( UINT driverTypeIndex = 0; driverTypeIndex < ARRAYSIZE(driverTypes); driverTypeIndex++ )
	{
		hr = D3D11CreateDevice(
			NULL,
			driverTypes[driverTypeIndex],
			NULL,
			createDeviceFlags,
			featureLevelsToTry,
			ARRAYSIZE(featureLevelsToTry),
			D3D11_SDK_VERSION,
			ppDevice,
			&initiatedFeatureLevel,
			ppDeviceContext);

		if( SUCCEEDED( hr ) )
		{
			cout << "Direct3D 11.0 device initiated with Direct3D " << 
				FeatureLevelToString(initiatedFeatureLevel) << " feature level" << endl;

			break;
		}
	}
	if( FAILED(hr) )
		return hr;


	return S_OK;
}

//data used by shader
struct ComputationData
{ 
	unsigned int NumBlocksX;
	unsigned int NumBlocksY;
};

//thread group size
static const unsigned int NUM_THREADS_X = 2;
static const unsigned int NUM_THREADS_Y = 2;

int main()
{
	//d3d device objects
	ID3D11Device*			device = NULL;
	ID3D11DeviceContext*	deviceContext = NULL;

	//custom compute shader wrapper objects
	ComputeWrap*			computeSys = NULL;
	ComputeShader*			computeShader = NULL;
	ComputeBuffer*			outputBuffer = NULL;

	//d3d standard buffer, initiated using custom compute shader system
	ID3D11Buffer*			cbComputationData = NULL;

	//custom d3d timer, used to time async d3d calls
	D3D11Timer*				d3d_timer = NULL;

	if(SUCCEEDED(InitD3D(&device, &deviceContext, D3D_DRIVER_TYPE_HARDWARE))) //GPU
	//if(SUCCEEDED(InitD3D(&device, &deviceContext, D3D_DRIVER_TYPE_WARP))) //CPU
	{
		//create helper sys and compute shader instance
		computeSys = new ComputeWrap(device, deviceContext);

		//setup shader compile macros, int -> char* needed
		char thread_str_x[10], thread_str_y[10];
		sprintf_s(thread_str_x, sizeof(thread_str_x), "%d", NUM_THREADS_X);
		sprintf_s(thread_str_y, sizeof(thread_str_y), "%d", NUM_THREADS_Y);
		D3D_SHADER_MACRO shaderDefines[] = {
			{ "NUM_THREADS_X", thread_str_x },
			{ "NUM_THREADS_Y", thread_str_y },
			{ NULL, NULL}
		};

		//create/compile compute shader
		computeShader = computeSys->CreateComputeShader(_T("../Shaders/BasicCompute.fx"), NULL, "main", shaderDefines);

		//init d3d timer object
		d3d_timer = new D3D11Timer(device, deviceContext);

		//create constant buffer
		ComputationData computationData;
		computationData.NumBlocksX = 2;
		computationData.NumBlocksY = 2;
		cbComputationData = computeSys->CreateConstantBuffer(sizeof(ComputationData), &computationData, "cbComputationData");

		//create computation result buffer (one integer per thread)
		unsigned int OutputSizePerBlock = sizeof(int) * NUM_THREADS_X * NUM_THREADS_X;
		outputBuffer = computeSys->CreateBuffer(
									STRUCTURED_BUFFER,
									OutputSizePerBlock,
									computationData.NumBlocksX * computationData.NumBlocksY,
									false,
									true,
									NULL,
									true,
									"output_buffer"
									);


		//set constant buffer to compute shader stage (to register b0, see shader)
		ID3D11Buffer* constant_buffers[] = { cbComputationData };
		deviceContext->CSSetConstantBuffers(0, ARRAYSIZE(constant_buffers), constant_buffers);

		//set output/result buffer to compute shader stage (to register u0, see shader)
		ID3D11UnorderedAccessView* uav_buffers[] = { outputBuffer->GetUnorderedAccessView() };
		deviceContext->CSSetUnorderedAccessViews(0, ARRAYSIZE(uav_buffers), uav_buffers, NULL);

		//connect compute shader
		computeShader->Set();

		//start timer
		d3d_timer->Start();

		//compute, for info see http://msdn.microsoft.com/en-us/library/windows/desktop/ff476405(v=vs.85).aspx
		deviceContext->Dispatch(computationData.NumBlocksX, computationData.NumBlocksY, 1);

		//stop timer
		d3d_timer->Stop();

		//print computation time
		cout << "Compute time: " << d3d_timer->GetTime() << endl;

		//disconnect compute shader
		computeShader->Unset();

		//unset result/output buffer register
		memset(uav_buffers, 0, sizeof(uav_buffers));
		deviceContext->CSSetUnorderedAccessViews(0, ARRAYSIZE(uav_buffers), uav_buffers, NULL);

		//unset constant buffer register
		memset(constant_buffers, 0, sizeof(constant_buffers));
		deviceContext->CSSetConstantBuffers(0, ARRAYSIZE(constant_buffers), constant_buffers);

		//copy computation result to staging buffer so that we can read from cpu
		outputBuffer->CopyToStaging();

		//map/lock staging buffer
		int* result = outputBuffer->Map<int>();

		//print computation results
		int iterations = (computationData.NumBlocksX * NUM_THREADS_X) * (computationData.NumBlocksY * NUM_THREADS_Y);
		while(iterations-- > 0)
		{
			cout << *result++ << endl;
		}

		//unmap/unlock staging buffer
		outputBuffer->Unmap();

		//cleanup
		SAFE_RELEASE(cbComputationData);
		SAFE_DELETE(outputBuffer);
		SAFE_DELETE(computeShader);
		SAFE_DELETE(computeSys);

		SAFE_RELEASE(device);
		SAFE_RELEASE(deviceContext);

		getchar();
	}
	else
	{
		cout << "Failed to initiate Direct3D" << endl;
	}
}