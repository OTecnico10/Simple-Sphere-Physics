#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <WinUser.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>
#include <vector>
#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <time.h>
#include <thread>

HINSTANCE               g_hInst = NULL;
HWND                    g_hWnd = NULL;
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*           g_pd3dDevice = NULL;
ID3D11DeviceContext*    g_pImmediateContext = NULL;
IDXGISwapChain*         g_pSwapChain = NULL;
ID3D11RenderTargetView* g_pRenderTargetView = NULL;
ID3D11Texture2D*        g_pDepthStencil = NULL;
ID3D11DepthStencilView* g_pDepthStencilView = NULL;
ID3D11VertexShader*     g_pVertexShader = NULL;
ID3D11PixelShader*      g_pPixelShader = NULL;
ID3D11InputLayout*      g_pVertexLayout = NULL;
ID3D11Buffer*           g_pVertexBuffer = NULL;
ID3D11Buffer*           g_pIndexBuffer = NULL;
ID3D11Buffer*           g_pcBufferShader1 = NULL;
XMMATRIX                g_View;
XMMATRIX                g_Projection;

//this is just timing stuff
float prev_time = 0.0f;
float dt = 0.0f;

float g_Theta = 0.0f;	//Camera angle theta
float g_Phi = 0.0f;		//Camera angle phi
float g_R = 125.0f;		//Camera radius

int num_sphere_indices = 0; //number of indices for the sphere model

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
HRESULT CompileShaderFromFile(LPCSTR szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut); //Code from one of Microsoft's Directx 11 examples. It makes compiling shaders a lot easier

bool init(HINSTANCE hInstance, int nCmdShow); //initialize windows stuff
bool initDirectx(); //initialize Directx 11
void update(); //update everything
void render(); //render everything
void cleanup(); //cleanup

void handle_collisions(int start, int end); //we will use this for multithreading

void CreateSphere(float radius, UINT sliceCount, UINT stackCount, std::vector<XMFLOAT3>* Positions, std::vector<WORD>* Indices, std::vector<XMFLOAT2>* UVs); //this code is edited a little from Frank D. Luna's book
																																							 //it makes creating a sphere model easier

struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT3 Norm;
};

struct cBufferShader1
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
	XMMATRIX mWorldInvTrans;
	XMFLOAT4 color;
	XMFLOAT3 LightDir;
	XMFLOAT3 EyePos;
	XMFLOAT3 EyeDir;
};

class Sphere
{
public:
	Sphere(XMFLOAT3 position, XMFLOAT4 color, float radius, float mass);
	void Update(float dt);
	void ApplyForce(XMFLOAT3 force, float dt);
	XMFLOAT3 Position;
	XMFLOAT3 Velocity;
	XMFLOAT4 Color;
	float Radius;
	float Mass;
	float dampen_time; //this is to keep the rate at which collision energy is lost constant. This isn't a very physically accurate simulation, so we can cheat a little here
};

Sphere::Sphere(XMFLOAT3 position, XMFLOAT4 color, float radius, float mass)
{
	Position = position;
	Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	Color = color;
	Radius = radius;
	Mass = mass;
	dampen_time = 0.0f;
}

void Sphere::Update(float dt)
{
	Position.x += Velocity.x * dt;
	Position.y += Velocity.y * dt;
	Position.z += Velocity.z * dt;

	dampen_time += dt;
}

void Sphere::ApplyForce(XMFLOAT3 force, float dt)
{
	Velocity.x += force.x / Mass * dt;
	Velocity.y += force.y / Mass * dt;
	Velocity.z += force.z / Mass * dt;
}

std::vector<Sphere> Spheres;

