#include "Shader.h"
#include "ErrorHandling.h"
#include <Windows.h>
#include "../external/dxcapi.use.h"

#include <fstream>
#include <sstream>
#include <vector>

#include <wrl/client.h>
using namespace Microsoft::WRL;

static dxc::DxcDllSupport dxcDllHelper;
static ComPtr<IDxcCompiler> dxcCompiler;
static ComPtr<IDxcLibrary> dxcLibrary;
static ComPtr<IDxcIncludeHandler> dxcIncludeHandler;

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

Shader Shader::CompileFromFile(const wchar_t* filename)
{
    LoadCompiler();

    // Load file
    std::ifstream shaderFile(filename);
    if (shaderFile.good() == false)
    {
        throw std::logic_error("Cannot open shader file");
    }
    std::stringstream strStream;
    strStream << shaderFile.rdbuf();
    std::string sShader = strStream.str();

    // Create blob from the string
    ComPtr<IDxcBlobEncoding> textBlob;
    ThrowIfFailed(dxcLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)sShader.c_str(), (uint32_t)sShader.size(), 0, &textBlob));

    // Compile
    ComPtr<IDxcOperationResult> result;
    ThrowIfFailed(dxcCompiler->Compile(textBlob.Get(), filename, L"", L"lib_6_3", nullptr, 0, nullptr, 0, dxcIncludeHandler.Get(), &result));

    // Verify the result
    HRESULT resultCode;
    ThrowIfFailed(result->GetStatus(&resultCode));
    if (FAILED(resultCode))
    {
        IDxcBlobEncoding* pError;
        HRESULT hr = result->GetErrorBuffer(&pError);
        if (FAILED(hr))
        {
            throw std::logic_error("Failed to get shader compiler error");
        }

        // Convert error blob to a string
        std::vector<char> infoLog(pError->GetBufferSize() + 1);
        memcpy(infoLog.data(), pError->GetBufferPointer(), pError->GetBufferSize());
        infoLog[pError->GetBufferSize()] = 0;

        std::string errorMsg = "Shader Compiler Error:\n";
        errorMsg.append(infoLog.data());

        MessageBoxA(nullptr, errorMsg.c_str(), "Error!", MB_OK);
        throw std::logic_error("Failed compile shader");
    }

    Shader shader;
    ThrowIfFailed(result->GetResult(&shader.m_shaderBlob));
    return shader;
}

D3D12_SHADER_BYTECODE Shader::GetByteCode() const
{
    return D3D12_SHADER_BYTECODE
    {
        m_shaderBlob->GetBufferPointer(),
        m_shaderBlob->GetBufferSize()
    };
}
