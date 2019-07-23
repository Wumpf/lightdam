#include "Shader.h"
#include "../ErrorHandling.h"
#include "../StringConversion.h"
#include <Windows.h>
#include "../../external/dxcapi.use.h"

#include <fstream>
#include <sstream>
#include <vector>

// todo: better logging?
#include <iostream>

#include <wrl/client.h>
using namespace Microsoft::WRL;

static dxc::DxcDllSupport dxcDllHelper;
static ComPtr<IDxcCompiler> dxcCompiler;
static ComPtr<IDxcLibrary> dxcLibrary;
static ComPtr<IDxcIncludeHandler> dxcIncludeHandler;

static const wchar_t* profileStrings[] =
{
    L"lib_6_3", // Library,
    L"vs_6_3", // Vertex,
    L"ps_6_3", // Pixel,
    L"cs_6_3", // Compute,
};

static void LoadCompiler()
{
    if (dxcCompiler)
        return;

    ThrowIfFailed(dxcDllHelper.Initialize());
    ThrowIfFailed(dxcDllHelper.CreateInstance(CLSID_DxcCompiler, dxcCompiler.GetAddressOf()));
    ThrowIfFailed(dxcDllHelper.CreateInstance(CLSID_DxcLibrary, dxcLibrary.GetAddressOf()));
    ThrowIfFailed(dxcLibrary->CreateIncludeHandler(&dxcIncludeHandler));
}

Shader::Shader()
{
}

Shader::~Shader()
{
    if (m_shaderBlob)
        m_shaderBlob->Release();
}

Shader Shader::CompileFromFile(Type type, const wchar_t* filename, const wchar_t* entryPointFunction)
{
    LoadCompiler();

    // Load file
    std::ifstream shaderFile(filename);
    if (shaderFile.good() == false)
    {
        throw std::runtime_error("Cannot open shader file \"" + Utf16toUtf8(filename) + "\"");
    }
    std::stringstream strStream;
    strStream << shaderFile.rdbuf();
    std::string sShader = strStream.str();

    // Create blob from the string
    ComPtr<IDxcBlobEncoding> textBlob;
    ThrowIfFailed(dxcLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)sShader.c_str(), (uint32_t)sShader.size(), 0, &textBlob));

    // Compile
    // Enabling 16bit types sounds meaningful for various operations, but creation of raytracing pipeline state object failed using shaders using it.
    // Reason unclear, maybe driver issues. So we don't enable it here so no one gets too tempted :)
    //static const wchar_t* arguments[] = { L"-enable-16bit-types" };
    ComPtr<IDxcOperationResult> result;
    ThrowIfFailed(dxcCompiler->Compile(textBlob.Get(), filename,
        type == Type::Library ? L"" : entryPointFunction, profileStrings[(int)type],
        nullptr, 0, //arguments, _countof(arguments),
        nullptr, 0,
        dxcIncludeHandler.Get(), &result));

    // Verify the result
    HRESULT resultCode;
    ThrowIfFailed(result->GetStatus(&resultCode));
    if (FAILED(resultCode))
    {
        IDxcBlobEncoding* pError;
        HRESULT hr = result->GetErrorBuffer(&pError);
        if (FAILED(hr))
        {
            throw std::runtime_error("Failed to get shader compiler error for \"" + Utf16toUtf8(filename) + "\"");
        }

        // Convert error blob to a string
        std::vector<char> infoLog(pError->GetBufferSize() + 1);
        memcpy(infoLog.data(), pError->GetBufferPointer(), pError->GetBufferSize());
        infoLog[pError->GetBufferSize()] = 0;

        std::string errorMsg = "Shader Compiler Error for \"" + Utf16toUtf8(filename) + "\":\n" + infoLog.data();
        throw std::runtime_error(errorMsg);
    }

    Shader shader;
    shader.m_type = type;
    ThrowIfFailed(result->GetResult(&shader.m_shaderBlob));
    return shader;
}

bool Shader::ReplaceShaderOnSuccessfulCompileFromFile(Type type, const wchar_t* filename, const wchar_t* entryPointFunction, Shader& shaderToReplace, bool throwOnFailure)
{
    Shader newShader;
    try
    {
        newShader = Shader::CompileFromFile(type, filename, entryPointFunction);
    }
    catch (const std::runtime_error& exception)
    {
        if (throwOnFailure) throw;
        std::cout << exception.what() << std::endl;
        return false;
    }
    shaderToReplace = std::move(newShader);
    return true;
}

D3D12_SHADER_BYTECODE Shader::GetByteCode() const
{
    return D3D12_SHADER_BYTECODE
    {
        m_shaderBlob->GetBufferPointer(),
        m_shaderBlob->GetBufferSize()
    };
}
