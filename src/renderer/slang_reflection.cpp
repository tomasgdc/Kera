#include "kera/renderer/slang_reflection.h"

#include "kera/utilities/json.h"

#include <algorithm>
#include <cmath>

namespace kera
{
    namespace
    {
        const JsonValue* objectItem(const JsonValue& object, const char* key)
        {
            return object.getObjectItem(key);
        }

        std::string stringItem(const JsonValue& object, const char* key)
        {
            const JsonValue* value = objectItem(object, key);
            return value && value->isString() ? value->asString() : std::string{};
        }

        uint32_t numberItem(const JsonValue& object, const char* key, uint32_t fallback = 0)
        {
            const JsonValue* value = objectItem(object, key);
            if (!value || !value->isNumber())
            {
                return fallback;
            }

            return static_cast<uint32_t>(std::max(0.0, std::floor(value->asNumber())));
        }

        std::size_t sizeItem(const JsonValue& object, const char* key, std::size_t fallback = 0)
        {
            const JsonValue* value = objectItem(object, key);
            if (!value || !value->isNumber())
            {
                return fallback;
            }

            return static_cast<std::size_t>(std::max(0.0, std::floor(value->asNumber())));
        }

        const JsonValue* bindingObject(const JsonValue& object)
        {
            return objectItem(object, "binding");
        }

        const JsonValue* firstDescriptorBinding(const JsonValue& object)
        {
            const JsonValue* binding = bindingObject(object);
            if (binding && binding->isObject())
            {
                return binding;
            }

            const JsonValue* bindings = objectItem(object, "bindings");
            if (!bindings || !bindings->isArray() || bindings->asArray().empty())
            {
                return nullptr;
            }

            const JsonValue& firstBinding = bindings->asArray().front();
            return firstBinding.isObject() ? &firstBinding : nullptr;
        }

        const JsonValue* typeObject(const JsonValue& object)
        {
            const JsonValue* type = objectItem(object, "type");
            return type && type->isObject() ? type : nullptr;
        }

        SlangReflectionBindingKind bindingKindFromType(const std::string& typeKind)
        {
            if (typeKind == "parameterBlock")
            {
                return SlangReflectionBindingKind::ParameterBlock;
            }
            if (typeKind == "constantBuffer")
            {
                return SlangReflectionBindingKind::ConstantBuffer;
            }
            if (typeKind == "resource")
            {
                return SlangReflectionBindingKind::Resource;
            }
            if (typeKind == "samplerState")
            {
                return SlangReflectionBindingKind::SamplerState;
            }
            return SlangReflectionBindingKind::Unknown;
        }

        ShaderType shaderTypeFromStage(const std::string& stage)
        {
            if (stage == "fragment")
            {
                return ShaderType::Fragment;
            }
            if (stage == "compute")
            {
                return ShaderType::Compute;
            }
            if (stage == "geometry")
            {
                return ShaderType::Geometry;
            }
            return ShaderType::Vertex;
        }

        std::string reflectedTypeName(const JsonValue& type)
        {
            const std::string name = stringItem(type, "name");
            if (!name.empty())
            {
                return name;
            }

            const JsonValue* elementType = objectItem(type, "elementType");
            if (elementType && elementType->isObject())
            {
                return reflectedTypeName(*elementType);
            }

            return stringItem(type, "kind");
        }

        std::size_t reflectedUniformSize(const JsonValue& type)
        {
            const JsonValue* elementVarLayout = objectItem(type, "elementVarLayout");
            if (elementVarLayout && elementVarLayout->isObject())
            {
                const JsonValue* binding = bindingObject(*elementVarLayout);
                if (binding && binding->isObject())
                {
                    return sizeItem(*binding, "size");
                }
            }

            return 0;
        }

        void parseBinding(const JsonValue& parameter, SlangReflectionMetadata& metadata)
        {
            const JsonValue* type = typeObject(parameter);
            if (!type)
            {
                return;
            }

            SlangReflectionBinding binding{};
            binding.name = stringItem(parameter, "name");
            binding.kind = bindingKindFromType(stringItem(*type, "kind"));
            binding.typeName = reflectedTypeName(*type);
            binding.uniformSize = reflectedUniformSize(*type);

            const JsonValue* directBinding = bindingObject(parameter);
            bool directDescriptorBinding = false;
            if (directBinding && directBinding->isObject())
            {
                binding.binding = numberItem(*directBinding, "index");
                binding.count = numberItem(*directBinding, "count", 1);
                directDescriptorBinding = stringItem(*directBinding, "kind") == "descriptorTableSlot";
            }

            const JsonValue* containerLayout = objectItem(*type, "containerVarLayout");
            if (!directDescriptorBinding && containerLayout && containerLayout->isObject())
            {
                const JsonValue* descriptorBinding = firstDescriptorBinding(*containerLayout);
                if (descriptorBinding)
                {
                    binding.binding = numberItem(*descriptorBinding, "index", binding.binding);
                    binding.space = numberItem(*descriptorBinding, "space", binding.space);
                    binding.count = numberItem(*descriptorBinding, "count", binding.count);
                }
            }

            if (!binding.name.empty() && binding.kind != SlangReflectionBindingKind::Unknown)
            {
                metadata.bindings.push_back(std::move(binding));
            }
        }

