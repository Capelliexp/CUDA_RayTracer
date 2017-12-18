//--------------------------------------------------------------------------------------
// File: TemplateMain.cpp
//
// BTH-D3D-Template
//
// Copyright (c) Stefan Petersson 2013. All rights reserved.
//--------------------------------------------------------------------------------------
#include "stdafx.h"
#include "ComputeHelp.h"
#include "D3D11Timer.h"
#include <DirectXMath.h>
#include <DirectXTex.h>
#include <iostream>
#include <windows.h>
#include <time.h>

#if defined( DEBUG ) || defined( _DEBUG )
#pragma comment(lib, "DirectXTexD.lib")
#else
#pragma comment(lib, "DirectXTex.lib")
#endif

struct globals {
	DirectX::XMVECTOR CamPos;	//16
	DirectX::XMVECTOR CamDir;	//16
	int triAmount;				//4
	int width;					//4
	int height;					//4
	int padding1;				//4
};

struct triangle {
	DirectX::XMVECTOR p0;
	DirectX::XMVECTOR p1;
	DirectX::XMVECTOR p2;

	DirectX::XMVECTOR edge0;
	DirectX::XMVECTOR edge1;
	DirectX::XMVECTOR edge2;

	DirectX::XMVECTOR norm;
	DirectX::XMVECTOR color;
};

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE				g_hInst					= NULL;  
HWND					g_hWnd					= NULL;

IDXGISwapChain*         g_SwapChain				= NULL;
ID3D11Device*			g_Device				= NULL;
ID3D11DeviceContext*	g_DeviceContext			= NULL;

ID3D11UnorderedAccessView*  g_BackBufferUAV		= NULL;  // compute output

ComputeWrap*			g_ComputeSys			= NULL;
ComputeShader*			g_ComputeShader			= NULL;

D3D11Timer*				g_Timer					= NULL;

ID3D11Buffer* globalBuffer;
ComputeBuffer* TriangleBuffer = NULL;

#define NUM_TRIANGLES 10

globals myGlobals;
triangle triArray[NUM_TRIANGLES];

int g_Width, g_Height;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT             InitWindow( HINSTANCE hInstance, int nCmdShow );
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
HRESULT				Render(float deltaTime);
HRESULT				Update(float deltaTime);
void				GenerateTriangle(triangle* tri);

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

//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT Init()
{
	HRESULT hr = S_OK;;

	RECT rc;
	GetClientRect( g_hWnd, &rc );
	g_Width = rc.right - rc.left;;
	g_Height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverType;

	D3D_DRIVER_TYPE driverTypes[] = 
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = sizeof(driverTypes) / sizeof(driverTypes[0]);

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory( &sd, sizeof(sd) );
	sd.BufferCount = 1;
	sd.BufferDesc.Width = g_Width;
	sd.BufferDesc.Height = g_Height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
	sd.OutputWindow = g_hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	D3D_FEATURE_LEVEL featureLevelsToTry[] = {
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0
	};
	D3D_FEATURE_LEVEL initiatedFeatureLevel;

	for( UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++ )
	{
		driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDeviceAndSwapChain(
			NULL,
			driverType,
			NULL,
			createDeviceFlags,
			featureLevelsToTry,
			ARRAYSIZE(featureLevelsToTry),
			D3D11_SDK_VERSION,
			&sd,
			&g_SwapChain,
			&g_Device,
			&initiatedFeatureLevel,
			&g_DeviceContext);

		if( SUCCEEDED( hr ) )
		{
			char title[256];
			sprintf_s(
				title,
				sizeof(title),
				"BTH - Direct3D 11.0 Template | Direct3D 11.0 device initiated with Direct3D %s feature level",
				FeatureLevelToString(initiatedFeatureLevel)
			);
			SetWindowTextA(g_hWnd, title);

			break;
		}
	}
	if( FAILED(hr) )
		return hr;

	// Create a render target view
	ID3D11Texture2D* pBackBuffer;
	hr = g_SwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (LPVOID*)&pBackBuffer );
	if( FAILED(hr) )
		return hr;

	// create shader unordered access view on back buffer for compute shader to write into texture
	hr = g_Device->CreateUnorderedAccessView( pBackBuffer, NULL, &g_BackBufferUAV );

	//create helper sys and compute shader instance
	g_ComputeSys = new ComputeWrap(g_Device, g_DeviceContext);
	g_ComputeShader = g_ComputeSys->CreateComputeShader(_T("../Shaders/BasicCompute.fx"), NULL, "main", NULL);
	g_Timer = new D3D11Timer(g_Device, g_DeviceContext);


	return S_OK;
}

HRESULT Update(float deltaTime)
{
	return S_OK;
}

