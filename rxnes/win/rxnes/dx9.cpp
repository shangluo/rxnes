extern "C"
{
	#include "video\video.h"
}

#ifdef RX_NES_RENDER_DX9
#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dcompiler.lib")

static LPDIRECT3D9 g_pD3D;
static LPDIRECT3DDEVICE9 g_pD3DDevice;
static LPDIRECT3DSURFACE9 g_pOffscreenBuffer;
static LPDIRECT3DSURFACE9 g_pBackbuffer;
static LPDIRECT3DTEXTURE9 g_pTexture;
static LPDIRECT3DVERTEXDECLARATION9 g_pVertexDeclaration;
static LPDIRECT3DPIXELSHADER9 g_pPixelShader;
static LPDIRECT3DVERTEXSHADER9 g_pVertexShader;
static LPDIRECT3DVERTEXBUFFER9 g_pVertexBuffer;


#define SAFE_RELEASE(p) \
	if (p) p->Release(); p = NULL;

void dx9_init(int width, int height, void *user)
{
	HWND hWnd = (HWND)user;
	g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (!g_pD3D)
	{
		return;
	}

	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.Windowed = TRUE;
	d3dpp.hDeviceWindow = hWnd;
	d3dpp.BackBufferCount = 1;
	d3dpp.BackBufferFormat = D3DFMT_R5G6B5;
	d3dpp.EnableAutoDepthStencil = TRUE;//TRUE;//FALSE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	//d3dpp.Flags = D3DPRESENT_DONOTWAIT;

	g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &g_pD3DDevice);
	if (!g_pD3DDevice)
	{
		return;
	}

	g_pD3DDevice->CreateOffscreenPlainSurface(256, 240, d3dpp.BackBufferFormat, D3DPOOL_DEFAULT, &g_pOffscreenBuffer, NULL);
	if (!g_pOffscreenBuffer)
	{
		return;
	}

	g_pD3DDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &g_pBackbuffer);
	if (!g_pBackbuffer)
	{
		return;
	}

	g_pD3DDevice->CreateTexture(256, 256, 0, D3DUSAGE_DYNAMIC, d3dpp.BackBufferFormat, D3DPOOL_DEFAULT, &g_pTexture, NULL);
	if (!g_pTexture)
	{
		return;
	}

	struct CustomVertex
	{
		float x, y, z;
		float s, t;
	};

	struct CustomVertex vert[4] =
	{
		-1.0f, -1.0f, 0, 0, 1,
		-1.0f, 1.0f, 0, 0, 0,
		1.0f, -1.0f, 0, 1, 1,
		1.0f, 1.0f, 0, 1, 0,
	};

	// vertex buffer
	if (g_pVertexBuffer == NULL)
	{
		g_pD3DDevice->CreateVertexBuffer(sizeof(vert), 0, 0, D3DPOOL_DEFAULT, &g_pVertexBuffer, NULL);
		VOID *pPtr = NULL;
		g_pVertexBuffer->Lock(0, 0, &pPtr, D3DLOCK_DISCARD);
		CopyMemory(pPtr, vert, sizeof(vert));
		g_pVertexBuffer->Unlock();
		g_pD3DDevice->SetStreamSource(0, g_pVertexBuffer, 0, sizeof(vert[0]));
	}

	if (g_pVertexShader == NULL)
	{
		static const char vertexShader[] = STRINGIFY
			(
		struct VS_IN
		{
			float4 pos : POSITION;
			float2 tex : TEXCOORD;
		};

		struct VS_OUT
		{
			float4 pos : POSITION;
			float2 tex : TEXCOORD;
		};

		float4x4 modelMatrix : register(c0);

		VS_OUT main(VS_IN param)
		{
			VS_OUT vout;
			vout.pos = mul(modelMatrix, param.pos);
			vout.tex = param.tex;
			return vout;
		}
		);

		LPD3DBLOB pFunc, pError;
		HRESULT hr = D3DCompile(vertexShader, strlen(vertexShader), "VS", NULL, NULL, "main", "vs_2_0", 0, 0, &pFunc, &pError);
		if (FAILED(hr))
		{
			const char * pErrorStr = (const char *)pError->GetBufferPointer();
			DEBUG_PRINT(pErrorStr);
		}

		DWORD *pCompiledCode = (DWORD *)pFunc->GetBufferPointer();
		g_pD3DDevice->CreateVertexShader(pCompiledCode, &g_pVertexShader);
		hr = g_pD3DDevice->SetVertexShader(g_pVertexShader);
		if (FAILED(hr))
		{
			DEBUG_PRINT("Vertex Shader Error");
		}

		float m[16] =
		{
			1.0, 0.0, 0.0, 0.0,
			0.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.0, 0.0, 0.0, 1.0,
		};
		hr = g_pD3DDevice->SetVertexShaderConstantF(0, m, 4);
		if (FAILED(hr))
		{
			DEBUG_PRINT("Set VertexShader ConstantF Error");
		}
	}


	// pixel shader
	if (g_pPixelShader == NULL)
	{
		static const char pixelShader[] = STRINGIFY
			(
		struct VS_OUT
		{
			float4 pos : POSITION;
			float2 tex : TEXCOORD;
		};

		sampler s;
		float4 main(VS_OUT param) : COLOR
		{
			//return float4(param.tex, 1.0, 1.0);
			return tex2D(s, param.tex);
		}
		);

		LPD3DBLOB pFunc, pError;
		HRESULT hr = D3DCompile(pixelShader, strlen(pixelShader), "PS", NULL, NULL, "main", "ps_2_0", 0, 0, &pFunc, &pError);
		if (FAILED(hr))
		{
			const char* pErrorStr = (const char*)pError->GetBufferPointer();
			DEBUG_PRINT(pErrorStr);
		}

		DWORD *pCompiledCode = (DWORD *)pFunc->GetBufferPointer();
		g_pD3DDevice->CreatePixelShader(pCompiledCode, &g_pPixelShader);
		hr = g_pD3DDevice->SetPixelShader(g_pPixelShader);
		if (FAILED(hr))
		{
			DEBUG_PRINT("Pixel Shader Error");
		}
	}

	if (g_pVertexDeclaration == NULL)
	{
		D3DVERTEXELEMENT9 desc[] =
		{
			{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, 3 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
			D3DDECL_END()
		};
		HRESULT hr = g_pD3DDevice->CreateVertexDeclaration(desc, &g_pVertexDeclaration);
		if (FAILED(hr))
		{
			DEBUG_PRINT("Create Vertex Declaration Error");
		}
		hr = g_pD3DDevice->SetVertexDeclaration(g_pVertexDeclaration);
		if (FAILED(hr))
		{
			DEBUG_PRINT("Set Vertex Declaration Error");
		}
	}
}

