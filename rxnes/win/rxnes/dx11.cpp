extern "C"
{
#include "video\video.h"
}

#ifdef RX_NES_RENDER_DX11
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

static HWND g_hWnd;

static ID3D11Device *g_pDevice;
static ID3D11DeviceContext *g_pDeviceContext;
static IDXGISwapChain *g_pSwapChain;
static ID3D11RenderTargetView *g_pRenderTarget;
static ID3D11DepthStencilView *g_pDepthStencilView;
static ID3D11VertexShader *g_pVertexShader;
static ID3D11PixelShader *g_pPixelShader;
static ID3D11Texture2D *g_pTexture;
static ID3D11Buffer *g_pBuffer;
static ID3D11InputLayout *g_pInputLayout;

#define SAFE_RELEASE(p) \
	if (p) p->Release(); p = NULL;

void dx11_init(int width, int height, void *user)
{
	g_hWnd = (HWND)user;

	HRESULT hr;
	D3D_FEATURE_LEVEL nFeatureLevel;
	DXGI_SWAP_CHAIN_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = 1;
	desc.OutputWindow = g_hWnd;
	desc.Windowed = TRUE;

	int nFlags = 0;
	//nFlags |= D3D11_CREATE_DEVICE_DEBUG;

	hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, nFlags, NULL, 0, D3D11_SDK_VERSION, &desc, &g_pSwapChain, &g_pDevice, &nFeatureLevel, &g_pDeviceContext);
	if (FAILED(hr))
	{
		DEBUG_PRINT("Failed to create d3d11 device!\n");
	}

	{
		ID3D11Texture2D *pBackBuffer;
		g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
		g_pDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTarget);
		g_pDeviceContext->OMSetRenderTargets(1, &g_pRenderTarget, NULL);
		pBackBuffer->Release();

		RECT rc;
		GetClientRect(g_hWnd, &rc);

		D3D11_VIEWPORT viewPort =
		{
			0, 0, rc.right, rc.bottom, 0.0, 1.0
		};
		g_pDeviceContext->RSSetViewports(1, &viewPort);
	}

	float clearColor[] = { 0.0, 0.0, 0.0, 1.0 };
	g_pDeviceContext->ClearRenderTargetView(g_pRenderTarget, clearColor);


	static const char *shaderCode = STRINGIFY(
	struct VS_IN
	{
		float4 pos : POSITION;
		float2 tex : TEXCOORD;
	};

	struct VS_OUT
	{
		float4 pos : SV_POSITION;
		float2 tex : TEXCOORD;
	};

	//float4x4 modelMatrix : register(c0);

	VS_OUT VSmain(VS_IN param)
	{
		VS_OUT vout;
		//vout.pos = mul(modelMatrix, param.pos);
		vout.pos = param.pos;
		vout.tex = param.tex;
		return vout;
	}

	SamplerState s
	{
		Filter = MIN_MAG_MIP_LINEAR;
	};
	Texture2D tex;
	float4 PSmain(VS_OUT param) : SV_TARGET
	{
		//return float4(1.0, 0.0, 0.0, 1.0);
		return tex.Sample(s, param.tex);
	//return tex2D(s, param.tex);
	}
	);
	LPD3DBLOB pFuncVertex, pFuncPixel, pError;
	hr = D3DCompile(shaderCode, strlen(shaderCode), "VS", NULL, NULL, "VSmain", "vs_5_0", D3DCOMPILE_DEBUG, 0, &pFuncVertex, &pError);
	if (FAILED(hr))
	{
		LPCSTR pErrorStr = (LPCSTR)pError->GetBufferPointer();
		DEBUG_PRINT(pErrorStr);
	}

	hr = g_pDevice->CreateVertexShader(pFuncVertex->GetBufferPointer(), pFuncVertex->GetBufferSize(), NULL, &g_pVertexShader);
	if (FAILED(hr))
	{
		DEBUG_PRINT("Vertex Shader Error");
	}

	g_pDeviceContext->VSSetShader(g_pVertexShader, NULL, 0);

	if (!g_pInputLayout)
	{
		D3D11_INPUT_ELEMENT_DESC desc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 3 * sizeof(float), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		g_pDevice->CreateInputLayout(desc, 2, pFuncVertex->GetBufferPointer(), pFuncVertex->GetBufferSize(), &g_pInputLayout);
		g_pDeviceContext->IASetInputLayout(g_pInputLayout);
	}


	hr = D3DCompile(shaderCode, strlen(shaderCode), "PS", NULL, NULL, "PSmain", "ps_5_0", D3DCOMPILE_DEBUG, 0, &pFuncPixel, &pError);
	if (FAILED(hr))
	{
		LPCSTR pErrorStr = (LPCSTR)pError->GetBufferPointer();
		DEBUG_PRINT(pErrorStr);
	}

	hr = g_pDevice->CreatePixelShader(pFuncPixel->GetBufferPointer(), pFuncPixel->GetBufferSize(), NULL, &g_pPixelShader);
	if (FAILED(hr))
	{
		DEBUG_PRINT("Pixel Shader Error");
	}

	g_pDeviceContext->PSSetShader(g_pPixelShader, NULL, 0);


	if (!g_pBuffer)
	{
		struct CustomVertex
		{
			float x, y, z;
			float s, t;
		};

		struct CustomVertex vert[4] =
		{
			-1.0f, -1.0f, 0.0, 0, 1,
			-1.0f, 1.0f, 0.0, 0, 0,
			1.0f, -1.0f, 0.0, 1, 1,
			1.0f, 1.0f, 0.0, 1, 0,
		};

		D3D11_BUFFER_DESC desc =
		{
			sizeof(vert),
			D3D11_USAGE_DEFAULT,
			D3D11_BIND_VERTEX_BUFFER,
			0,
			0,
			sizeof(vert[0])
		};

		D3D11_SUBRESOURCE_DATA init =
		{
			vert,
			0,
			0
		};
		g_pDevice->CreateBuffer(&desc, &init, &g_pBuffer);

		CONST UINT stride = sizeof(vert[0]);
		CONST UINT offset = 0;
		g_pDeviceContext->IASetVertexBuffers(0, 1, &g_pBuffer, &stride, &offset);
		g_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	}


	if (!g_pTexture)
	{

		D3D11_TEXTURE2D_DESC  desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Width = 256;
		desc.Height = 240;
		desc.MipLevels = desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		g_pDevice->CreateTexture2D(&desc, NULL, &g_pTexture);

		ID3D11ShaderResourceView *pRC;
		g_pDevice->CreateShaderResourceView(g_pTexture, NULL, &pRC);
		g_pDeviceContext->PSSetShaderResources(0, 1, &pRC);
	}


}