HRESULT CreateBuffers() {
	HRESULT blob;

	//CAMERA
	myGlobals.CamPos = { 0,0,0,0 };
	myGlobals.CamDir = { 0,0,1,0 };
	myGlobals.triAmount = NUM_TRIANGLES;
	myGlobals.width = 800;
	myGlobals.height = 800;

	D3D11_BUFFER_DESC GlobalsDesc;
	memset(&GlobalsDesc, 0, sizeof(GlobalsDesc));
	GlobalsDesc.ByteWidth = sizeof(globals);
	GlobalsDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	GlobalsDesc.Usage = D3D11_USAGE_DYNAMIC;
	GlobalsDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	GlobalsDesc.MiscFlags = 0;
	GlobalsDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA GlobalsDesc_data;
	GlobalsDesc_data.pSysMem = &myGlobals;
	GlobalsDesc_data.SysMemPitch = 0;
	GlobalsDesc_data.SysMemSlicePitch = 0;

	blob = g_Device->CreateBuffer(&GlobalsDesc, &GlobalsDesc_data, &globalBuffer);
	if (FAILED(blob))
		return blob;

	//TRIANGLE
	for(int i = 0; i < NUM_TRIANGLES; i++)
		GenerateTriangle(&triArray[i]);

	TriangleBuffer = g_ComputeSys->CreateBuffer(STRUCTURED_BUFFER, sizeof(triangle), NUM_TRIANGLES, true, false, triArray);

	return S_OK;
}

HRESULT Render(float deltaTime){
	ID3D11UnorderedAccessView* uav[] = { g_BackBufferUAV };
	g_DeviceContext->CSSetUnorderedAccessViews(0, 1, uav, NULL);

	ID3D11ShaderResourceView* srv[] = { TriangleBuffer->GetResourceView() };
	g_DeviceContext->CSSetShaderResources(0, 1, srv);

	g_DeviceContext->CSSetConstantBuffers(0, 1, &globalBuffer);

	g_ComputeShader->Set();
	g_Timer->Start();
	g_DeviceContext->Dispatch( 25, 25, 1 );
	g_Timer->Stop();
	g_ComputeShader->Unset();

	if(FAILED(g_SwapChain->Present( 0, 0 )))
		return E_FAIL;

	char title[256];
	sprintf_s(
		title,
		sizeof(title),
		"BTH - DirectCompute DEMO - Dispatch time: %f",
		g_Timer->GetTime()
	);
	SetWindowTextA(g_hWnd, title);

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow ){
	srand(time(NULL));

	if( FAILED( InitWindow( hInstance, nCmdShow ) ) )
		return 0;

	if( FAILED( Init() ) )
		return 0;

	__int64 cntsPerSec = 0;
	QueryPerformanceFrequency((LARGE_INTEGER*)&cntsPerSec);
	float secsPerCnt = 1.0f / (float)cntsPerSec;

	__int64 prevTimeStamp = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&prevTimeStamp);

	//vårt stuff
	CreateBuffers();

	// Main message loop
	MSG msg = {0};
	while(WM_QUIT != msg.message)
	{
		if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		else
		{
			__int64 currTimeStamp = 0;
			QueryPerformanceCounter((LARGE_INTEGER*)&currTimeStamp);
			float dt = (currTimeStamp - prevTimeStamp) * secsPerCnt;

			//render
			Update(dt);
			Render(dt);

			prevTimeStamp = currTimeStamp;
		}
	}

	return (int) msg.wParam;
}

//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow )
{
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX); 
	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = WndProc;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = hInstance;
	wcex.hIcon          = 0;
	wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName   = NULL;
	wcex.lpszClassName  = _T("BTH_D3D_Template");
	wcex.hIconSm        = 0;
	if( !RegisterClassEx(&wcex) )
		return E_FAIL;

	// Create window
	g_hInst = hInstance; 
	RECT rc = { 0, 0, 800, 800 };
	AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
	
	if(!(g_hWnd = CreateWindow(
							_T("BTH_D3D_Template"),
							_T("BTH - Direct3D 11.0 Template"),
							WS_OVERLAPPEDWINDOW,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							rc.right - rc.left,
							rc.bottom - rc.top,
							NULL,
							NULL,
							hInstance,
							NULL)))
	{
		return E_FAIL;
	}

	ShowWindow( g_hWnd, nCmdShow );

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message) 
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_KEYDOWN:

		switch(wParam)
		{
			case VK_ESCAPE:
				PostQuitMessage(0);
				break;
		}
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

void GenerateTriangle(triangle* tri) {
	float x1 = (int)rand()%10 + 1 + 3;
	float y1 = (int)rand()%10 + 1 - 10;
	float z1 = (int)rand()%10 + 1 + 20;
	tri->p0 = {x1,y1,z1,1};

	float x2 = x1 + (int)rand()%5;
	float y2 = y1 + (int)rand()%5;
	float z2 = z1 + (int)rand()%5;
	tri->p1 = {x2,y2,z2,1};

	float x3 = x1 + (int)rand()%5;
	float y3 = y1 + (int)rand()%5;
	float z3 = z1 + (int)rand()%5;
	tri->p2 = {x3,y3,z3,1};
	
	tri->edge0 = DirectX::XMVectorSubtract(tri->p0, tri->p1);
	tri->edge1 = DirectX::XMVectorSubtract(tri->p0, tri->p2);
	tri->edge2 = DirectX::XMVectorSubtract(tri->p1, tri->p2);

	tri->norm = DirectX::XMVector2Cross(tri->edge0, tri->edge1);
	//tri->norm = { 0,0,-1,0 };

	float red   = (int)rand()%256;
	float green = (int)rand()%256;
	float blue  = (int)rand()%256;
	tri->color = {red/255,green/255,blue/255,1};
}