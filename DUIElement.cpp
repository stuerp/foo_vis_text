
/** $VER: DUIElement.cpp (2024.06.12) P. Stuer **/

#include "pch.h"

#include "DUIElement.h"

#pragma hdrstop

#pragma region ui_element_instance interface

/// <summary>
/// Initializes a new instance.
/// </summary>
DUIElement::DUIElement(ui_element_config::ptr data, ui_element_instance_callback::ptr callback) : m_callback(callback)
{
    set_configuration(data);
}

/// <summary>
/// Retrieves the name of the element.
/// </summary>
void DUIElement::g_get_name(pfc::string_base & name)
{
    name = STR_COMPONENT_NAME;
}

/// <summary>
/// Retrieves the description of the element.
/// </summary>
const char * DUIElement::g_get_description()
{
    return "Displays HTML-formatted content";
}

/// <summary>
/// Retrieves the GUID of the element.
/// </summary>
GUID DUIElement::g_get_guid()
{
    return UIElement::GetGUID();
}

/// <summary>
/// Retrieves the subclass GUID of the element.
/// </summary>
GUID DUIElement::g_get_subclass()
{
    return ui_element_subclass_playback_visualisation;
}

/// <summary>
/// Retrieves the default configuration of the element.
/// </summary>
ui_element_config::ptr DUIElement::g_get_default_configuration()
{
    configuration_t DefaultConfiguration;

    ui_element_config_builder Builder;

    DefaultConfiguration.Write(&Builder.m_stream);

    return Builder.finish(g_get_guid());
}

/// <summary>
/// Initializes the element's windows.
/// </summary>
void DUIElement::initialize_window(HWND p_parent)
{
    const DWORD Style = 0;
    const DWORD ExStyle = 0;

    this->Create(p_parent, nullptr, nullptr, Style, ExStyle);
}

/// <summary>
/// Alters element's current configuration. Specified ui_element_config's GUID must be the same as this element's GUID.
/// </summary>
void DUIElement::set_configuration(ui_element_config::ptr data)
{
    ui_element_config_parser Parser(data);

    _Configuration.Read(&Parser.m_stream, Parser.get_remaining());
}

/// <summary>
/// Retrieves element's current configuration. Returned object's GUID must be set to your element's GUID so your element can be re-instantiated with stored settings.
/// </summary>
ui_element_config::ptr DUIElement::get_configuration()
{
    ui_element_config_builder Builder;

    _Configuration.Write(&Builder.m_stream);

    return Builder.finish(g_get_guid());
}

/// <summary>
/// Used by the host to notify the element about various events.
/// See ui_element_notify_* GUIDs for possible "what" parameter; meaning of other parameters depends on the "what" value.
/// Container classes should dispatch all notifications to their children.
/// </summary>
void DUIElement::notify(const GUID & what, t_size param1, const void * param2, t_size param2Size)
{
    if (what == ui_element_notify_edit_mode_changed)
    {
        SetWebViewVisibility(IsWebViewVisible());
    }
/*
    if (what == ui_element_notify_colors_changed)
    {
    }
    else
    if (what == ui_element_notify_font_changed)
    {
    }
    else
    if (what == ui_element_notify_visibility_changed)
    {
    }
*/
}

static service_factory_single_t<ui_element_impl_visualisation<DUIElement>> _Factory;

#pragma endregion