void dx9_render_frame(void *buffer, int buffer_width, int buffer_height, int bpp)
{
	// copy
	g_pD3DDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
	D3DLOCKED_RECT lockedRect;
	HRESULT hr = g_pTexture->LockRect( 0, &lockedRect, NULL, D3DLOCK_DISCARD);

	if (SUCCEEDED(hr))
	{
		CopyMemory(lockedRect.pBits, buffer, buffer_width * buffer_height * bpp);
		g_pTexture->UnlockRect(0);
	}


	g_pD3DDevice->BeginScene();
	//g_pD3DDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	g_pD3DDevice->SetTexture(0, g_pTexture);
	g_pD3DDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
	g_pD3DDevice->EndScene();
	g_pD3DDevice->Present(NULL, NULL, NULL, NULL);
}

void dx9_uninit()
{
	SAFE_RELEASE(g_pBackbuffer);
	SAFE_RELEASE(g_pOffscreenBuffer);
	SAFE_RELEASE(g_pTexture);
	SAFE_RELEASE(g_pD3D);
	SAFE_RELEASE(g_pD3DDevice);

	SAFE_RELEASE(g_pVertexDeclaration);
	SAFE_RELEASE(g_pPixelShader);
	SAFE_RELEASE(g_pVertexShader);
	SAFE_RELEASE(g_pVertexBuffer);
}

static struct dx9_init_block
{
	video_context *_context;
	dx9_init_block()
	{
		video_context *_context = video_context_create(video_name, dx9_init, dx9_render_frame, dx9_uninit);
		video_context_make_current(_context);
	}

	~dx9_init_block()
	{
		video_context_make_current(NULL);
		video_context_destroy(_context);
	}
}dx9_video_block;

video_init_block_impl(dx9, dx9_init, dx9_render_frame, dx9_uninit)

#endif