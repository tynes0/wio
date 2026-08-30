// Internal compiler detail extracted from the owning translation unit.
// This file is included inside that translation unit's anonymous namespace.
        std::string mangleInterfaceTypeName(const Ref<sema::StructType>& type)
        {
            if (!type)
                return {};
            std::string name = Mangler::mangleInterface(type->name, type->scopePath);
            if (!type->genericArguments.empty())
            {
                name += "<";
                for (size_t i = 0; i < type->genericArguments.size(); ++i)
                {
                    name += toCppType(type->genericArguments[i]);
                    if (i + 1 < type->genericArguments.size())
                        name += ", ";
                }
                name += ">";
            }

            return name;
        }

        std::string mangleNamedType(const Ref<sema::StructType>& type)
        {
            if (!type)
                return {};

            return type->isInterface ? mangleInterfaceTypeName(type) : mangleStructTypeName(type);
        }

        Ref<sema::StructType> getStructTypeFromSymbol(const Ref<sema::Symbol>& symbol)
        {
            if (!symbol || symbol->kind != sema::SymbolKind::Struct || !symbol->type || symbol->type->kind() != sema::TypeKind::Struct)
                return nullptr;

            return symbol->type.AsFast<sema::StructType>();
        }

        std::string mangleNamedType(const Ref<sema::Symbol>& symbol)
        {
            return mangleNamedType(getStructTypeFromSymbol(symbol));
        }
    }
