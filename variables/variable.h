#pragma once

#include <string>

#include "../utils/any.h"
#include "../utils/frenum.h"
#include "../base.h"

namespace wio
{
    // Deðiþkenin yapýsal türünü belirten enum
    enum class variable_base_type
    {
        none = 0, variable, array, dictionary, function
    };

    MakeFrenumInNamespace(wio, variable_base_type, none, variable, array, dictionary, function)

        // Tüm deðiþken türlerinin temel sýnýfý
        class variable_base
    {
    public:
        virtual ~variable_base() = default;
        virtual variable_base_type get_base_type() const = 0;
        virtual std::string to_string() const = 0; // Buraya taþýdýk ve virtual yaptýk
    };

    // Deðiþkenin içerik türünü belirten enum
    enum class variable_type
    {
        vt_none, // nothing
        vt_integer, // long long
        vt_float, // double  // Daha yüksek hassasiyet için double kullanýyoruz
        vt_string, // std::string
        vt_character, // char
        vt_bool, // bool
        vt_array,   // array (eklendi)
        vt_dictionary, // dictionary (eklendi)
        vt_function  // function (eklendi)
    };

    MakeFrenumInNamespace(wio, variable_type, vt_none, vt_integer, vt_float, vt_string, vt_character, vt_bool, vt_array, vt_dictionary, vt_function)

        // Tek bir deðiþkeni temsil eden sýnýf
        class variable : public variable_base
    {
    public:
        variable() : m_type(variable_type::vt_none) {} // Default constructor

        variable(const std::string& id, any data, variable_type type, bool is_constant = false)
            : m_id(id), m_data(data), m_type(type), m_is_constant(is_constant) {
        }

        const std::string& get_id() const { return m_id; }

        any& get_data() { return m_data; }
        const any& get_data() const { return m_data; } // const versiyon

        variable_type get_type() const { return m_type; }

        bool is_constant() const { return m_is_constant; }

        bool has_value() const { return !m_data.empty(); }

        variable_base_type get_base_type() const override { return variable_base_type::variable; }

        std::string to_string() const override; // Implementasyonu aþaðýda

    private:
        std::string m_id;   // Deðiþkenin adý
        any m_data;         // Deðiþkenin deðeri (herhangi bir türde olabilir)
        variable_type m_type;     // Deðiþkenin içerik türü
        bool m_is_constant = false; // Deðiþkenin sabit (constant) olup olmadýðý
    };

    // variable::to_string() implementasyonu
    inline std::string variable::to_string() const
    {
        if (!has_value()) return  frenum::to_string(m_type) + " " + m_id + " = undefined";

        switch (m_type) {
        case variable_type::vt_integer:
            return frenum::to_string(m_type) + " " + m_id + " = " + std::to_string(any_cast<long long>(m_data));
        case variable_type::vt_float:
            return frenum::to_string(m_type) + " " + m_id + " = " + std::to_string(any_cast<double>(m_data));
        case variable_type::vt_string:
            return frenum::to_string(m_type) + " " + m_id + " = " + any_cast<std::string>(m_data);
        case variable_type::vt_character:
            return frenum::to_string(m_type) + " " + m_id + " = " + std::string(1, any_cast<char>(m_data));
        case variable_type::vt_bool:
            return frenum::to_string(m_type) + " " + m_id + " = " + (any_cast<bool>(m_data) ? "true" : "false");
        default:
            return "unknown";
        }
    }
}