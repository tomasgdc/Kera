// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/slang_reflection.h"

#include "kera/utilities/json.h"

#include <algorithm>
#include <cmath>
#include <optional>

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

            const JsonValue& first_binding = bindings->asArray().front();
            return first_binding.isObject() ? &first_binding : nullptr;
        }

        const JsonValue* typeObject(const JsonValue& object)
        {
            const JsonValue* type = objectItem(object, "type");
            return type && type->isObject() ? type : nullptr;
        }

        ESlangReflectionBindingKind bindingKindFromType(const std::string& type_kind)
        {
            if (type_kind == "parameterBlock")
            {
                return ESlangReflectionBindingKind::PARAMETER_BLOCK;
            }
            if (type_kind == "constantBuffer")
            {
                return ESlangReflectionBindingKind::CONSTANT_BUFFER;
            }
            if (type_kind == "resource")
            {
                return ESlangReflectionBindingKind::RESOURCE;
            }
            if (type_kind == "samplerState")
            {
                return ESlangReflectionBindingKind::SAMPLER_STATE;
            }
            return ESlangReflectionBindingKind::UNKNOWN;
        }

        EShaderType shaderTypeFromStage(const std::string& stage)
        {
            if (stage == "fragment")
            {
                return EShaderType::FRAGMENT;
            }
            if (stage == "compute")
            {
                return EShaderType::COMPUTE;
            }
            if (stage == "geometry")
            {
                return EShaderType::GEOMETRY;
            }
            return EShaderType::VERTEX;
        }

        std::string reflectedTypeName(const JsonValue& type)
        {
            const std::string name = stringItem(type, "name");
            if (!name.empty())
            {
                return name;
            }

            const JsonValue* element_type = objectItem(type, "elementType");
            if (element_type && element_type->isObject())
            {
                return reflectedTypeName(*element_type);
            }

            return stringItem(type, "kind");
        }

        std::size_t reflectedUniformSize(const JsonValue& type)
        {
            const JsonValue* element_var_layout = objectItem(type, "elementVarLayout");
            if (element_var_layout && element_var_layout->isObject())
            {
                const JsonValue* binding = bindingObject(*element_var_layout);
                if (binding && binding->isObject())
                {
                    return sizeItem(*binding, "size");
                }
            }

            return 0;
        }

        bool isFloat32Scalar(const JsonValue& type)
        {
            return stringItem(type, "kind") == "scalar" && stringItem(type, "scalarType") == "float32";
        }

        bool isFloat32ElementType(const JsonValue& type)
        {
            const JsonValue* element_type = objectItem(type, "elementType");
            return element_type && element_type->isObject() && isFloat32Scalar(*element_type);
        }

        std::optional<EVertexFormat> reflectedVertexFormat(const JsonValue& type, uint32_t& out_location_count)
        {
            const std::string kind = stringItem(type, "kind");
            out_location_count = 1;

            if (kind == "vector" && isFloat32ElementType(type))
            {
                switch (numberItem(type, "elementCount"))
                {
                    case 2:
                        return EVertexFormat::FLOAT2;
                    case 3:
                        return EVertexFormat::FLOAT3;
                    case 4:
                        return EVertexFormat::FLOAT4;
                    default:
                        return std::nullopt;
                }
            }

            if (kind == "matrix" && isFloat32ElementType(type))
            {
                const uint32_t rows = numberItem(type, "rowCount");
                const uint32_t columns = numberItem(type, "columnCount");
                if (rows == 4 && columns == 4)
                {
                    out_location_count = 4;
                    return EVertexFormat::FLOAT4;
                }
            }

            return std::nullopt;
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
            binding.type_name = reflectedTypeName(*type);
            binding.uniform_size = reflectedUniformSize(*type);

            const JsonValue* direct_binding = bindingObject(parameter);
            bool direct_descriptor_binding = false;
            if (direct_binding && direct_binding->isObject())
            {
                binding.binding = numberItem(*direct_binding, "index");
                binding.count = numberItem(*direct_binding, "count", 1);
                direct_descriptor_binding = stringItem(*direct_binding, "kind") == "descriptorTableSlot";
            }

            const JsonValue* container_layout = objectItem(*type, "containerVarLayout");
            if (!direct_descriptor_binding && container_layout && container_layout->isObject())
            {
                const JsonValue* descriptor_binding = firstDescriptorBinding(*container_layout);
                if (descriptor_binding)
                {
                    binding.binding = numberItem(*descriptor_binding, "index", binding.binding);
                    binding.space = numberItem(*descriptor_binding, "space", binding.space);
                    binding.count = numberItem(*descriptor_binding, "count", binding.count);
                }
            }

            if (!binding.name.empty() && binding.kind != ESlangReflectionBindingKind::UNKNOWN)
            {
                metadata.bindings.push_back(std::move(binding));
            }
        }

        void parseInputFields(const JsonValue& parameter, SlangReflectionEntryPoint& entry_point)
        {
            const JsonValue* type = typeObject(parameter);
            if (!type)
            {
                return;
            }

            const std::string parameter_name = stringItem(parameter, "name");
            const JsonValue* parameter_binding = bindingObject(parameter);
            const uint32_t base_location =
                parameter_binding && parameter_binding->isObject() ? numberItem(*parameter_binding, "index") : 0;

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
                input.parameter_name = parameter_name;
                input.field_name = stringItem(field, "name");
                input.name = input.field_name;
                input.semantic_name = stringItem(field, "semanticName");
                input.location = base_location;

                const JsonValue* field_type = typeObject(field);
                uint32_t reflected_location_count = 1;
                if (field_type)
                {
                    if (const std::optional<EVertexFormat> format =
                            reflectedVertexFormat(*field_type, reflected_location_count))
                    {
                        input.format = *format;
                        input.has_format = true;
                        input.location_count = reflected_location_count;
                    }
                }

                const JsonValue* field_binding = bindingObject(field);
                if (field_binding && field_binding->isObject())
                {
                    input.location += numberItem(*field_binding, "index");
                    input.location_count = numberItem(*field_binding, "count", input.location_count);
                }

                if (!input.field_name.empty())
                {
                    entry_point.inputs.push_back(std::move(input));
                }
            }
        }

        void parseEntryPoint(const JsonValue& entry_point_value, SlangReflectionMetadata& metadata)
        {
            SlangReflectionEntryPoint entry_point{};
            entry_point.name = stringItem(entry_point_value, "name");
            entry_point.stage = shaderTypeFromStage(stringItem(entry_point_value, "stage"));

            const JsonValue* parameters = objectItem(entry_point_value, "parameters");
            if (parameters && parameters->isArray())
            {
                for (const JsonValue& parameter : parameters->asArray())
                {
                    if (parameter.isObject())
                    {
                        parseInputFields(parameter, entry_point);
                    }
                }
            }

            if (!entry_point.name.empty())
            {
                metadata.entry_points.push_back(std::move(entry_point));
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
            std::find_if(entry_points.begin(), entry_points.end(),
                         [&name](const SlangReflectionEntryPoint& entry_point) { return entry_point.name == name; });
        return found != entry_points.end() ? &(*found) : nullptr;
    }

    bool parseSlangReflectionMetadata(const std::string& reflection_json, SlangReflectionMetadata& out_metadata,
                                      std::string* out_diagnostics)
    {
        out_metadata = {};
        if (out_diagnostics)
        {
            out_diagnostics->clear();
        }

        JsonValue root;
        if (!Json::parse(reflection_json, root) || !root.isObject())
        {
            if (out_diagnostics)
            {
                *out_diagnostics = "Failed to parse Slang reflection JSON.";
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
                    parseBinding(parameter, out_metadata);
                }
            }
        }

        const JsonValue* entry_points = objectItem(root, "entryPoints");
        if (entry_points && entry_points->isArray())
        {
            for (const JsonValue& entry_point : entry_points->asArray())
            {
                if (entry_point.isObject())
                {
                    parseEntryPoint(entry_point, out_metadata);
                }
            }
        }

        return true;
    }

}  // namespace kera