void CreateSphere(float radius, UINT sliceCount, UINT stackCount, std::vector<XMFLOAT3>* Positions, std::vector<WORD>* Indices, std::vector<XMFLOAT2>* UVs) //From Frank D. Luna's book but edited a little bit
{
	//
	// Compute the vertices stating at the top pole and moving down the stacks.
	//

	// Poles: note that there will be texture coordinate distortion as there is
	// not a unique point on the texture map to assign to the pole when mapping
	// a rectangular texture onto a sphere.
	XMFLOAT3 topVertex(0.0f, +radius, 0.0f);
	XMFLOAT3 bottomVertex(0.0f, -radius, 0.0f);

	Positions->push_back(topVertex);

	float phiStep = XM_PI / stackCount;
	float thetaStep = 2.0f*XM_PI / sliceCount;

	// Compute vertices for each stack ring (do not count the poles as rings).
	for (UINT i = 1; i <= stackCount - 1; ++i)
	{
		float phi = i*phiStep;

		// Vertices of ring.
		for (UINT j = 0; j <= sliceCount; ++j)
		{
			float theta = j*thetaStep;

			XMFLOAT3 v;

			// spherical to cartesian
			v.x = radius*sinf(phi)*cosf(theta);
			v.y = radius*cosf(phi);
			v.z = radius*sinf(phi)*sinf(theta);

			//// Partial derivative of P with respect to theta
			//v.TangentU.x = -radius*sinf(phi)*sinf(theta);
			//v.TangentU.y = 0.0f;
			//v.TangentU.z = +radius*sinf(phi)*cosf(theta);

			//XMVECTOR T = XMLoadFloat3(&v.TangentU);
			//XMStoreFloat3(&v.TangentU, XMVector3Normalize(T));

			//XMVECTOR p = XMLoadFloat3(&v.Position);
			//XMStoreFloat3(&v.Normal, XMVector3Normalize(p));

			XMFLOAT2 uv;

			uv.x = theta / XM_2PI;
			uv.y = phi / XM_PI;

			Positions->push_back(v);
			UVs->push_back(uv);
		}
	}

	Positions->push_back(bottomVertex);

	//
	// Compute indices for top stack.  The top stack was written first to the vertex buffer
	// and connects the top pole to the first ring.
	//

	for (UINT i = 1; i <= sliceCount; ++i)
	{
		Indices->push_back(0);
		Indices->push_back(i + 1);
		Indices->push_back(i);
	}

	//
	// Compute indices for inner stacks (not connected to poles).
	//

	// Offset the indices to the index of the first vertex in the first ring.
	// This is just skipping the top pole vertex.
	UINT baseIndex = 1;
	UINT ringVertexCount = sliceCount + 1;
	for (UINT i = 0; i < stackCount - 2; ++i)
	{
		for (UINT j = 0; j < sliceCount; ++j)
		{
			Indices->push_back(baseIndex + i*ringVertexCount + j);
			Indices->push_back(baseIndex + i*ringVertexCount + j + 1);
			Indices->push_back(baseIndex + (i + 1)*ringVertexCount + j);

			Indices->push_back(baseIndex + (i + 1)*ringVertexCount + j);
			Indices->push_back(baseIndex + i*ringVertexCount + j + 1);
			Indices->push_back(baseIndex + (i + 1)*ringVertexCount + j + 1);
		}
	}

	//
	// Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
	// and connects the bottom pole to the bottom ring.
	//

	// South pole vertex was added last.
	UINT southPoleIndex = (UINT)Positions->size() - 1;

	// Offset the indices to the index of the first vertex in the last ring.
	baseIndex = southPoleIndex - ringVertexCount;

	for (UINT i = 0; i < sliceCount; ++i)
	{
		Indices->push_back(southPoleIndex);
		Indices->push_back(baseIndex + i);
		Indices->push_back(baseIndex + i + 1);
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

HRESULT CompileShaderFromFile(LPCSTR szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob;
	hr = D3DX11CompileFromFile(szFileName, NULL, NULL, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL);
	if (FAILED(hr))
	{
		if (pErrorBlob != NULL)
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		if (pErrorBlob) pErrorBlob->Release();
		return hr;
	}
	if (pErrorBlob) pErrorBlob->Release();

	return S_OK;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	bool s = true;
	if (!init(hInstance, nCmdShow))
	{
		cleanup();
		return 0;
	}

	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

		}
		else
		{
			if (GetAsyncKeyState(VK_ESCAPE))
				PostQuitMessage(0);
			std::thread renderthread(render); //render on a separate thread
			update();
			renderthread.join();
		}
	}
	cleanup();

	return 0;

}

bool init(HINSTANCE hInstance, int nCmdShow)
{
	//Open console window
	AllocConsole();
	freopen("conin$", "r", stdin);
	freopen("conout$", "w", stdout);
	freopen("conout$", "w", stderr);

	srand(time(NULL));

	//add some spheres
	for (int i = 0; i < 2500; i++)
	{
		float radius = (rand() % 150 + 100) / 100.0f;
		Sphere sphere(XMFLOAT3((rand() % 500 - 250) / 10.0f, (rand() % 250 + 250) / 10.0f, (rand() % 500 - 250) / 10.0f), XMFLOAT4((rand() % 100) / 100.0f, (rand() % 100) / 100.0f, (rand() % 100) / 100.0f, 1.0f),
			radius, radius * 200.0f);
		Spheres.push_back(sphere);
	}

	//window class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = "WindowClass1";
	wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_APPLICATION);
	if (!RegisterClassEx(&wcex))
		return E_FAIL;

	//create window

	g_hInst = hInstance;
	RECT rc = { 0, 0, 1280, 800 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindow("WindowClass1", "Simple Sphere Physics", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
		NULL);

	if (!g_hWnd)
		return E_FAIL;

	ShowWindow(g_hWnd, nCmdShow);

	return initDirectx();
}