void dx11_render_frame(void *buffer, int buffer_width, int buffer_height, int bpp)
{	
	if (g_pTexture)
	{
		// update
		D3D11_MAPPED_SUBRESOURCE rc;
		g_pDeviceContext->Map(g_pTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &rc);

		static u32 buffer2[240][256];
		video_rgb565_2_rgba888((u16(*)[256])buffer, buffer2, 255, 0);
		memcpy(rc.pData, buffer2, sizeof(buffer2));
		g_pDeviceContext->Unmap(g_pTexture, 0);
	}

	g_pDeviceContext->Draw(4, 0);
	HRESULT hr = g_pSwapChain->Present(1, 0);
	if (FAILED(hr))
	{
		DEBUG_PRINT("Failed to present!\n");
	}
}

void dx11_uninit()
{
	SAFE_RELEASE(g_pDevice);
	SAFE_RELEASE(g_pDeviceContext);
	SAFE_RELEASE(g_pSwapChain);
	SAFE_RELEASE(g_pRenderTarget);
	SAFE_RELEASE(g_pDepthStencilView);
	SAFE_RELEASE(g_pVertexShader);
	SAFE_RELEASE(g_pPixelShader);
	SAFE_RELEASE(g_pTexture);
	SAFE_RELEASE(g_pBuffer);
	SAFE_RELEASE(g_pInputLayout);
}

video_init_block_impl(dx11, dx11_init, dx11_render_frame, dx11_uninit)

#endif