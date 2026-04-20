// ShaderWallpaper.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <chrono>
#include <thread>
#include <exception>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#ifndef NOMINMAX
#    define NOMINMAX
#endif  // !defined(NOMINMAX)
#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif  // !defined(WIN32_LEAN_AND_MEAN)
#include <windows.h>
#include <tchar.h>
#include <GL/glew.h>
#include "Finally.hpp"


//! Vertex shader source for version 1.0
static const char vertSource100[] = "attribute vec3 position;\nvoid main(void)\n{\n    gl_Position = vec4(position, 1.0);\n}\n";
//! Vertex shader source for version 3.0 ES
static const char vertSource300[] = "#version 300 es\nin vec3 position;\nvoid main(void)\n{\n    gl_Position = vec4(position, 1.0);\n}\n";
//! Vertex positions of quad.
static const GLfloat kVertices[] = {
  -1.0f, 1.0f, 0.0f,
  1.0f, 1.0f, 0.0f,
  -1.0f, -1.0f, 0.0f,
  1.0f, -1.0f, 0.0f
};
//! Triangle indices of quad.
static const GLushort kTriangles[] = {
  0, 2, 1,
  1, 2, 3
};


static HWND GetWorkerW();
static GLint buildProgram(const char* fsFilePath);
static std::string readAllText(const char *filePath);
static GLuint createShaderObjectFromFile(const char *filePath, GLenum shaderType);
static GLuint createShaderObjectFromText(const std::string& shaderSource, GLenum shaderType);
static GLuint createShaderObjectFromText(const char *shaderSource, size_t sourceSize, GLenum shaderType);
static GLint linkShaders(GLuint vsObj, GLuint fsObj);
static GLint createVbo(const GLfloat *vertices, size_t n);
static GLint createIbo(const GLushort *triangles, size_t n);

namespace fs = std::filesystem;


class GlProg
{
public:
	GlProg()
		: _program{ 0 }
		, _vbo{ 0 }
		, _ibo{ 0 }
		, _aPosition{ 0 }
		, _uTime{ 0 }
		, _uMouse{ 0 }
		, _uResolution{ 0 }
		, _uFrameCount{ 0 }
		, _uPositionOffset{ 0 }
	{
	}

	GlProg(GLint program)
		: _program(program)
		, _vbo{ 0 }
		, _ibo{ 0 }
		, _aPosition{ glGetAttribLocation(program, "position")}
		, _uTime{ glGetUniformLocation(program, "u_time") }
		, _uMouse{ glGetUniformLocation(program, "u_mouse") }
		, _uResolution{ glGetUniformLocation(program, "u_resolution") }
		, _uFrameCount{ glGetUniformLocation(program, "u_frameCount") }
		, _uPositionOffset{ glGetUniformLocation(program, "u_positionOffset") }
	{

	}

	GLint program() const
	{
		return _program;
	}

	GLint getOrCreateVbo(const GLfloat *vertices, size_t n)
	{
		return _vbo == 0 ? (_vbo = createVbo(vertices, n)) : _vbo;
	}

	GLint getOrCreateIbo(const GLushort *triangles, size_t n)
	{
		return _ibo == 0 ? (_ibo = createIbo(triangles, n)) : _ibo;
	}

	GLint aPosition() const
	{
		return _aPosition;
	}

	GLint uTime() const
	{
		return _uTime;
	}

	GLint uMouse() const
	{
		return _uMouse;
	}

	GLint uResolution() const
	{
		return _uResolution;
	}

	GLint uFrameCount() const
	{
		return _uFrameCount;
	}

	GLint uPositionOffset() const
	{
		return _uPositionOffset;
	}
private:
	GLint _program;
	GLint _vbo;
	GLint _ibo;
	GLint _aPosition;
	GLint _uTime;
	GLint _uMouse;
	GLint _uResolution;
	GLint _uFrameCount;
	GLint _uPositionOffset;
};