bool initDirectx()
{
	//create direct3d device and the swap chain for it
	HRESULT hr = S_OK;

	RECT rc = { 0, 0, 1280, 800 };

	//get dimensions of the window
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;

	//swapchain stuff
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = g_hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	//create the device and swap chain command does what it says it does
	hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
		D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);

	//make render target view
	ID3D11Texture2D* pBackBuffer = NULL;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	if (FAILED(hr))
		return hr;

	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;

	//create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = g_pd3dDevice->CreateTexture2D(&descDepth, NULL, &g_pDepthStencil);
	if (FAILED(hr))
		return hr;

	//create the depth stencil view out of the depth stencil texture
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
	if (FAILED(hr))
		return hr;

	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

	//setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports(1, &vp);

	//that's pretty much everything needed to create the device and other stuff to make it work
	//now we gotta make the shaders

	//compile vertex shader
	ID3DBlob* pVSBlob = NULL;
	hr = CompileShaderFromFile("shader.fx", "VS", "vs_4_0", &pVSBlob); //this function is from one of microsoft's directx 11 demos, it just makes stuff easier
	if (FAILED(hr))
	{
		MessageBox(NULL, "The fx file didn't compile, run this application in the location with the fx file", "Error", MB_OK);
		return hr;
	}

	//create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShader);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	//Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);

	//create the input layout
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pVertexLayout);

	pVSBlob->Release();

	if (FAILED(hr))
		return hr;

	//set the input layout
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	// Compile the pixel shader
	ID3DBlob* pPSBlob = NULL;
	hr = CompileShaderFromFile("shader.fx", "PS", "ps_5_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
			"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", "Error", MB_OK);
		return hr;
	}

	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShader);

	pPSBlob->Release();

	if (FAILED(hr))
		return hr;

	//make the sphere model
	std::vector<XMFLOAT3> verts;
	std::vector<WORD> indices;
	std::vector<XMFLOAT2> uvs;

	CreateSphere(1.0f, 100, 100, &verts, &indices, &uvs);

	num_sphere_indices = indices.size();

	Vertex* Vertices = static_cast<Vertex*>(std::malloc(verts.size() * sizeof(Vertex))); //this is some of the data we will pass to the Directx 11 machine to make the vertex buffer

	for (int i = 0; i < verts.size(); i++)
	{
		Vertex v;
		v.Pos = verts[i];
		XMVECTOR n = XMLoadFloat3(&verts[i]);
		XMStoreFloat3(&v.Norm, XMVector3Normalize(n));

		Vertices[i] = v;
	}

	WORD* Indices = static_cast<WORD*>(std::malloc(indices.size() * sizeof(WORD))); //this is some of the data we will pass to the Directx 11 machine to make the index buffer

	for (int i = 0; i < indices.size(); i++)
	{
		Indices[i] = indices[i];
	}

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex) * verts.size();
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = Vertices;
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);

	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(WORD) * indices.size();
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	InitData.pSysMem = Indices;
	g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pIndexBuffer);

	free(Vertices);
	free(Indices);

	// Set vertex buffer
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// Set index buffer
	g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	// Set primitive topology
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create the constant buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(cBufferShader1);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, NULL, &g_pcBufferShader1);
	if (FAILED(hr))
		return hr;

	// Initialize the view matrix
	XMVECTOR Eye = XMVectorSet(0.0f, 5.0f, -5.0f, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	g_View = XMMatrixLookAtLH(Eye, At, Up);

	// Initialize the projection matrix
	g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, width / (FLOAT)height, 0.01f, 1000.0f);

	return true;
}

