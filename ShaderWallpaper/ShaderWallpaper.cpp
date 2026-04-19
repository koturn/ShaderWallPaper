// ShaderWallpaper.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <chrono>
#include <thread>
#include <exception>
#include <fstream>
#include <filesystem>
#include <iostream>
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
static BOOL CALLBACK MyMonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData);
static GLint buildProgram(const char* fsFilePath);
static std::string readAllText(const char *filePath);
static GLuint createShaderObjectFromFile(const char *filePath, GLenum shaderType);
static GLuint createShaderObjectFromText(const std::string& shaderSource, GLenum shaderType);
static GLuint createShaderObjectFromText(const char *shaderSource, size_t sourceSize, GLenum shaderType);
static GLint linkShaders(GLuint vsObj, GLuint fsObj);
static GLint createVbo(const GLfloat *vertices, size_t n);
static GLint createIbo(const GLushort *triangles, size_t n);

namespace fs = std::filesystem;


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

		std::vector<RECT> rects;
		// RECT rect;
		// if (!GetWindowRect(hWorkerW, &rect)) {
		// 	std::cerr << "GetWindowRect failed" << std::endl;
		// }
		// rects.emplace_back(rect);
		if (!EnumDisplayMonitors(NULL, NULL, +[](HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) -> BOOL {
				MONITORINFOEX monitorInfo;
				monitorInfo.cbSize = sizeof(monitorInfo);
				GetMonitorInfo(hMonitor, &monitorInfo);
				((std::vector<RECT>*)dwData)->emplace_back(monitorInfo.rcMonitor);
				return TRUE;
			}, (LPARAM)&rects)) {
			std::cerr << "EnumDisplayMonitors() failed" << std::endl;
			return 2;
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
		int format = ChoosePixelFormat(hDC, &pfd);
		if (format == 0) {
			std::cerr << "ChoosePixelFormat() failed" << std::endl;
			return 2;
		}

		if (!SetPixelFormat(hDC, format, &pfd)) {
			std::cerr << "SetPixelFormat() failed" << std::endl;
			return 3;
		}

		auto hGlrc = wglCreateContext(hDC);
		if (hGlrc == nullptr) {
			std::cerr << "wglCreateContext() failed" << std::endl;
			std::printf("LastError = 0x%08x\n", GetLastError());
			return 2;
		}
		auto f2 = makeFinally([&] {
			wglDeleteContext(hGlrc);
			});
		// make it the calling thread's current rendering context 
		if (!wglMakeCurrent(hDC, hGlrc)) {
			std::cerr << "wglMakeCurrent() failed" << std::endl;
			std::printf("LastError = 0x%08x\n", GetLastError());
			return 3;
		}
		auto f3 = makeFinally([&] {
			wglMakeCurrent(nullptr, nullptr);
		});

		const char *targetDir = argc > 1 ? argv[1] : ".";
		std::vector<std::pair<std::string, GLint>> pathProgPairs;
		for (const fs::directory_entry& entry : fs::recursive_directory_iterator(targetDir)) {
			const auto& ext = entry.path().extension();
			if (ext == ".glsl") {
				pathProgPairs.emplace_back(std::make_pair(entry.path().string(), 0));
				std::cout << entry.path() << std::endl;
			}
		}

		if (glewInit() != GLEW_OK) {
			std::cerr << "glewInit() failed" << std::endl;
			return 4;
		}

		// call OpenGL APIs as desired ... 
		auto program = buildProgram("tes.glsl");  // TODO
		auto f4 = makeFinally([=] {
			glDeleteProgram(program);
		});

		std::cout << "Compile success" << std::endl;

		auto uTime = glGetUniformLocation(program, "u_time");
		auto uMouse = glGetUniformLocation(program, "u_mouse");
		auto uResolution = glGetUniformLocation(program, "u_resolution");
		auto uFrameCount = glGetUniformLocation(program, "u_frameCount");
		auto uPositionOffset = glGetUniformLocation(program, "u_positionOffset");

		// Bind VBO.
		auto attribLocation = glGetAttribLocation(program, "position");
		glBindBuffer(GL_ARRAY_BUFFER, createVbo(kVertices, sizeof(kVertices) / sizeof(kVertices[0])));
		glEnableVertexAttribArray(attribLocation);
		glVertexAttribPointer(attribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
		// Bind IBO.
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, createIbo(kTriangles, sizeof(kTriangles) / sizeof(kTriangles[0])));

		glUseProgram(program);

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

		auto isContinue = true;
		std::thread th{ [&] {
			std::string s;
			std::cin >> s;
			isContinue = false;
			std::cout << "FINISH" << std::endl;
		} };

		int frameCount = 0;
		const auto start = std::chrono::high_resolution_clock::now();
		while (isContinue) {
			glClearColor(0.0f, 0.0f, 0.3, 0.0f);
			glClearDepth(1.0f);
			glClearStencil(0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

			const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
			for (const auto& r : rects) {
				auto width = std::abs(r.right - r.left);
				auto height = std::abs(r.bottom - r.top);
				glViewport(r.left, r.top, width, height);
				// glClear(GL_COLOR_BUFFER_BIT);

				glUniform1f(uTime, static_cast<float>(static_cast<double>(elapsed) / 1000.0L));
				glUniform2f(uMouse, 0.0f, 0.0f);
				glUniform2f(uResolution, static_cast<float>(width), static_cast<float>(height));
				glUniform1f(uFrameCount, static_cast<float>(frameCount));
				glUniform2f(uPositionOffset, static_cast<float>(r.left), static_cast<float>(r.top));

				glDrawElements(GL_TRIANGLES, sizeof(kTriangles) / sizeof(kTriangles[0]), GL_UNSIGNED_SHORT, nullptr);
			}

			glFlush();
			SwapBuffers(hDC);

			frameCount++;

			std::this_thread::sleep_for(std::chrono::milliseconds{15});
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


static BOOL CALLBACK MyMonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    MONITORINFOEX monitorInfo;
 
    monitorInfo.cbSize = sizeof( monitorInfo );
    GetMonitorInfo( hMonitor, &monitorInfo );

	std::cout << "top = " << monitorInfo.rcMonitor.top
		<< ", left = " << monitorInfo.rcMonitor.left
		<< ", right = " << monitorInfo.rcMonitor.right
		<< ", bottom = " << monitorInfo.rcMonitor.bottom
		<< std::endl;

	// std::cout << "top = " << monitorInfo.rcWork.top
	// 	<< ", left = " << monitorInfo.rcWork.left
	// 	<< ", right = " << monitorInfo.rcWork.right
	// 	<< ", bottom = " << monitorInfo.rcWork.bottom
	// 	<< std::endl;

	((std::vector<RECT>*)dwData)->emplace_back(monitorInfo.rcMonitor);

    return TRUE;
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
    throw std::runtime_error{oss.str()};
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
    throw std::runtime_error{infoLog};
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
