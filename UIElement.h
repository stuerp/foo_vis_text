
/** $VER: UIElement.h (2024.06.12) P. Stuer **/

#pragma once

#include "framework.h"

#include "Resources.h"
#include "FileWatcher.h"
#include "Configuration.h"

#include <SDK/cfg_var.h>
#include <SDK/coreDarkMode.h>
#include <SDK/playback_control.h>
#include <SDK/play_callback.h>
#include <SDK/playlist.h>
#include <SDK/ui_element.h>

#include <pfc/string_conv.h>
#include <pfc/string-conv-lite.h>

#include <wrl.h>
#include <wil/com.h>

#include <WebView2.h>

#include "HostObjectImpl.h"

using namespace Microsoft::WRL;

/// <summary>
/// Implements the UIElement and Playback interface.
/// </summary>
class UIElement : public CWindowImpl<UIElement>, private play_callback_impl_base, private playlist_callback_single_impl_base
{
public:
    UIElement();

    UIElement(const UIElement &) = delete;
    UIElement & operator=(const UIElement &) = delete;
    UIElement(UIElement &&) = delete;
    UIElement & operator=(UIElement &&) = delete;

    virtual ~UIElement() { }

    #pragma region CWindowImpl

    static CWndClassInfo & GetWndClassInfo();

    static const UINT UM_WEB_VIEW_READY = WM_USER + 100;
    static const UINT UM_ASYNC          = WM_USER + 101;

    #pragma endregion

    #pragma region WebView

    ICoreWebView2Controller * GetWebViewController()
    {
        return _Controller.get();
    }

    ICoreWebView2 * GetWebView()
    {
        return _WebView.get();
    }

    const wchar_t * const _HostName = TEXT(STR_COMPONENT_BASENAME) L".local";

    #pragma endregion

    #pragma region HostObject

    void RunAsync(std::function<void()> callback) noexcept
    {
        auto * Task = new std::function<void()>(std::move(callback));

        PostMessage(UM_ASYNC, reinterpret_cast<WPARAM>(Task), 0);
    }

    void MessageBoxAsync(std::wstring message, std::wstring title)
    {
        RunAsync
        (
            [this, message = std::move(message), title = std::move(title)]
            {
                MessageBox(message.c_str(), title.c_str(), MB_OK);
            }
        );
    }

    #pragma endregion

    #pragma region Configuration

    const configuration_t & GetConfiguration() const noexcept
    {
        return _Configuration;
    }

    void SetConfiguration(const configuration_t & configuration) noexcept
    {
        _Configuration = configuration;

        OnConfigurationChanged();
    }

    #pragma endregion

protected:
    /// <summary>
    /// Retrieves the GUID of the element.
    /// </summary>
    static const GUID & GetGUID() noexcept
    {
        static const GUID guid = GUID_UI_ELEMENT;

        return guid;
    }

    virtual bool IsWebViewVisible() const noexcept = 0;

    virtual void SetWebViewVisibility(bool visible) noexcept;

private:
    #pragma region Playback callback methods

    void on_playback_starting(play_control::t_track_command command, bool paused);
    void on_playback_new_track(metadb_handle_ptr hTrack);
    void on_playback_stop(play_control::t_stop_reason reason);
    void on_playback_seek(double time);
    void on_playback_pause(bool state);
    void on_playback_edited(metadb_handle_ptr hTrack);
    void on_playback_dynamic_info(const file_info & fileInfo);
    void on_playback_dynamic_info_track(const file_info & fileInfo);
    void on_playback_time(double time);
    void on_volume_change(float newValue);

    #pragma endregion

    #pragma region playlist_callback_single

    virtual void on_item_focus_change(t_size fromIndex, t_size toIndex) override;

    #pragma endregion

    #pragma region CWindowImpl

    LRESULT OnCreate(LPCREATESTRUCT cs) noexcept;
    void OnDestroy() noexcept;
    void OnSize(UINT nType, CSize size) noexcept;
    void OnPaint(CDCHandle dc) noexcept;
    LRESULT OnTemplateChanged(UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
    LRESULT OnWebViewReady(UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
    LRESULT OnAsync(UINT msg, WPARAM wParam, LPARAM lParam) noexcept;

    BEGIN_MSG_MAP_EX(UIElement)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_SIZE(OnSize)
        MSG_WM_PAINT(OnPaint)

        MESSAGE_HANDLER_EX(UM_TEMPLATE_CHANGED, OnTemplateChanged)
        MESSAGE_HANDLER_EX(UM_WEB_VIEW_READY, OnWebViewReady)
        MESSAGE_HANDLER_EX(UM_ASYNC, OnAsync)
    END_MSG_MAP()

    #pragma endregion

    void Initialize();

private:
    bool GetWebViewVersion(std::wstring & versionInfo);
    void CreateWebView();
    void DeleteWebView() noexcept;

    HRESULT CreateContextMenu(const wchar_t * itemLabel, const wchar_t * iconName) noexcept;
    HRESULT SetDarkMode(bool enabled) const noexcept;

    void InitializeFileWatcher();
    void InitializeWebView();

    std::wstring GetTemplateFilePath() const noexcept;

    void ShowPreferences() noexcept;

    void OnConfigurationChanged() noexcept;

protected:
    configuration_t _Configuration;

private:
    fb2k::CCoreDarkModeHooks _DarkMode;

    std::wstring _UserDataFolderPath;
    std::wstring _ExpandedTemplateFilePath;

    wil::com_ptr<ICoreWebView2Environment> _Environment;
    wil::com_ptr<ICoreWebView2Controller> _Controller;
    wil::com_ptr<ICoreWebView2> _WebView;
    wil::com_ptr<ICoreWebView2ContextMenuItem> _ContextSubMenu;

    EventRegistrationToken _NavigationStartingToken = {};
    EventRegistrationToken _ContextMenuRequestedToken = {};

    wil::com_ptr<HostObject> _HostObject;

    FileWatcher _FileWatcher;
};
