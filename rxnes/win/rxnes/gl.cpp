extern "C"
{
#include "video\video.h"
}

#ifdef RX_NES_RENDER_GL
#define GLEW_STATIC 
#include "glew/include/GL/glew.h"
#include "glew/include/GL/wglew.h"
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glew/lib/Release/Win32/glew32s.lib")

#include <DirectXMath.h>

static HWND g_hWnd;
static HGLRC g_hGL;
static float g_AngleX;
static float g_AngleY;
static float g_XOffset;
static float g_YOffset;
static float g_zDinstance = -207.84f;

static GLuint g_vao;
static GLuint g_program;
static GLuint g_vshader, g_fshader, g_gshader;
static GLuint g_vbo1, g_vbo2;
static GLuint g_texture;

static const char * g_fragShaderSource = STRINGIFY
(
	void mainImage(out vec4 fragColor, in vec2 fragCoord)
	{
		fragColor = texture2D(iChannel0, texCoord.st);
	}
);

static const char * g_fragShaderSourceDef = STRINGIFY
(
	void mainImage(out vec4 fragColor, in vec2 fragCoord)
	{
		fragColor = texture2D(iChannel0, texCoord.st);
	}
);

void gl_init(int width, int height, void *user)
{
	g_hWnd = (HWND)user;
	HDC hDC = GetDC(g_hWnd);

	RECT rc;
	GetClientRect(g_hWnd, &rc);

	PIXELFORMATDESCRIPTOR pfd;
	int iPixelFormat;

	ZeroMemory(&pfd, sizeof(pfd));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;

	iPixelFormat = ChoosePixelFormat(hDC, &pfd);

	if (iPixelFormat > 0)
	{
		SetPixelFormat(hDC, iPixelFormat, &pfd);
		g_hGL = wglCreateContext(hDC);

		if (g_hGL)
		{
			wglMakeCurrent(hDC, g_hGL);

			GLenum err;
			if ((err = glewInit()) != GLEW_OK)
			{
				const GLubyte *str = glewGetErrorString(err);
				return;
			}

			const int attribList[] =
			{
				WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
				WGL_CONTEXT_MINOR_VERSION_ARB, 2,
				WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
				0
			};

			HGLRC hGLCore = wglCreateContextAttribsARB(hDC, NULL, attribList);
			if (hGLCore)
			{
				wglMakeCurrent(hDC, hGLCore);
				if ((err = glewInit()) != GLEW_OK)
				{
					const GLubyte *str = glewGetErrorString(err);
					return;
				}

				wglDeleteContext(g_hGL);
				g_hGL = hGLCore;
			}
		}
	}

	ReleaseDC(g_hWnd, hDC);


	if (!GLEW_VERSION_2_0)
	{
		return;
	}


	g_vshader = glCreateShader(GL_VERTEX_SHADER);

	static const GLchar *vertexShader = STRINGIFY(
		uniform mat4 modelview;
	uniform mat4 projection;

	in vec4 pos;
	in vec2 tex;
	out vec2 texCoordGS;
	void main()
	{
		vec4 dstPos = projection * modelview * pos;
		//dstPos.y = -dstPos.y;
		gl_Position = dstPos;
		texCoordGS = tex;
	}
	);
	int shaderLen = strlen(vertexShader);

	glShaderSource(g_vshader, 1, &vertexShader, &shaderLen);
	glCompileShader(g_vshader);

	char log[1024];
	int len;
	glGetShaderInfoLog(g_vshader, 1024, &len, log);

	g_gshader = glCreateShader(GL_GEOMETRY_SHADER);
	static const GLchar *geometryShader = STRINGIFY(
		layout(triangles) in;
	layout(triangle_strip, max_vertices = 3) out;
	in vec2 texCoordGS[];
	out vec2 texCoord;

	void main()
	{
		for (int i = 0; i < 3; ++i)
		{
			gl_Position = gl_in[i].gl_Position;
			texCoord = texCoordGS[i];
			EmitVertex();
		}
		EndPrimitive();
	}
	);
	shaderLen = strlen(geometryShader);

	glShaderSource(g_gshader, 1, &geometryShader, &shaderLen);
	glCompileShader(g_gshader);
	glGetShaderInfoLog(g_gshader, 1024, &len, log);

	int fragLen = 0;
	char *fragSource = NULL;
	g_fshader = glCreateShader(GL_FRAGMENT_SHADER);

	static const GLchar *fragShaderHead = STRINGIFY(
		uniform sampler2D iChannel0;
	uniform vec3 iResolution;
	uniform float iGlobalTime;

	in vec2 texCoord;
	out vec4 color;)
		;

	static const GLchar *fragShaderEnd = STRINGIFY(
		void main()
	{
		mainImage(gl_FragColor, gl_FragCoord.xy);
	};);

	shaderLen = strlen(fragShaderHead) + strlen(g_fragShaderSource) + strlen(fragShaderEnd);
	fragSource = (char *)malloc(shaderLen + 1);
	strcpy(fragSource, fragShaderHead);
	strcat(fragSource, g_fragShaderSource);
	strcat(fragSource, fragShaderEnd);

	fragSource[shaderLen] = '\0';
	glShaderSource(g_fshader, 1, &fragSource, &shaderLen);
	free(fragSource);

	glCompileShader(g_fshader);
	glGetShaderInfoLog(g_fshader, 1024, &len, log);

	g_program = glCreateProgram();
	glAttachShader(g_program, g_vshader);
	glAttachShader(g_program, g_gshader);
	glAttachShader(g_program, g_fshader);

	glLinkProgram(g_program);

	glGetProgramInfoLog(g_program, 1024, &len, log);
	glUseProgram(g_program);
	
	// setup uniform
	int location;

	// vertex shader
	location = glGetUniformLocation(g_program, "modelview");
	if (location != -1)
	{
		DirectX::XMMATRIX mat, mat1, mat2;
		mat = DirectX::XMMatrixTranslation(g_XOffset, g_YOffset, g_zDinstance);
		mat1 = DirectX::XMMatrixRotationX(g_AngleX);
		mat2 = DirectX::XMMatrixRotationY(g_AngleY);
		glUniformMatrix4fv(location, 1, GL_FALSE, (GLfloat *)&(mat * mat1 * mat2));
	}

	location = glGetUniformLocation(g_program, "projection");
	if (location != -1)
	{
		DirectX::XMMATRIX mat;
		mat = DirectX::XMMatrixPerspectiveFovRH(60.0f * 3.14 / 180, rc.right * 1.0f / rc.bottom, 1, 1000);
		glUniformMatrix4fv(location, 1, GL_FALSE, (GLfloat *)&mat);
	}

	location = glGetUniformLocation(g_program, "iChannel0");
	if (location != -1)
	{
		glUniform1i(location, 0);
	}	

	glGenVertexArrays(1, &g_vao);
	glBindVertexArray(g_vao);

	glGenTextures(1, &g_texture);
	glBindTexture(GL_TEXTURE_2D, g_texture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	GLfloat vertice[][3] =
	{
		-128.0f, 120.0f, 0.1f,
		-128.0f, -120.0f, 0.1f,
		128.0f, -120.0f, 0.1f,
		128.0f, 120.0f, 0.1f,
		-128.0f, 120.0f, -0.1f,
		-128.0f, -120.0f, -0.1f,
		128.0f, -120.0f, -0.1f,
		128.0f, 120.0f, -0.1f,
	};

	GLfloat texices[][2] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
	};

	glGenBuffers(1, &g_vbo1);
	glBindBuffer(GL_ARRAY_BUFFER, g_vbo1);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertice), vertice, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glGenBuffers(1, &g_vbo2);
	glBindBuffer(GL_ARRAY_BUFFER, g_vbo2);
	glBufferData(GL_ARRAY_BUFFER, sizeof(texices), texices, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

	glBindAttribLocation(g_program, 0, "pos");
	glBindAttribLocation(g_program, 1, "tex");
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);
	wglSwapIntervalEXT(1);
}