        void parseInputFields(const JsonValue& parameter, SlangReflectionEntryPoint& entryPoint)
        {
            const JsonValue* type = typeObject(parameter);
            if (!type)
            {
                return;
            }

            const JsonValue* parameterBinding = bindingObject(parameter);
            const uint32_t baseLocation =
                parameterBinding && parameterBinding->isObject() ? numberItem(*parameterBinding, "index") : 0;

            const JsonValue* fields = objectItem(*type, "fields");
            if (!fields || !fields->isArray())
            {
                return;
            }

            for (const JsonValue& field : fields->asArray())
            {
                if (!field.isObject())
                {
                    continue;
                }

                SlangReflectionInput input{};
                input.name = stringItem(field, "name");
                input.semanticName = stringItem(field, "semanticName");
                input.location = baseLocation;

                const JsonValue* fieldBinding = bindingObject(field);
                if (fieldBinding && fieldBinding->isObject())
                {
                    input.location += numberItem(*fieldBinding, "index");
                    input.locationCount = numberItem(*fieldBinding, "count", 1);
                }

                if (!input.name.empty())
                {
                    entryPoint.inputs.push_back(std::move(input));
                }
            }
        }

        void parseEntryPoint(const JsonValue& entryPointValue, SlangReflectionMetadata& metadata)
        {
            SlangReflectionEntryPoint entryPoint{};
            entryPoint.name = stringItem(entryPointValue, "name");
            entryPoint.stage = shaderTypeFromStage(stringItem(entryPointValue, "stage"));

            const JsonValue* parameters = objectItem(entryPointValue, "parameters");
            if (parameters && parameters->isArray())
            {
                for (const JsonValue& parameter : parameters->asArray())
                {
                    if (parameter.isObject())
                    {
                        parseInputFields(parameter, entryPoint);
                    }
                }
            }

            if (!entryPoint.name.empty())
            {
                metadata.entryPoints.push_back(std::move(entryPoint));
            }
        }
    }  // namespace

    const SlangReflectionBinding* SlangReflectionMetadata::findBinding(const std::string& name) const
    {
        const auto found = std::find_if(bindings.begin(), bindings.end(), [&name](const SlangReflectionBinding& binding)
                                        { return binding.name == name; });
        return found != bindings.end() ? &(*found) : nullptr;
    }

    const SlangReflectionEntryPoint* SlangReflectionMetadata::findEntryPoint(const std::string& name) const
    {
        const auto found =
            std::find_if(entryPoints.begin(), entryPoints.end(),
                         [&name](const SlangReflectionEntryPoint& entryPoint) { return entryPoint.name == name; });
        return found != entryPoints.end() ? &(*found) : nullptr;
    }

    bool parseSlangReflectionMetadata(const std::string& reflectionJson, SlangReflectionMetadata& outMetadata,
                                      std::string* outDiagnostics)
    {
        outMetadata = {};
        if (outDiagnostics)
        {
            outDiagnostics->clear();
        }

        JsonValue root;
        if (!Json::parse(reflectionJson, root) || !root.isObject())
        {
            if (outDiagnostics)
            {
                *outDiagnostics = "Failed to parse Slang reflection JSON.";
            }
            return false;
        }

        const JsonValue* parameters = objectItem(root, "parameters");
        if (parameters && parameters->isArray())
        {
            for (const JsonValue& parameter : parameters->asArray())
            {
                if (parameter.isObject())
                {
                    parseBinding(parameter, outMetadata);
                }
            }
        }

        const JsonValue* entryPoints = objectItem(root, "entryPoints");
        if (entryPoints && entryPoints->isArray())
        {
            for (const JsonValue& entryPoint : entryPoints->asArray())
            {
                if (entryPoint.isObject())
                {
                    parseEntryPoint(entryPoint, outMetadata);
                }
            }
        }

        return true;
    }

}  // namespace kera