struct Monitor
{
    RECT rc;
    HWND hwnd;
    HDC dc;
    HGLRC rcGL;
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void setupVsync(bool doVSync)
{
	if (strstr((char*)glGetString(GL_EXTENSIONS), "WGL_EXT_swap_control") == 0) {
		return;
	}

	auto wglSwapIntervalEXT = (BOOL(WINAPI*)(int))wglGetProcAddress("wglSwapIntervalEXT");
	if (wglSwapIntervalEXT != nullptr) {
		wglSwapIntervalEXT(doVSync ? 1 : 0);
	}
}


int main(int argc, char *argv[])
{
	try {
		auto hWorkerW = GetWorkerW();
		auto hDC = GetDCEx(hWorkerW, nullptr, 0x403);
		if (hDC == nullptr) {
			std::cerr << "GetDCEx() failed" << std::endl;
			return 1;
		}
		auto f1 = makeFinally([&] {
			ReleaseDC(hWorkerW, hDC);
		});

		// std::vector<RECT> rects;
		// // RECT rect;
		// // if (!GetWindowRect(hWorkerW, &rect)) {
		// // 	std::cerr << "GetWindowRect failed" << std::endl;
		// // }
		// // rects.emplace_back(rect);
		// if (!EnumDisplayMonitors(NULL, NULL, +[](HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) -> BOOL {
		// 		MONITORINFOEX monitorInfo;
		// 		monitorInfo.cbSize = sizeof(monitorInfo);
		// 		GetMonitorInfo(hMonitor, &monitorInfo);
		// 		((std::vector<RECT>*)dwData)->emplace_back(monitorInfo.rcMonitor);
		// 		return TRUE;
		// 	}, (LPARAM)&rects)) {
		// 	std::cerr << "EnumDisplayMonitors() failed" << std::endl;
		// 	return 2;
		// }

		std::vector<Monitor> mons;
		auto ret = EnumDisplayMonitors(NULL, NULL, [](HMONITOR hMon, HDC, LPRECT, LPARAM p) -> BOOL {
			std::vector<Monitor>* list = (std::vector<Monitor>*)p;
			MONITORINFO mi = { sizeof(mi) };
			if (!GetMonitorInfo(hMon, &mi)) {
				return FALSE;
			}
			Monitor m = {};
			m.rc = mi.rcMonitor;
			list->push_back(m);
			return TRUE;
		}, (LPARAM)&mons);
		if (!ret) {
			return 1000;
		}

		PIXELFORMATDESCRIPTOR pfd = {
			sizeof(PIXELFORMATDESCRIPTOR),
			1,
			PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
			PFD_TYPE_RGBA,
			32,     // color
			0, 0,   // R
			0, 0,   // G
			0, 0,   // B
			0, 0,   // A
			0, 0, 0, 0, 0,      // AC R G B A
			24,     // depth
			8,      // stencil
			0,      // aux
			0,      // layertype
			0,  // reserved
			0,  // layermask
			0,  // visiblemask
			0   // damagemask
		};
		int pf = ChoosePixelFormat(hDC, &pfd);
		if (pf == 0) {
			std::cerr << "ChoosePixelFormat() failed" << std::endl;
			return 2;
		}

		// if (!SetPixelFormat(hDC, pf, &pfd)) {
		// 	std::cerr << "SetPixelFormat() failed" << std::endl;
		// 	return 3;
		// }

		WNDCLASS wc = { 0 };
		wc.lpfnWndProc = WndProc;
		wc.hInstance = nullptr;
		wc.lpszClassName = _T("ShaderWallPaper");
		if (RegisterClass(&wc) == 0) {
			std::cerr << "RegisterClass() failed" << std::endl;
			return 1001;
		}

		int index = 0;
		for (auto& m : mons) {
			int w = m.rc.right - m.rc.left;
			int h = m.rc.bottom - m.rc.top;

			m.hwnd = CreateWindowEx(
				0,
				_T("ShaderWallPaper"),
				L"",
				WS_CHILD | WS_VISIBLE,
				m.rc.left,
				m.rc.top,
				w,
				h,
				hWorkerW,
				0,
				nullptr,
				0);
			if (m.hwnd == nullptr) {
				std::cerr << "CreateWindowEx() failed: 0x" << std::hex << GetLastError() << std::endl;
				return 100;
			}

			SetWindowLong(m.hwnd, GWL_EXSTYLE, GetWindowLong(m.hwnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);

			m.dc = GetDC(m.hwnd);
			if (m.dc == nullptr) {
				std::cerr << "GetDC() failed" << std::endl;
				std::printf("LastError = 0x%08x\n", GetLastError());
				return 2;
			}
			if (!SetPixelFormat(m.dc, pf, &pfd)) {
				std::cerr << "SetPixelFormat() failed" << std::endl;
				return 3;
			}

			m.rcGL = wglCreateContext(m.dc);
			if (m.rcGL == nullptr) {
				std::cerr << "wglCreateContext() failed" << std::endl;
				std::printf("LastError = 0x%08x\n", GetLastError());
				return 4;
			}

			if (index == 0) {
				// for glewInit().
				if (!wglMakeCurrent(m.dc, m.rcGL)) {
					std::cerr << "wglMakeCurrent() failed" << std::endl;
					std::printf("LastError = 0x%08x\n", GetLastError());
					return 5;
				}
			} else {
				if (!wglShareLists(mons[0].rcGL, m.rcGL)) {
					std::cerr << "wglShareLists() failed" << std::endl;
					std::printf("LastError = 0x%08x\n", GetLastError());
				}
			}

			index++;
		}

		if (glewInit() != GLEW_OK) {
			std::cerr << "glewInit() failed" << std::endl;
			return 4;
		}


		// auto hGlrc = wglCreateContext(hDC);
		// if (hGlrc == nullptr) {
		// 	std::cerr << "wglCreateContext() failed" << std::endl;
		// 	std::printf("LastError = 0x%08x\n", GetLastError());
		// 	return 2;
		// }
		// auto f2 = makeFinally([&] {
		// 	wglDeleteContext(hGlrc);
		// 	});
		// // make it the calling thread's current rendering context 
		// if (!wglMakeCurrent(hDC, hGlrc)) {
		// 	std::cerr << "wglMakeCurrent() failed" << std::endl;
		// 	std::printf("LastError = 0x%08x\n", GetLastError());
		// 	return 3;
		// }
		// auto f3 = makeFinally([&] {
		// 	wglMakeCurrent(nullptr, nullptr);
		// });

		const char *targetDir = argc > 1 ? argv[1] : ".";
		std::vector<std::pair<std::string, GlProg>> pathProgPairs;
		for (const fs::directory_entry& entry : fs::recursive_directory_iterator(targetDir)) {
			const auto& ext = entry.path().extension();
			if (ext == ".glsl") {
				pathProgPairs.emplace_back(std::make_pair(entry.path().string(), GlProg{}));
			}
		}
		auto f4 = makeFinally([&] {
			for (const auto& pair : pathProgPairs) {
				auto program = pair.second.program();
				if (program != 0) {
					glDeleteProgram(program);
				}
			}
		});

		// call OpenGL APIs as desired ... 
		// for (auto&& pair : pathProgPairs) {
		// 	std::cout << "Compile " << pair.first << "..." << std::endl;
		// 	auto program = buildProgram(pair.first.c_str());  // TODO
		// 	std::cout << "Compile " << pair.first << "... Done" << std::endl;
		// 	pair.second = GlProg{ program };

		// 	// Bind VBO.
		// 	auto attribLocation = glGetAttribLocation(program, "position");
		// 	glBindBuffer(GL_ARRAY_BUFFER, createVbo(kVertices, sizeof(kVertices) / sizeof(kVertices[0])));
		// 	glEnableVertexAttribArray(attribLocation);
		// 	glVertexAttribPointer(attribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
		// 	// Bind IBO.
		// 	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, createIbo(kTriangles, sizeof(kTriangles) / sizeof(kTriangles[0])));
		// }

		// glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

		auto isContinue = true;
		std::thread th{ [&] {
			std::string s;
			std::cin >> s;
			isContinue = false;
			std::cout << "FINISH" << std::endl;
		} };

		// std::vector<std::pair<RECT, int>> rectIndexPairs;
		std::vector<std::pair<Monitor, int>> monitorIndexPairs;
		std::uniform_int_distribution<> dist(0, pathProgPairs.size() - 1);
		std::mt19937 rnd{ std::random_device{}() };

		for (const auto& mon : mons) {
			monitorIndexPairs.emplace_back(std::make_pair(mon, 0));
		}

		setupVsync(true);

		auto needShuffle = true;
		int frameCount = 0;
		auto start = std::chrono::high_resolution_clock::now();
		while (isContinue) {
			const auto frameStart = std::chrono::high_resolution_clock::now();
			const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frameStart - start).count();

			// Handle Window Messages.
			MSG msg;
			while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
				if (msg.message == WM_QUIT) {
					return 0;
				}
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			if (elapsed >= 5000) {
				needShuffle = true;
				start = frameStart;
			}

			if (needShuffle) {
				needShuffle = false;
				for (auto& mon : monitorIndexPairs) {
					while (!pathProgPairs.empty()) {
						auto progIndex = dist(rnd);
						try {
							auto& pair = pathProgPairs[progIndex];
							if (pair.second.program() == 0) {  // Need compile
								std::cout << "Compile " << pair.first << "..." << std::endl;
								auto program = buildProgram(pair.first.c_str());  // TODO
								std::cout << "Compile " << pair.first << "... Done" << std::endl;
								pair.second = GlProg{ program };
							}

							mon.second = progIndex;
							break;
						} catch (std::exception& ex) {
							std::cerr << ex.what() << std::endl;
							pathProgPairs.erase(pathProgPairs.begin() + progIndex);
							dist.param(std::uniform_int_distribution<>::param_type{ 0, (int)pathProgPairs.size() - 1 });
						}
					}
				}
				if (pathProgPairs.empty()) {
					return 6;  // No available program.
				}
			}

			for (const auto& miPair : monitorIndexPairs) {
				const auto& mon = miPair.first;
				const auto& r = mon.rc;
				auto width = std::abs(r.right - r.left);
				auto height = std::abs(r.bottom - r.top);

				// std::cout << "top = " << r.top
				// 	<< ", left = " << r.left
				// 	<< ", right = " << r.right
				// 	<< ", bottom = " << r.bottom
				// 	<< ", dc = " << mon.dc 
				// 	<< ", rcGL = " << mon. rcGL
				// 	<< "\n";

				if (!wglMakeCurrent(mon.dc, mon.rcGL))
				{
					std::cerr << "wglMakeCurrent() failed" << std::endl;
					std::fprintf(stderr, "LastError = 0x%08x\n", GetLastError());
					return 3;
				}

				glViewport(0, 0, width, height);
				glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
				glClearDepth(1.0f);
				glClearStencil(0);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

				auto& glProg = pathProgPairs[miPair.second].second;
				glUseProgram(glProg.program());

				glBindBuffer(GL_ARRAY_BUFFER, glProg.getOrCreateVbo(kVertices, sizeof(kVertices) / sizeof(kVertices[0])));
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glProg.getOrCreateIbo(kTriangles, sizeof(kTriangles) / sizeof(kTriangles[0])));

				auto attribLocation = glProg.aPosition();
				glEnableVertexAttribArray(attribLocation);
				glVertexAttribPointer(attribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

				glUniform1f(glProg.uTime(), static_cast<float>(static_cast<double>(elapsed) / 1000.0L));
				glUniform2f(glProg.uMouse(), 0.0f, 0.0f);
				glUniform2f(glProg.uResolution(), static_cast<float>(width), static_cast<float>(height));
				glUniform1f(glProg.uFrameCount(), static_cast<float>(frameCount));
				// glUniform2f(glProg.uPositionOffset(), static_cast<float>(r.left), static_cast<float>(r.top));
				glUniform2f(glProg.uPositionOffset(), static_cast<float>(0), static_cast<float>(0));

				glDrawElements(GL_TRIANGLES, sizeof(kTriangles) / sizeof(kTriangles[0]), GL_UNSIGNED_SHORT, nullptr);

				if (!SwapBuffers(mon.dc)) {
					std::cerr << "SwapBuffers() failed" << std::endl;
				}
			}

			frameCount++;

			const auto frameEnd = std::chrono::high_resolution_clock::now();
			const auto sleepTime = std::chrono::microseconds{15 * 1000} - (frameEnd - frameStart);
			if (sleepTime > std::chrono::microseconds{0}) {
				std::this_thread::sleep_for(sleepTime);
			}
			else {
				std::cerr << "DELAYES" << std::endl;
			}
		}
		th.join();
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() << std::endl;
	}
}


static HWND GetWorkerW()
{
	// https://qiita.com/utasimaru/items/494796bf725ac11a4883
    auto hProgman = FindWindow(_T("Progman"), nullptr);
	SendMessageTimeout(hProgman, 0x52c, 0, 0, 0x0, 1000, nullptr);

	HWND hWorkerW = nullptr;
	// EnumWindows(EnumWindowsCallback, (LPARAM)&hWorkerW);
	EnumWindows(+[](HWND hWnd, LPARAM lParam) -> BOOL {
		auto shell = FindWindowEx(hWnd, nullptr, _T("SHELLDLL_DefView"), nullptr);
		if (shell != nullptr) {
			*(HWND*)lParam = FindWindowEx(nullptr, hWnd, _T("WorkerW"), nullptr);
		}
		return TRUE;
	}, (LPARAM)&hWorkerW);

	if (hWorkerW == nullptr) {
		throw std::runtime_error{"EnumWindows() failed"};
	}

	return hWorkerW;
}


/*!
 * @brief Read all data in specified file.
 * @param [in] filePath  Target file path.
 * @return std::string of read text.
 */
static std::string readAllText(const char *filePath)
{
	std::ifstream ifs(filePath, std::ios::binary);
	if (!ifs.is_open()) {
		throw std::runtime_error{ std::string("Failed to open ") + filePath };
	}
	return std::string{ (std::istreambuf_iterator<char>{ifs}), std::istreambuf_iterator<char>{} };
}


/*!
 * @brief Create shader object from specified file.
 * @param [in] filePath  Target file path.
 * @param [in] shaderType  Shader type.
 * @return Created shader object.
 */
static GLuint createShaderObjectFromFile(const char *filePath, GLenum shaderType)
{
	auto shaderSource = readAllText(filePath);
	return createShaderObjectFromText(shaderSource, shaderType);
}


/*!
 * @brief Create shader object from specified string.
 * @param [in] shaderSource  Shader source string.
 * @param [in] shaderType  Shader type.
 * @return Created shader object.
 */
static GLuint createShaderObjectFromText(const std::string& shaderSource, GLenum shaderType)
{
	return createShaderObjectFromText(shaderSource.c_str(), shaderSource.length(), shaderType);
}


/*!
 * @brief Create shader object from specified string.
 * @param [in] shaderSource  Pointer to Shader source.
 * @param [in] sourceSize  Length of shader source.
 * @param [in] shaderType  Shader type.
 * @return Created shader object.
 */
static GLuint createShaderObjectFromText(const char *shaderSource, size_t sourceSize, GLenum shaderType)
{

	auto shaderObj = glCreateShader(shaderType);
	if (shaderObj == 0) {
		std::ostringstream oss;
		oss << "Failed to create shader type=["
			<< shaderType
			<< "]";
		throw std::runtime_error{ oss.str() };
	}

	auto sourceLength = static_cast<GLint>(sourceSize);

	glShaderSource(shaderObj, 1, &shaderSource, &sourceLength);
	glCompileShader(shaderObj);

	GLint infoLogLength;
	glGetShaderiv(shaderObj, GL_INFO_LOG_LENGTH, &infoLogLength);
	std::string infoLog(static_cast<size_t>(infoLogLength), '\0');

	GLint fsCompileResult;
	glGetShaderiv(shaderObj, GL_COMPILE_STATUS, &fsCompileResult);
	if (fsCompileResult == GL_FALSE) {
		GLsizei logLength;
		glGetShaderInfoLog(shaderObj, static_cast<GLsizei>(infoLog.length()), &logLength, const_cast<GLchar*>(infoLog.data()));
		glDeleteShader(shaderObj);
		throw std::runtime_error{ infoLog };
	} else if (!infoLog.empty()) {
		std::cerr << infoLog << std::endl;
	}

	return shaderObj;
}


/*!
 * @brief Build shader program.
 * @param [in] fsFilePath  Fragment shader path.
 * @return Built shader program.
 */
static GLint buildProgram(const char *fsFilePath)
{
	auto fsObj = createShaderObjectFromFile(fsFilePath, GL_FRAGMENT_SHADER);
	auto f1 = makeFinally([=] {
		glDeleteShader(fsObj);
	});

	auto vsObj = createShaderObjectFromText(vertSource100, sizeof(vertSource100), GL_VERTEX_SHADER);
	auto f2 = makeFinally([=] {
		glDeleteShader(vsObj);
	});

	return linkShaders(vsObj, fsObj);
}


/*!
 * @brief Link vertex shader and fragment shader.
 * @param [in] vsObj  Vertex shader object.
 * @param [in] fsObj  Fragment shader object.
 * @return Build shader program.
 */
static GLint linkShaders(GLuint vsObj, GLuint fsObj)
{
	static char infoLogBuf[1024 * 1024];

	GLuint shader = glCreateProgram();
	if (shader == 0) {
		throw std::runtime_error{ "Failed to create program" };
	}

	glAttachShader(shader, vsObj);
	glAttachShader(shader, fsObj);
	glLinkProgram(shader);
	GLint linkResult;
	glGetProgramiv(shader, GL_LINK_STATUS, &linkResult);
	if (linkResult == GL_FALSE) {
		GLsizei logLength;
		glGetProgramInfoLog(shader, static_cast<GLsizei>(sizeof(infoLogBuf)), &logLength, infoLogBuf);
		throw std::runtime_error{ infoLogBuf };
	}

	return shader;
}


/*!
 * @brief Create vertex buffer object.
 * @param [in] vertices  Vertex positions.
 * @param [in] n  Number of elements.
 * @return Vertex buffer object.
 */
static GLint createVbo(const GLfloat *vertices, size_t n)
{
	GLuint vbo;
	glGenBuffers(1, &vbo);
	if (vbo == 0) {
		throw std::runtime_error{ "Failed to create VBO" };
	}

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * n, vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return vbo;
}


/*!
 * @brief Create index buffer object.
 * @param [in] triangles  Index array.
 * @param [in] n  Number of elements.
 * @return Index buffer object.
 */
static GLint createIbo(const GLushort *triangles, size_t n)
{
	GLuint ibo;
	glGenBuffers(1, &ibo);
	if (ibo == 0) {
		throw std::runtime_error{ "Failed to create IBO" };
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(triangles[0]) * n, triangles, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	return ibo;
}
