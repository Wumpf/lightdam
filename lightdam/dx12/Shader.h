#pragma once

#include <memory>
#include <d3d12.h>
#include <vector>
#include "../../external/dxc/dxcapi.h"

struct IDxcBlob;

class Shader
{
public:
    Shader();
    ~Shader();

    using PreprocessorDefineList = std::vector<DxcDefine>;

    enum class Type
    {
        Library,
        Vertex,
        Pixel,
        Compute,
    };

    Shader(Shader&& temp) : m_type(temp.m_type), m_shaderBlob(temp.m_shaderBlob) { temp.m_shaderBlob = nullptr; }
    void operator = (Shader&& temp) { this->~Shader(); m_shaderBlob = temp.m_shaderBlob; temp.m_shaderBlob = nullptr; }
    Shader(const Shader&) = delete;
    void operator = (const Shader&) = delete;

    // Compiles a shader from file using DXC.
    static Shader CompileFromFile(Type type, const wchar_t* filename, const wchar_t* entryPointFunction, const PreprocessorDefineList& preprocessorDefines);

    struct LoadInstruction
    {
        Type type;
        const wchar_t* filename;
        const wchar_t* entryPointFunction;
        PreprocessorDefineList preprocessorDefines;
    };
    // Replaces a shader if loading was successful.
    static bool ReplaceShaderOnSuccessfulCompileFromFile(Type type, const wchar_t* filename, const wchar_t* entryPointFunction, const PreprocessorDefineList& preprocessorDefines, Shader& shaderToReplace, bool throwOnFailure);
    // Replaces a list of shaders if loading all of them was successful.
    template<int N>
    static bool ReplaceShadersOnSuccessfulCompileFromFiles(const LoadInstruction(&shaderLoads)[N], Shader* (&shaderToReplace)[N], bool throwOnFailure)
    {
        Shader loadedShaders[N];
        for (int i = 0; i < N; ++i)
            if (!ReplaceShaderOnSuccessfulCompileFromFile(shaderLoads[i].type, shaderLoads[i].filename, shaderLoads[i].entryPointFunction, shaderLoads[i].preprocessorDefines, loadedShaders[i], throwOnFailure))
                return false;
        for (int i = 0; i < N; ++i)
            *shaderToReplace[i] = std::move(loadedShaders[i]);
        return true;
    }

    IDxcBlob* GetBlob() const { return m_shaderBlob; }
    D3D12_SHADER_BYTECODE GetByteCode() const;

private:
    Type m_type;
    IDxcBlob* m_shaderBlob = nullptr;
};