void cleanup()
{
	if (g_pImmediateContext) g_pImmediateContext->ClearState();
	if (g_pDepthStencil) g_pDepthStencil->Release();
	if (g_pDepthStencilView) g_pDepthStencilView->Release();
	if (g_pRenderTargetView) g_pRenderTargetView->Release();
	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();
	if (g_pcBufferShader1) g_pcBufferShader1->Release();
	if (g_pVertexBuffer) g_pVertexBuffer->Release();
	if (g_pIndexBuffer) g_pIndexBuffer->Release();
	if (g_pVertexLayout) g_pVertexLayout->Release();
	if (g_pVertexShader) g_pVertexShader->Release();
	if (g_pPixelShader) g_pPixelShader->Release();
}

void update()
{
	// Update our time
	static float t = 0.0f;
	static DWORD dwTimeStart = 0;
	DWORD dwTimeCur = GetTickCount();
	if (dwTimeStart == 0)
		dwTimeStart = dwTimeCur;
	t = (dwTimeCur - dwTimeStart) / 1000.0f;

	dt = t - prev_time;

	prev_time = t;

	//gets cursor movement to update camera
	POINT currCursorPos;

	GetCursorPos(&currCursorPos);

	SetCursorPos(500, 500);

	g_Theta += (currCursorPos.x - 500) * 3.14159 / 180 / 10;
	g_Phi -= (currCursorPos.y - 500) * 3.14159 / 180 / 10;

	//this is for a panning camera
	//g_Theta = t / 3;
	//g_Phi = XM_PIDIV4 / 3;

	//add spheres if the spacebar key is pressed
	if (GetAsyncKeyState(VK_SPACE))
	{
		float radius = (rand() % 150 + 100) / 100.0f;
		Sphere sphere(XMFLOAT3((rand() % 500 - 250) / 10.0f, (rand() % 250 + 250) / 10.0f, (rand() % 500 - 250) / 10.0f), XMFLOAT4((rand() % 100) / 100.0f, (rand() % 100) / 100.0f, (rand() % 100) / 100.0f, 1.0f),
			radius, radius * 200.0f);
		Spheres.push_back(sphere);
	}

	//multithread the simulation
	std::thread thread1(handle_collisions, 0, Spheres.size() / 4);
	std::thread thread2(handle_collisions, Spheres.size() / 4, Spheres.size() / 2);
	std::thread thread3(handle_collisions, Spheres.size() / 2, 3 * Spheres.size() / 4);
	std::thread thread4(handle_collisions, 3 * Spheres.size() / 4, Spheres.size());

	thread1.join();
	thread2.join();
	thread3.join();
	thread4.join();

	//apply bounds for the spheres
	for (int i = 0; i < Spheres.size(); i++)
	{
		Spheres[i].ApplyForce(XMFLOAT3(0.0f, -9.8f * Spheres[i].Mass, 0.0f), dt);
		if (Spheres[i].Position.y - Spheres[i].Radius < 0)
		{
			Spheres[i].Position.y = Spheres[i].Radius;
			Spheres[i].Velocity.y *= -0.5f;
		}
		if (Spheres[i].Position.x - Spheres[i].Radius < -50.0f)
		{
			Spheres[i].Position.x = -50.0f + Spheres[i].Radius;
			Spheres[i].Velocity.x *= -0.5f;
		}
		else if (Spheres[i].Position.x + Spheres[i].Radius > 50.0f)
		{
			Spheres[i].Position.x = 50.0f - Spheres[i].Radius;
			Spheres[i].Velocity.x *= -0.5f;
		}
		if (Spheres[i].Position.z - Spheres[i].Radius < -50.0f)
		{
			Spheres[i].Position.z = -50.0f + Spheres[i].Radius;
			Spheres[i].Velocity.z *= -0.5f;
		}
		else if (Spheres[i].Position.z + Spheres[i].Radius > 50.0f)
		{
			Spheres[i].Position.z = 50.0f - Spheres[i].Radius;
			Spheres[i].Velocity.z *= -0.5f;
		}
		Spheres[i].Update(dt);
	}

	//makes the interface a little cleaner by restricting camera angles
	if (g_Phi > 3.14159 / 2)
	{
		g_Phi = 3.14159 / 2;
	}
	if (g_Phi < -3.14159 / 2)
	{
		g_Phi = -3.14159 / 2;
	}
}