void gl_render_frame(void *buffer, int buffer_width, int buffer_height, int bpp)
{

	RECT rc;
	GetClientRect(g_hWnd, &rc);
	glViewport(0, 0, rc.right, rc.bottom);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	static u32 gl_buffer[240][256];
	video_rgb565_2_rgba888((u16(*)[256])buffer, gl_buffer, 128, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 240, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, gl_buffer);
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindVertexArray(g_vao);
	glUseProgram(g_program);

	// draw
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
	
	glBindVertexArray(0);

	// swap buffers
	HDC hDC = GetDC(g_hWnd);
	SwapBuffers(hDC);
	ReleaseDC(g_hWnd, hDC);
}

void gl_uninit()
{
	glDeleteShader(g_vshader);
	glDeleteShader(g_gshader);
	glDeleteShader(g_fshader);
	glDeleteProgram(g_program);
	glDeleteVertexArrays(1, &g_vao);
	glDeleteBuffers(1, &g_vbo1);
	glDeleteBuffers(1, &g_vbo2);
	glDeleteTextures(1, &g_texture);

	wglMakeCurrent(NULL, NULL);
	if (g_hGL)
	{
		wglDeleteContext(g_hGL);
		g_hGL = NULL;
	}
}

video_init_block_impl(gl, gl_init, gl_render_frame, gl_uninit)

#endif