void handle_collisions(int start, int end)
{
	for (int i = start; i < end; i++)
	{
		for (int j = 0; j < Spheres.size(); j++)
		{
			//calculate squared distance from centers
			float squared_distance = (Spheres[i].Position.x - Spheres[j].Position.x) * (Spheres[i].Position.x - Spheres[j].Position.x) +
				(Spheres[i].Position.y - Spheres[j].Position.y) * (Spheres[i].Position.y - Spheres[j].Position.y) + (Spheres[i].Position.z - Spheres[j].Position.z) * (Spheres[i].Position.z - Spheres[j].Position.z);
			//calculate the sum of the radii squared
			float radius_sum_squared = (Spheres[i].Radius + Spheres[j].Radius) * (Spheres[i].Radius + Spheres[j].Radius);
			if (squared_distance < radius_sum_squared && i != j)
			{
				//calculate force
				float force = pow(abs(squared_distance - radius_sum_squared) * 10.0f, 2.0f) * 10.0f;
				//create normalized vectors to apply the forces in the correct direction
				XMVECTOR f1 = XMVectorSet(Spheres[j].Position.x - Spheres[i].Position.x, Spheres[j].Position.y - Spheres[i].Position.y, Spheres[j].Position.z - Spheres[i].Position.z, 0.0f);
				XMVECTOR f2 = XMVectorSet(Spheres[i].Position.x - Spheres[j].Position.x, Spheres[i].Position.y - Spheres[j].Position.y, Spheres[i].Position.z - Spheres[j].Position.z, 0.0f);
				XMFLOAT3 force1;
				XMFLOAT3 force2;
				XMStoreFloat3(&force1, XMVector3Normalize(f1) * force);
				XMStoreFloat3(&force1, XMVector3Normalize(f2) * force);

				//apply the forces
				Spheres[i].ApplyForce(force1, dt);
				Spheres[j].ApplyForce(force2, dt);

				float dampening_cooldown_time = 0.16f; //time between dampening updates
				float dampening_constant = 0.9f;

				//dampen the spheres' velocities
				if (Spheres[i].dampen_time > dampening_cooldown_time)
				{
					Spheres[i].Velocity.x *= dampening_constant;
					Spheres[i].Velocity.y *= dampening_constant;
					Spheres[i].Velocity.z *= dampening_constant;

					Spheres[i].dampen_time = 0.0f;
				}

				if (Spheres[j].dampen_time > dampening_cooldown_time)
				{
					Spheres[j].Velocity.x *= dampening_constant;
					Spheres[j].Velocity.y *= dampening_constant;
					Spheres[j].Velocity.z *= dampening_constant;

					Spheres[j].dampen_time = 0.0f;
				}
			}
		}
	}
}

void render()
{
	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	//XMVECTOR Eye = XMVectorSet(cos(t)*5.0f, 2.0f, sin(t)*5.0f, 0.0f);
	XMVECTOR Eye = XMVectorSet(g_R * sin(g_Theta) * cos(g_Phi), g_R * sin(g_Phi), g_R * cos(g_Theta) * cos(g_Phi), 1.0f);
	XMVECTOR At = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	g_View = XMMatrixLookAtLH(Eye, At, Up);

	cBufferShader1 cb1;
	cb1.mWorld = XMMatrixTranspose(XMMatrixIdentity());
	cb1.mView = XMMatrixTranspose(g_View);
	cb1.mProjection = XMMatrixTranspose(g_Projection);
	cb1.color = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
	cb1.LightDir = XMFLOAT3(0.0f, -1.0f, -1.0f);
	XMStoreFloat3(&cb1.EyePos, Eye);
	XMStoreFloat3(&cb1.EyeDir, At - Eye);

	XMMATRIX A = cb1.mWorld;
	//A.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

	XMVECTOR det = XMMatrixDeterminant(A);
	cb1.mWorldInvTrans = XMMatrixInverse(&det, A);

	g_pImmediateContext->UpdateSubresource(g_pcBufferShader1, 0, NULL, &cb1, 0, 0);

	float ClearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	g_pImmediateContext->VSSetShader(g_pVertexShader, NULL, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pcBufferShader1);
	g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pcBufferShader1);
	g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);

	for (int i = 0; i < Spheres.size(); i++)
	{
		cb1.mWorld = XMMatrixTranspose(XMMatrixMultiply(XMMatrixScaling(Spheres[i].Radius, Spheres[i].Radius, Spheres[i].Radius), XMMatrixTranslation(Spheres[i].Position.x, Spheres[i].Position.y, Spheres[i].Position.z)));
		cb1.color = Spheres[i].Color;
		g_pImmediateContext->UpdateSubresource(g_pcBufferShader1, 0, NULL, &cb1, 0, 0);
		g_pImmediateContext->DrawIndexed(num_sphere_indices, 0, 0);
	}

	g_pSwapChain->Present(0, 0);
}